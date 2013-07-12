/*
 * Process.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "Process.h"
#include "Scheduler.h"
#include "common.h"
#include "virt_memory.h"
#include "phys_memory.h"

#include <fcntl.h>
#include <string.h>

namespace VirtMem
{
	extern FixedSizeAllocator<TranslationTable::SmallPageActual, 1048576 / sizeof(TranslationTable::SmallPageActual)> g_sysThreadStacks;
	extern FixedSizeAllocator<TranslationTable::L1Table, 1048576 / sizeof(TranslationTable::L1Table)> g_masterTables;
}

Process::Process(const char *pRootFilename, const char *pInitialDirectory,
		const char *pFilename, BaseFS &rVfs, File &rLoader)
: m_pMainThread(0),
  m_pfs(pRootFilename, pInitialDirectory),
  m_rVfs(rVfs),
  m_pExeFile(0),
  m_rLoader(rLoader),
  m_pEnvironment(0)
{
	const int length = 500;
	char string[length];

	//build the file name
	if (!m_pfs.BuildFullPath(pFilename, string, length))
	{
		m_state = kDead;
		return;
	}

	//open the file handle
	BaseDirent *pTemp = m_rVfs.OpenByName(string, O_RDONLY);
	if (!pTemp || pTemp->IsDirectory())
	{
		m_state = kDead;
		return;
	}
	m_pExeFile = (File *)pTemp;

	//get their sizes
	stat64 exe_stat, ld_stat;
	m_pExeFile->Fstat(exe_stat);
	rLoader.Fstat(ld_stat);

	//allocate some memory for loading (in the kernel)
	unsigned char *pProgramData, *pInterpData;
	pProgramData = new unsigned char[exe_stat.st_size];
	if (!pProgramData)
	{
		m_rVfs.Close(*pTemp);

		m_state = kDead;
		return;
	}

	pInterpData = new unsigned char[ld_stat.st_size];
	if (!pInterpData)
	{
		delete[] pProgramData;

		m_rVfs.Close(*pTemp);

		m_state = kDead;
		return;
	}

	//allocate the lo page table
	if (!VirtMem::g_masterTables.Allocate(&m_pVirtTable))
	{
		delete[] pProgramData;
		delete[] pInterpData;

		m_rVfs.Close(*pTemp);

		m_state = kDead;
		return;
	}

	if (!VirtMem::VirtToPhys(m_pVirtTable, &m_pPhysTable))
	{
		VirtMem::g_masterTables.Free(m_pVirtTable);

		delete[] pProgramData;
		delete[] pInterpData;

		m_rVfs.Close(*pTemp);

		m_state = kDead;
		return;
	}

	//clear the table
	for (unsigned int count = 0; count < 2048; count++)
		m_pVirtTable->e[count].fault.Init();

	//install the table
	MapProcess();

	//load the files
	m_pExeFile->ReadFrom(pProgramData, exe_stat.st_size, 0);
	rLoader.ReadFrom(pInterpData, ld_stat.st_size, 0);

	//parse the files
	Elf startingElf, interpElf;

	startingElf.Load(pProgramData, exe_stat.st_size);
	interpElf.Load(pInterpData, ld_stat.st_size);

	bool has_tls = false;
	unsigned int tls_memsize, tls_filesize, tls_vaddr;

	//prepare the memory map
	LoadElf(interpElf, 0x70000000, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	ASSERT(has_tls == false);
	char *pInterpName = LoadElf(startingElf, 0, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	SetBrk((void *)0x10000000);
	SetHighZero((void *)0x60000000);

	//allocate a stack
	if (!VirtMem::AllocAndMapVirtContig((void *)(0x80000000 - 4096), 1, false,
			TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
		ASSERT(0);

	//prepare the aux vector
	m_auxVec[0].a_type = AT_PHDR;
	m_auxVec[0].a_un.a_val = 0x8000 + 52;

	m_auxVec[1].a_type = AT_PHNUM;
	m_auxVec[1].a_un.a_val = startingElf.GetNumProgramHeaders();

	m_auxVec[2].a_type = AT_ENTRY;
	m_auxVec[2].a_un.a_val = (unsigned int)startingElf.GetEntryPoint();;

	m_auxVec[3].a_type = AT_BASE;
	m_auxVec[3].a_un.a_val = 0x70000000;

	m_auxVec[4].a_type = AT_PAGESZ;
	m_auxVec[4].a_un.a_val = 4096;

	m_auxVec[5].a_type = AT_RANDOM;
	m_auxVec[5].a_un.a_val = (unsigned int)&m_auxVec[5].a_un.a_val;			//why not

	m_auxVec[sm_auxSize - 1].a_type = AT_NULL;
	m_auxVec[sm_auxSize - 1].a_un.a_val = 0;

	//push arg 0
	AddArgument(pFilename);

	//free the temp loading memory
	delete[] pProgramData;
	delete[] pInterpData;

	m_state = kInitialising;

	//construct the thread
	if (pInterpName)
		m_entryPoint = (unsigned int)interpElf.GetEntryPoint() + 0x70000000;
	else
		m_entryPoint = (unsigned int)startingElf.GetEntryPoint();

	VirtMem::DumpVirtToPhys(0, (void *)0x7fffffff, true, true, false);
}

Process::~Process()
{
	ASSERT(m_state == kStopped);

	//delete all threads
	while (m_threads.size())
	{
		auto p = m_threads.back();
		delete p;
		m_threads.pop_back();
	}

	//free the page tables
	//todo check for shared page tables
	for (unsigned int count = 0; count < 2048; count++)
		VirtMem::RemovePageTable((void *)(count * 1048576), false, false);

	//unmap lo
	VirtMem::SetL1TableLo(0);

	//free lo
	VirtMem::g_masterTables.Free(m_pVirtTable);
}

void Process::MapProcess(void)
{
	VirtMem::SetL1TableLo((TranslationTable::TableEntryL1 *)m_pVirtTable);
}

void* Process::GetBrk(void)
{
	return m_pBrk;
}

void Process::SetBrk(void *p)
{
	m_pBrk = p;
}

void Process::SetDefaultStdio(void)
{
	//open stdio
	ASSERT(m_pfs.Open(*m_rVfs.OpenByName("/Devices/stdin", O_RDONLY)) == 0);
	ASSERT(m_pfs.Open(*m_rVfs.OpenByName("/Devices/stdout", O_RDONLY)) == 1);
	ASSERT(m_pfs.Open(*m_rVfs.OpenByName("/Devices/stderr", O_RDONLY)) == 2);
}

void Process::SetStdio(File& rStdio, File& rStdout, File& rStderr)
{
	ASSERT(m_pfs.Open(rStdio) == 0);
	ASSERT(m_pfs.Open(rStdout) == 1);
	ASSERT(m_pfs.Open(rStderr) == 2);
}

void Process::AddArgument(const char *p)
{
	ASSERT(p);
	char *pBuffer = new char[strlen(p) + 1];
	ASSERT(pBuffer);

	strcpy(pBuffer, p);

	m_arguments.push_back(pBuffer);
}

void Process::SetEnvironment(const char *p)
{
	ASSERT(p);
	char *pBuffer = new char[strlen(p) + 1];
	ASSERT(pBuffer);

	strcpy(pBuffer, p);

	m_pEnvironment = pBuffer;
}

void Process::MakeRunnable(void)
{
	ASSERT(m_state == kInitialising);

	//get number of arguments
	unsigned int numArgs = m_arguments.size();
	ASSERT(numArgs);

	//check there's an environment
	ASSERT(m_pEnvironment);

	//compute word offset from sp of pAuxVec
	unsigned int wordOffset = numArgs + 1	//for zero
								+ 1			//for enviroment
								+ 1			//for another zero
								+ 1;		//for the stack pointer itself

	//size of the aux vector
	const unsigned int auxSize = sizeof(m_auxVec);

	//size of all the text
	unsigned int textSize = strlen(m_pEnvironment) + 1;

	for (auto it = m_arguments.begin(); it != m_arguments.end(); it++)
		textSize += strlen(*it) + 1;

	//compute the stack position, and round it down to be 8b aligned
	unsigned int sp = 0x80000000 - 8;
	sp = sp - textSize - auxSize - wordOffset * sizeof(int);
	sp = sp & ~7;

	char *pSp = (char *)sp;
	pSp += wordOffset * sizeof(int);

	//copy in aux vector
	memcpy(pSp, &m_auxVec, auxSize);
	pSp += auxSize;

	//now fill in some text
	strcpy(pSp, m_pEnvironment);
	char *envPos = pSp;
	pSp += strlen(m_pEnvironment) + 1;

	delete[] m_pEnvironment;
	m_pEnvironment = 0;

	unsigned int slot = 0;
	//number of arguments
	*(unsigned int *)(sp + slot++ * sizeof(int)) = m_arguments.size();

	for (auto it = m_arguments.begin(); it != m_arguments.end(); it++)
	{
		strcpy(pSp, *it);
		*(unsigned int *)(sp + slot++ * sizeof(int)) = (unsigned int)pSp;
		pSp += strlen(*it) + 1;

		delete[] *it;
	}

	//a zero once we're out of args
	*(unsigned int *)(sp + slot++ * sizeof(int)) = 0;

	//env slot
	*(unsigned int *)(sp + slot++ * sizeof(int)) = (unsigned int)envPos;

	//a zero after the env
	*(unsigned int *)(sp + slot++ * sizeof(int)) = 0;

	//construct the thread
	m_pMainThread = new Thread(m_entryPoint,
		this, false, 1, 0, sp);

	ASSERT(m_pMainThread);
	m_threads.push_back(m_pMainThread);

	m_state = kStopped;
}

void Process::Schedule(Scheduler& rSched)
{
	ASSERT(m_state == kStopped);

	for (auto it = m_threads.begin(); it != m_threads.end(); it++)
		rSched.AddThread(**it);

	m_state = kRunnable;
}

void Process::Deschedule(Scheduler& rSched)
{
	ASSERT(m_state == kRunnable);

	for (auto it = m_threads.begin(); it != m_threads.end(); it++)
		rSched.RemoveThread(**it);

	m_state = kStopped;
}

void* Process::GetHighZero(void)
{
	return m_pHighZero;
}

void Process::SetHighZero(void *p)
{
	m_pHighZero = p;
}

char *Process::LoadElf(Elf &elf, unsigned int voffset, bool &has_tls, unsigned int &tls_memsize, unsigned int &tls_filesize, unsigned int &tls_vaddr)
{
	PrinterUart<PL011> p;
	has_tls = false;
	char *pInterp = 0;

	for (unsigned int count = 0; count < elf.GetNumProgramHeaders(); count++)
	{
		void *pData;
		unsigned int vaddr;
		unsigned int memSize, fileSize;

		int p_type = elf.GetProgramHeader(count, &pData, &vaddr, &memSize, &fileSize);

		vaddr += voffset;

		p.PrintString("Header ");
		p.PrintHex(count);
		p.PrintString(" file data ");
		p.PrintHex((unsigned int)pData);
		p.PrintString(" virtual address ");
		p.PrintHex(vaddr);
		p.PrintString(" memory size ");
		p.PrintHex(memSize);
		p.PrintString(" file size ");
		p.PrintHex(fileSize);
		p.PrintString("\n");

		if (p_type == PT_TLS)
		{
			tls_memsize = memSize;
			tls_filesize = fileSize;
			tls_vaddr = vaddr;
			has_tls = true;
		}

		if (p_type == PT_INTERP)
		{
			p.PrintString("interpreter :");
			p.PrintString((char *)pData);
			p.PrintString("\n");
			pInterp = (char *)pData;
		}

		if (p_type == PT_LOAD)
		{
			unsigned int beginVirt = vaddr & ~4095;
			unsigned int endVirt = ((vaddr + memSize) + 4095) & ~4095;
			unsigned int pagesNeeded = (endVirt - beginVirt) >> 12;

			for (unsigned int i = 0; i < pagesNeeded; i++)
			{
				void *pPhys = PhysPages::FindPage();
				ASSERT(pPhys != (void *)-1);

				bool ok = VirtMem::MapPhysToVirt(pPhys, (void *)beginVirt, 4096, false,
						TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
				ASSERT(ok);

				//clear the page
				memset((void *)beginVirt, 0, 4096);

				//next page
				beginVirt += 4096;
			}

			//copy in the file data
			for (unsigned int c = 0; c < fileSize; c++)
			{
				((unsigned char *)vaddr)[c] = ((unsigned char *)pData)[c];
			}
		}
	}

	return pInterp;
}

Thread::Thread(unsigned int entryPoint, Process* pParent, bool priv,
		unsigned int stackSizePages, int priority, unsigned int userStack)
: m_pParent(pParent),
  m_isPriv(priv),
  m_priority(priority)
{
	ASSERT(stackSizePages == 1);

	memset(&m_pausedState, 0, sizeof(m_pausedState));

	m_pausedState.m_newPc = entryPoint;
	m_pausedState.m_spsr.m_t = entryPoint & 1;
	m_pausedState.m_spsr.m_f = 0;
	m_pausedState.m_spsr.m_i = 0;

	m_state = kRunnable;

	if (m_isPriv)
	{
		if (!VirtMem::g_sysThreadStacks.Allocate(&m_pPrivStack))
			ASSERT(0);

		m_pausedState.m_regs[13] = (unsigned int)m_pPrivStack;
		m_pausedState.m_spsr.m_mode = kSystem;
	}
	else
	{
		m_pausedState.m_regs[13] = userStack;
		m_pausedState.m_spsr.m_mode = kUser;
	}

	memset(m_name, 0, sm_nameLength);
}

Thread::~Thread()
{
	if (m_isPriv)
		VirtMem::g_sysThreadStacks.Free(m_pPrivStack);
}

Thread::State Thread::GetState(void)
{
	return m_state;
}

bool Thread::SetState(State target)
{
	switch (m_state)
	{
	case kRunning:
		if (target == kBlocked || target == kRunnable || target == kDead)
		{
			m_state = target;
			return true;
		}
		else
			return false;

	case kDead:				//not going anywhere
		if (target == kDead)
			return true;
		return false;

	case kBlocked:
		if (target == kDead)
		{
			m_state = target;
			return true;
		}
		return false;		//needs an unblock

	case kRunnable:
		if (target == kRunning || target == kDead)
		{
			m_state = target;
			return true;
		}
		else
			return false;

	default:
		ASSERT(0);
		return false;
	}
}

bool Thread::Unblock(void)
{
	if (m_state == kBlocked)
	{
		m_state = kRunnable;
		return true;
	}
	else
		return false;
}

void Thread::HaveSavedState(ExceptionState exceptionState)
{
	m_pausedState = exceptionState;
}

bool Thread::Run(void)
{
	if (m_pParent)
	{
		ASSERT(!m_isPriv);
		m_pParent->MapProcess();
	}
	else
	{
		ASSERT(m_isPriv);
		//unmap the lo table
		VirtMem::SetL1TableLo(0);
	}

	Resume(&m_pausedState);

	return true;
}

bool Thread::RunAsHandler(Thread& rBlocked)
{
	//handler should be priv
	if (!m_isPriv)
		return false;

	if (rBlocked.m_pParent)
		rBlocked.m_pParent->MapProcess();
	else
		VirtMem::SetL1TableLo(0);

	//set r1 to be &rBlocked
	m_pausedState.m_regs[1] = (unsigned int)&rBlocked;
	//interrupts are on

	Resume(&m_pausedState);
	return true;
}

void Thread::SetName(const char *pName)
{
	if (!pName)
		return;

	strncpy(m_name, pName, sm_nameLength - 1);		//for the null char
}
