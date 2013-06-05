#include "print_uart.h"
#include "common.h"
#include "PL181.h"
#include "FatFS.h"
#include "elf.h"
#include "VirtualFS.h"
#include "WrappedFile.h"
#include "Stdio.h"
#include "TTY.h"

int main(int argc, const char **argv);

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <fcntl.h>

struct VectorTable
{
	enum ExceptionType
	{
		kReset = 0,					//the initial supervisor stack
		kUndefinedInstruction = 1,
		kSupervisorCall = 2,
		kPrefetchAbort = 3,
		kDataAbort = 4,
		kIrq = 6,
		kFiq = 7,
	};
	typedef void(*ExceptionVector)(void);

	static void EncodeAndWriteBranch(ExceptionVector v, ExceptionType e, unsigned int subtract = 0)
	{
		unsigned int *pBase = GetTableAddress();

		unsigned int target = (unsigned int)v - subtract;
		unsigned int source = (unsigned int)pBase + (unsigned int)e * 4;
		unsigned int diff = target - source - 8;

		ASSERT(((diff >> 2) & ~0xffffff) == 0);

		unsigned int b = (0xe << 28) | (10 << 24) | (((unsigned int)diff >> 2) & 0xffffff);
		pBase[(unsigned int)e] = b;
	}

	static void SetTableAddress(unsigned int *pAddress)
	{
#ifdef PBES
		asm("mcr p15, 0, %0, c12, c0, 0" : : "r" (pAddress) : "memory");
#endif
	}

	static unsigned int *GetTableAddress(void)
	{
#ifdef PBES
		unsigned int existing;
		asm("mrc p15, 0, %0, c12, c0, 0" : "=r" (existing) : : "cc");

		return (unsigned int *)existing;
#else
		return 0;
#endif
	}
};

enum Mode
{
	kUser = 16,
	kFiq = 17,
	kIrq = 18,
	kSupervisor = 19,
	kAbort = 23,
	kUndefined = 27,
	kSystem = 31,
};

struct Cpsr
{
	unsigned int m_mode:5;
	unsigned int m_t:1;
	unsigned int m_f:1;
	unsigned int m_i:1;
	unsigned int m_a:1;
	unsigned int m_e:1;
	unsigned int m_it2:6;
	unsigned int m_ge:4;
	unsigned int m_raz:4;
	unsigned int m_j:1;
	unsigned int m_it:2;
	unsigned int m_q:1;
	unsigned int m_v:1;
	unsigned int m_c:1;
	unsigned int m_z:1;
	unsigned int m_n:1;
};

struct RfeData
{
	union {
		void(*m_pPc)(void);
		void *func;
	};
	Cpsr m_cpsr;
	unsigned int *m_pSp;
};

const unsigned int initial_stack_size = 1024;
unsigned int initial_stack[initial_stack_size];
unsigned int initial_stack_end;

//testing elf
//extern unsigned int _binary_C__Users_Simon_workspace_tester_Debug_tester_strip_start;
//extern unsigned int _binary_C__Users_Simon_workspace_tester_Debug_tester_strip_size;
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_strip_start;
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_strip_size;

extern unsigned int _binary_ld_stripped_so_start;
extern unsigned int _binary_ld_stripped_so_size;
/////////

struct FixedStack
{
	static const unsigned int sm_size = 1024;
	unsigned int m_stack[sm_size];
} g_stacks[8];

extern "C" unsigned int *GetStackHigh(VectorTable::ExceptionType e)
{
	ASSERT((unsigned int)e < 8);
	return g_stacks[(unsigned int)e].m_stack + FixedStack::sm_size - 1;
}


extern "C" void _UndefinedInstruction(void);
extern "C" void UndefinedInstruction(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("undefined instruction at ");
	p.Print(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\n");
	}
}

extern "C" void _SupervisorCall(void);
extern "C" unsigned int SupervisorCall(unsigned int r7);

