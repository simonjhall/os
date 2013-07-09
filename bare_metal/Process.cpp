/*
 * Process.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "Process.h"
#include "common.h"
#include "virt_memory.h"
#include "phys_memory.h"#

#include <fcntl.h>
#include <string.h>

namespace VirtMem
{
	extern FixedSizeAllocator<TranslationTable::SmallPageActual, 1048576 / sizeof(TranslationTable::SmallPageActual)> g_sysThreadStacks;
	extern FixedSizeAllocator<TranslationTable::L1Table, 1048576 / sizeof(TranslationTable::L1Table)> g_masterTables;
}

Process::Process(ProcessFS &rPfs, const char *pFilename, BaseFS &rVfs, File &rLoader)
: m_rPfs(rPfs),
  m_rVfs(rVfs),
  m_pExeFile(0),
  m_rLoader(rLoader)
{
	static const int length = 500;
	char string[length];

	//build the file name
	if (!m_rPfs.BuildFullPath(pFilename, string, length))
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

	//open stdio
	ASSERT(m_rPfs.Open(*m_rVfs.OpenByName("/Devices/stdin", O_RDONLY)) == 0);
	ASSERT(m_rPfs.Open(*m_rVfs.OpenByName("/Devices/stdout", O_RDONLY)) == 1);
	ASSERT(m_rPfs.Open(*m_rVfs.OpenByName("/Devices/stderr", O_RDONLY)) == 2);

	m_exeFd = m_rPfs.Open(*m_pExeFile);
	m_ldFd = m_rPfs.Open(rLoader);

	//get their sizes
	stat64 exe_stat, ld_stat;
	m_rPfs.GetFile(m_exeFd)->Fstat(exe_stat);
	m_rPfs.GetFile(m_ldFd)->Fstat(ld_stat);

	//allocate some memory for loading (in the kernel)
	unsigned char *pProgramData, *pInterpData;
	pProgramData = new unsigned char[exe_stat.st_size];
	ASSERT(pProgramData);
	pInterpData = new unsigned char[ld_stat.st_size];
	ASSERT(pInterpData);

	//allocate the lo page table
	if (!VirtMem::g_masterTables.Allocate(&m_pVirtTable))
		ASSERT(0);

	if (!VirtMem::VirtToPhys(m_pVirtTable, &m_pPhysTable))
		ASSERT(0);

	//install the table
	MapProcess();

	//load the files
	m_rPfs.GetFile(m_exeFd)->Read(pProgramData, exe_stat.st_size);
	m_rPfs.GetFile(m_ldFd)->Read(pInterpData, ld_stat.st_size);

	//parse the files
	Elf startingElf, interpElf;

	startingElf.Load(pProgramData, exe_stat.st_size);
	interpElf.Load(pInterpData, ld_stat.st_size);

	bool has_tls = false;
	unsigned int tls_memsize, tls_filesize, tls_vaddr;

	LoadElf(interpElf, 0x70000000, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	ASSERT(has_tls == false);
	char *pInterpName = LoadElf(startingElf, 0, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	SetBrk((void *)0x10000000);

//	m_threads.push_back(*new Thread(entryPoint, this, false, 1, 0));
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
		unsigned int stackSizePages, int priority)
: m_pParent(pParent),
  m_isPriv(priv),
  m_priority(priority)
{
	ASSERT(stackSizePages == 1);

	m_pausedState.m_newPc = entryPoint;
	m_pausedState.m_spsr.m_t = entryPoint & 1;
	m_pausedState.m_spsr.m_f = 0;
	m_pausedState.m_spsr.m_i = 0;

	m_state = kRunnable;

	if (m_isPriv)
	{
		TranslationTable::SmallPageActual *sp;
		if (!VirtMem::g_sysThreadStacks.Allocate(&sp))
			ASSERT(0);

		m_pausedState.m_regs[13] = (unsigned int)sp;
		m_pausedState.m_spsr.m_mode = kSystem;
	}
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
	case kBlocked:
		return false;		//needs an unblock

	case kRunnable:
		if (target == kRunning)
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

void Thread::Unblock(void)
{
	m_state = kRunnable;
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

	//run without interrupts
	m_pausedState.m_spsr.m_i = 1;
	//and set r1 to be &rBlocked
	m_pausedState.m_regs[1] = (unsigned int)&rBlocked;

	Resume(&m_pausedState);
	return true;
}