extern "C" void _PrefetchAbort(void);
extern "C" void PrefetchAbort(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("prefetch abort at ");
	p.Print(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\n");
	}

	ASSERT(0);
}

extern "C" void _DataAbort(void);
extern "C" void DataAbort(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("data abort at ");
	p.Print(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\n");
	}

	ASSERT(0);
}

extern "C" void Irq(void)
{
}

extern "C" void Fiq(void)
{
}

extern "C" void InvokeSyscall(unsigned int r7);
extern "C" void ChangeModeAndJump(unsigned int r0, unsigned int r1, unsigned int r2, RfeData *);

extern "C" void _start(void);


extern unsigned int TlsLow, TlsHigh;
extern unsigned int thread_section_begin, thread_section_end, thread_section_mid;

extern unsigned int entry;
extern unsigned int __end__;


extern unsigned int __trampoline_start__;
extern unsigned int __trampoline_end__;

extern unsigned int __UndefinedInstruction_addr;
extern unsigned int __SupervisorCall_addr;
extern unsigned int __PrefetchAbort_addr;
extern unsigned int __DataAbort_addr;
extern unsigned int __Irq_addr;
extern unsigned int __Fiq_addr;

extern "C" void __UndefinedInstruction(void);
extern "C" void __SupervisorCall(void);
extern "C" void __PrefetchAbort(void);
extern "C" void __DataAbort(void);
extern "C" void __Irq(void);
extern "C" void __Fiq(void);

extern "C" void EnableFpu(bool);

static void MapKernel(unsigned int physEntryPoint)
{
	unsigned int virt_phys_offset = (unsigned int)&entry - physEntryPoint;
    //the end of the program, rounded up to the nearest phys page
    unsigned int virt_end = ((unsigned int)&__end__ + 4095) & ~4095;
    unsigned int image_length_4k_align = virt_end - (unsigned int)&entry;
    ASSERT((image_length_4k_align & 4095) == 0);
    unsigned int image_length_section_align = (image_length_4k_align + 1048575) & ~1048575;

    PhysPages::BlankUsedPages();
    PhysPages::ReservePages(physEntryPoint >> 12, image_length_section_align >> 12);

    VirtMem::AllocL1Table(physEntryPoint);
    TranslationTable::TableEntryL1 *pEntries = VirtMem::GetL1TableVirt();

    if (!VirtMem::InitL1L2Allocators())
    	ASSERT(0);

    //disable the existing phys map
    for (unsigned int i = physEntryPoint >> 20; i < (physEntryPoint + image_length_section_align) >> 20; i++)
    	pEntries[i].fault.Init();

    //IO sections
#ifdef PBES
    MapPhysToVirt((void *)(1152 * 1048576), (void *)(0xfd0 * 1048576), 1048576,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#else
    VirtMem::MapPhysToVirt((void *)(256 * 1048576), (void *)(0xfefU * 1048576), 1048576,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    VirtMem::MapPhysToVirt((void *)(257 * 1048576), (void *)(0xfd0U * 1048576), 1048576,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#endif

    //map the trampoline vector one page up from exception vector
    VirtMem::MapPhysToVirt(PhysPages::FindPage(), VectorTable::GetTableAddress(), 4096,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    unsigned int tramp_phys = (unsigned int)&__trampoline_start__ - virt_phys_offset;
    VirtMem::MapPhysToVirt((void *)tramp_phys, (void *)((unsigned int)VectorTable::GetTableAddress() + 0x1000), 4096,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    SetHighBrk((void *)virt_end);

    //executable top section
    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);

    //copy in the high code
    unsigned char *pHighCode = (unsigned char *)(0xffff0fe0 - 4);
    unsigned char *pHighSource = (unsigned char *)&TlsLow;

    for (unsigned int count = 0; count < (unsigned int)&TlsHigh - (unsigned int)&TlsLow; count++)
    	pHighCode[count] = pHighSource[count];

    //set the emulation value
    *(unsigned int *)(0xffff0fe0 - 4) = 0;
    //set the register value
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (0) : "cc");
}

template <class T>
void DumpMem(T *pVirtual, unsigned int len)
{
	PrinterUart<PL011> p;
	for (unsigned int count = 0; count < len; count++)
	{
		p.Print((unsigned int)pVirtual);
		p.PrintString(": ");
		p.Print(*pVirtual);
		p.PrintString("\n");
		pVirtual++;
	}
}

void LoadElf(Elf &elf, unsigned int voffset, bool &has_tls, unsigned int &tls_memsize, unsigned int &tls_filesize, unsigned int &tls_vaddr)
{
	PrinterUart<PL011> p;

	has_tls = false;

	for (unsigned int count = 0; count < elf.GetNumProgramHeaders(); count++)
	{
		void *pData;
		unsigned int vaddr;
		unsigned int memSize, fileSize;

		int p_type = elf.GetProgramHeader(count, &pData, &vaddr, &memSize, &fileSize);

		vaddr += voffset;

		p.PrintString("Header ");
		p.Print(count);
		p.PrintString(" file data ");
		p.Print((unsigned int)pData);
		p.PrintString(" virtual address ");
		p.Print(vaddr);
		p.PrintString(" memory size ");
		p.Print(memSize);
		p.PrintString(" file size ");
		p.Print(fileSize);
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

				bool ok = VirtMem::MapPhysToVirt(pPhys, (void *)beginVirt, 4096,
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
}

unsigned int FillPHdr(Elf &elf, ElfW(Phdr) *pHdr, unsigned int voffset)
{
	for (unsigned int count = 0; count < elf.GetNumProgramHeaders(); count++)
	{
		void *pData;
		unsigned int vaddr;
		unsigned int memSize, fileSize;

		int p_type = elf.GetProgramHeader(count, &pData, &vaddr, &memSize, &fileSize);

		vaddr += voffset;

		pHdr[count].p_align = 2;
		pHdr[count].p_filesz = fileSize;
		pHdr[count].p_flags = 0;
		pHdr[count].p_memsz = memSize;
		pHdr[count].p_offset = 0;
		pHdr[count].p_paddr = vaddr;
		pHdr[count].p_type = p_type;
		pHdr[count].p_vaddr = vaddr;
	}

	return elf.GetNumProgramHeaders();
}

extern "C" void Setup(unsigned int entryPoint)
{
	VectorTable::SetTableAddress(0);

	MapKernel(entryPoint);

	PL011::EnableFifo(false);
	PL011::EnableUart(true);

	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true);

	PrinterUart<PL011> p;

	p << "mmu and uart enabled\n";

	__UndefinedInstruction_addr = (unsigned int)&_UndefinedInstruction;
	__SupervisorCall_addr = (unsigned int)&_SupervisorCall;
	__PrefetchAbort_addr = (unsigned int)&_PrefetchAbort;
	__DataAbort_addr = (unsigned int)&_DataAbort;

	VectorTable::EncodeAndWriteBranch(&__UndefinedInstruction, VectorTable::kUndefinedInstruction, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__SupervisorCall, VectorTable::kSupervisorCall, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__PrefetchAbort, VectorTable::kPrefetchAbort, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__DataAbort, VectorTable::kDataAbort, 0xf001b000);
	p << "exception table inserted\n";

	EnableFpu(true);

	if (!InitMempool((void *)0xa0000000, 256))
		ASSERT(0);


//	{
		PL181 sd((volatile void *)(0xfefU * 1048576 + 0x5000));

		sd.GoIdleState();
		if (!sd.GoReadyState())
		{
			p << "no card\n";
			ASSERT(0);
		}
		if (!sd.GoIdentificationState())
		{
			p << "definitely no card\n";
			ASSERT(0);
		}
		unsigned int rca;
		bool ok = sd.GetCardRcaAndGoStandbyState(rca);
		ASSERT(ok);

		ok = sd.GoTransferState(rca);
		ASSERT(ok);
//
////				char buffer[100];
////				ok = sd.ReadDataFromLogicalAddress(1, buffer, 100);
////				ASSERT(ok);
//
//				fat.ReadBpb();
//				fat.ReadEbr();
//
//				unsigned int fat_size = fat.FatSize();
//				fat_size = (fat_size + 4095) & ~4095;
//				unsigned int fat_pages = fat_size >> 12;
//
//				for (unsigned int count = 0; count < fat_pages; count++)
//				{
//					void *pPhysPage = PhysPages::FindPage();
//					ASSERT(pPhysPage != (void *)-1);
//
//					ok = VirtMem::MapPhysToVirt(pPhysPage, (void *)(0x90000000 + count * 4096),
//							4096, TranslationTable::kRwNa, TranslationTable::kNoExec,
//							TranslationTable::kOuterInnerWbWa, 0);
//					ASSERT(ok);
//				}
//
//				fat.HaveFatMemory((void *)0x90000000);
//				fat.LoadFat();
//
//
////				void *pDir = __builtin_alloca(fat.ClusterSizeInBytes() * fat.CountClusters(fat.RootDirectoryRelCluster()));
////				fat.ReadClusterChain(pDir, fat.RootDirectoryRelCluster());
////
////				unsigned int slot = 0;
////				FatFS::DirEnt dirent;
//
//				fat.ListDirectory(p, fat.RootDirectoryRelCluster());

//				do
//				{
//					ok = fat.IterateDirectory(pDir, slot, dirent);
//					if (ok)
//					{
//						p.PrintString("file "); p.PrintString(dirent.m_name);
//						p.PrintString(" rel cluster "); p.Print(dirent.m_cluster);
//						p.PrintString(" abs cluster "); p.Print(fat.RelativeToAbsoluteCluster(dirent.m_cluster));
//						p.PrintString(" size "); p.Print(dirent.m_size);
//						p.PrintString(" attr "); p.Print(dirent.m_type);
//						p.PrintString("\n");
//					}
//
////					if (strcmp(dirent.m_name, "ld-2.15.so") == 0)
////					{
////						unsigned int file_size = dirent.m_size;
////						unsigned page_file_size = (file_size + 4095) & ~4095;
////						unsigned int file_pages = page_file_size >> 12;
////
////						for (unsigned int count = 0; count < file_pages; count++)
////						{
////							void *pPhysPage = PhysPages::FindPage();
////							ASSERT(pPhysPage != (void *)-1);
////
////							ok = VirtMem::MapPhysToVirt(pPhysPage, (void *)(0xa0000000 + count * 4096),
////									4096, TranslationTable::kRwNa, TranslationTable::kNoExec,
////									TranslationTable::kOuterInnerWbWa, 0);
////							ASSERT(ok);
////						}
////
////						unsigned int to_read = file_size;
////						unsigned char *pReadInto = (unsigned char *)0xa0000000;
////						unsigned int cluster = dirent.m_cluster;
////						while (to_read > 0)
////						{
////							bool full_read;
////							unsigned int reading;
////
////							if (fat.ClusterSizeInBytes() > to_read)		//not enough data left
////							{
////								full_read = false;
////								reading = to_read;
////							}
////							else
////							{
////								full_read = true;
////								reading = fat.ClusterSizeInBytes();
////							}
////
////							fat.ReadCluster(pReadInto, fat.RelativeToAbsoluteCluster(cluster), reading);
////							to_read -= reading;
////							pReadInto += reading;
////
////							if (full_read)
////							{
////								ok = fat.NextCluster(cluster, cluster);
////								ASSERT(ok);
////							}
////							else
////								ASSERT(to_read == 0);
////				ReadEbr		}
////					}
//				} while (ok);

//				p << "\n";
//			}
//		}

		VirtualFS vfs;
		vfs.Mkdir("/", "Volumes");
		vfs.Mkdir("/Volumes", "sd");

		FatFS fat(sd);

		vfs.Attach(fat, "/Volumes/sd");

//		BaseDirent *b = vfs.Locate("/");
//		b = vfs.Locate("/Volumes");
//		b = vfs.Locate("/Volumes/sd");
//		b = vfs.Locate("/Volumes/sd/Libraries");
//
//		b = vfs.Locate("/Volumes/sd/Libraries/ld-2.15.so");
//
//		b = vfs.Locate("/Volumes/sd/Programs/tester");
//
		fat.Attach(vfs, "/Programs");
//
//		b = vfs.Locate("/Volumes/sd/Programs/Volumes/sd/Libraries/ld-2.15.so");

		BaseDirent *f = vfs.OpenByName("/Volumes/sd/Programs/Volumes/sd/Libraries/ld-2.15.so", O_RDONLY);
		ASSERT(f);
		ProcessFS pfs("/Volumes", "/sd/Programs");

		char string[500];
		pfs.BuildFullPath("/sd/Programs/Volumes/sd/Libraries/ld-2.15.so", string, 500);
		pfs.BuildFullPath("Volumes/sd/Libraries/ld-2.15.so", string, 500);
		pfs.Chdir("Volumes");
		pfs.Chdir("/sd");

		int fd = pfs.Open(*f);
		ASSERT(fd >= 0);
		int fd2 = pfs.Dup(fd);
		int fd3 = pfs.Dup(fd2);
		WrappedFile *w = pfs.GetFile(fd2);
		pfs.Close(fd2);
		w = pfs.GetFile(fd3);
		pfs.Close(fd3);
		pfs.Close(fd);
		w = pfs.GetFile(fd);

		vfs.Mkdir("/", "Devices");

		PL011 uart;
		Stdio in(Stdio::kStdin, uart, vfs);
		vfs.AddOrphanFile("/Devices", in);

		BaseDirent *pIn = vfs.OpenByName("/Devices/stdin", O_WRONLY);

		Stdio out(Stdio::kStdout, uart, vfs);
		vfs.AddOrphanFile("/Devices", out);

		BaseDirent *pOut = vfs.OpenByName("/Devices/stdout", O_WRONLY);
		((File *)pOut)->WriteTo("hello", strlen("hello"), 0);

		TTY tty(in, out, vfs);
		vfs.AddOrphanFile("/Devices", tty);

		int ld = pfs.Open(*vfs.OpenByName(pfs.BuildFullPath("/sd/Programs/Volumes/sd/Libraries/ld-2.15.so", string, 500),
				O_RDONLY));

		char elf_header[100];
		pfs.GetFile(ld)->Read(elf_header, 100);

//		sd.GoIdleState();
//		unsigned int ocr = sd.SendOcr();
//
//		unsigned int cid = sd.AllSendCid();
//		unsigned int rela = sd.SendRelativeAddr();
//
//		p.PrintString("CID "); p.Print(cid); p.PrintString("\n");
//		p.PrintString("RCA "); p.Print(rela >> 16); p.PrintString("\n");
//		p.PrintString("STATUS "); p.Print(rela & 0xffff); p.PrintString("\n");
//
//		sd.SelectDeselectCard(rela >> 16);
//
//		sd.ReadDataUntilStop(0);
//
//		char buffer[100];
//		sd.ReadOutData(buffer, 100);
//
//		sd.StopTransmission();
//	}

//	asm volatile ("swi 0");

//	p.PrintString("swi\n");

//	asm volatile (".word 0xffffffff\n");
//	InvokeSyscall(1234);

	Elf startingElf, interpElf;

//	startingElf.Load(&_binary__home_simon_workspace_tester_Debug_tester_strip_start,
//			(unsigned int)&_binary__home_simon_workspace_tester_Debug_tester_strip_size);
//
//	interpElf.Load(&_binary_ld_stripped_so_start,
//			(unsigned int)&_binary_ld_stripped_so_size);//no thanks

	bool has_tls = false;
	unsigned int tls_memsize, tls_filesize, tls_vaddr;

	//////////////////////////////////////
	LoadElf(interpElf, 0x70000000, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	ASSERT(has_tls == false);
	LoadElf(startingElf, 0, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	//////////////////////////////////////

	RfeData rfe;
//	rfe.m_pPc = &_start;
	rfe.func = (void *)((unsigned int)interpElf.GetEntryPoint() + 0x70000000);
	rfe.m_pSp = (unsigned int *)0xffeffffc;		//4095MB-4b

	memset(&rfe.m_cpsr, 0, 4);
	rfe.m_cpsr.m_mode = kUser;
	rfe.m_cpsr.m_a = 1;
	rfe.m_cpsr.m_i = 1;
	rfe.m_cpsr.m_f = 1;

	if ((unsigned int)rfe.m_pPc & 1)		//thumb
		rfe.m_cpsr.m_t = 1;

	//move down enough for some stuff
	rfe.m_pSp = rfe.m_pSp - 100;
	//fill in argc
	rfe.m_pSp[0] = 1;
	//fill in argv
	const char *pElfName = "/init.elf";
	const char *pEnv = "LD_DEBUG=all";

	ElfW(auxv_t) *pAuxVec = (ElfW(auxv_t) *)&rfe.m_pSp[5];
	unsigned int aux_size = sizeof(ElfW(auxv_t)) * 4;

	ElfW(Phdr) *pHdr = (ElfW(Phdr) *)((unsigned int)pAuxVec + aux_size);

	pAuxVec[0].a_type = AT_PHDR;
//	pAuxVec[0].a_un.a_val = (unsigned int)pHdr;
//	pAuxVec[0].a_un.a_val = (unsigned int)startingElf.GetAllProgramHeaders();
	pAuxVec[0].a_un.a_val = 0x8000 + 52;

	pAuxVec[1].a_type = AT_PHNUM;
//	pAuxVec[1].a_un.a_val = 1;

	pAuxVec[2].a_type = AT_ENTRY;
	pAuxVec[2].a_un.a_val = (unsigned int)startingElf.GetEntryPoint();

	pAuxVec[3].a_type = AT_BASE;
	pAuxVec[3].a_un.a_val = 0x70000000;

	pAuxVec[4].a_type = AT_NULL;
	pAuxVec[4].a_un.a_val = 0;

	/*pHdr->p_align = 2;
//	pHdr->p_filesz = (unsigned int)&thread_section_mid - (unsigned int)&thread_section_begin;
//	pHdr->p_memsz = (unsigned int)&thread_section_end - (unsigned int)&thread_section_begin;
	pHdr->p_offset = 0;
	pHdr->p_paddr = 0;
//	pHdr->p_vaddr = (unsigned int)&thread_section_begin;
	pHdr->p_type = PT_TLS;

	if (has_tls)
	{
		pHdr->p_filesz = tls_filesize;
		pHdr->p_memsz = tls_memsize;
		pHdr->p_vaddr = tls_vaddr;
	}
	else
	{
		pHdr->p_filesz = 0;
		pHdr->p_memsz = 0;
		pHdr->p_vaddr = 0;
	}*/


//	pAuxVec[1].a_un.a_val = FillPHdr(startingElf, pHdr, 0);
	pAuxVec[1].a_un.a_val = startingElf.GetNumProgramHeaders();
	unsigned int hdr_size = sizeof(ElfW(Phdr)) * pAuxVec[1].a_un.a_val;

	unsigned int text_start_addr = (unsigned int)pAuxVec + aux_size + hdr_size;

	unsigned int e = (unsigned int)strcpy((char *)text_start_addr, pEnv);
	unsigned int a = (unsigned int)strcpy((char *)text_start_addr + strlen(pEnv) + 1, pElfName);

	rfe.m_pSp[1] = a;
	rfe.m_pSp[2] = 0;
	rfe.m_pSp[3] = e;
	rfe.m_pSp[4] = 0;

	ChangeModeAndJump(0, 0, 0, &rfe);
}
