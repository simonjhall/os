#include "print_uart.h"
#include "common.h"

int main(int argc, const char **argv);

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include "elf.h"

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
	p.PrintString("\r\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\r\n");
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
	p.PrintString("\r\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\r\n");
	}

	ASSERT(0);
}

extern "C" void _DataAbort(void);
extern "C" void DataAbort(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("data abort at ");
	p.Print(addr);
	p.PrintString("\r\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\r\n");
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
extern unsigned int _end;


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

static inline void SetupMmu(unsigned int physEntryPoint)
{
	unsigned int virt_phys_offset = (unsigned int)&entry - physEntryPoint;
    //the end of the program, rounded up to the nearest phys page
    unsigned int virt_end = ((unsigned int)&_end + 4095) & ~4095;
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

    SetHighBrk((void *)virt_end);

    //executable top section
    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    //IO sections
#ifdef PBES
    MapPhysToVirt((void *)(1152 * 1048576), (void *)(1152 * 1048576), 1048576,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#else
    VirtMem::MapPhysToVirt((void *)(257 * 1048576), (void *)(257 * 1048576), 1048576,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#endif

    //map the trampoline vector one page up from exception vector
    VirtMem::MapPhysToVirt(PhysPages::FindPage(), VectorTable::GetTableAddress(), 4096,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    unsigned int tramp_phys = (unsigned int)&__trampoline_start__ - virt_phys_offset;
    VirtMem::MapPhysToVirt((void *)tramp_phys, (void *)((unsigned int)VectorTable::GetTableAddress() + 0x1000), 4096,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

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
		p.PrintString("\r\n");
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
		p.PrintString("\r\n");

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
			p.PrintString("\r\n");
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

	SetupMmu(entryPoint);

	PL011::EnableFifo(false);
	PL011::EnableUart(true);

	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true);

	PrinterUart<PL011> p;
	p.PrintString("pre-mmu\r\n");

	p.PrintString("mmu enabled\r\n");

	__UndefinedInstruction_addr = (unsigned int)&_UndefinedInstruction;
	__SupervisorCall_addr = (unsigned int)&_SupervisorCall;
	__PrefetchAbort_addr = (unsigned int)&_PrefetchAbort;
	__DataAbort_addr = (unsigned int)&_DataAbort;

	VectorTable::EncodeAndWriteBranch(&__UndefinedInstruction, VectorTable::kUndefinedInstruction, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__SupervisorCall, VectorTable::kSupervisorCall, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__PrefetchAbort, VectorTable::kPrefetchAbort, 0xf001b000);
	VectorTable::EncodeAndWriteBranch(&__DataAbort, VectorTable::kDataAbort, 0xf001b000);
	p.PrintString("exception table inserted\r\n");

//	asm volatile ("swi 0");

//	p.PrintString("swi\r\n");

//	asm volatile (".word 0xffffffff\n");
//	InvokeSyscall(1234);

	Elf startingElf, interpElf;

	startingElf.Load(&_binary__home_simon_workspace_tester_Debug_tester_strip_start,
			(unsigned int)&_binary__home_simon_workspace_tester_Debug_tester_strip_size);

	interpElf.Load(&_binary_ld_stripped_so_start,
			(unsigned int)&_binary_ld_stripped_so_size);

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
	const char *pEnv = "VAR=var";

	ElfW(auxv_t) *pAuxVec = (ElfW(auxv_t) *)&rfe.m_pSp[5];
	unsigned int aux_size = sizeof(ElfW(auxv_t)) * 4;

	ElfW(Phdr) *pHdr = (ElfW(Phdr) *)((unsigned int)pAuxVec + aux_size);

	pAuxVec[0].a_type = AT_PHDR;
	pAuxVec[0].a_un.a_val = (unsigned int)pHdr;

	pAuxVec[1].a_type = AT_PHNUM;
//	pAuxVec[1].a_un.a_val = 1;

	pAuxVec[2].a_type = AT_ENTRY;
	pAuxVec[2].a_un.a_val = (unsigned int)startingElf.GetEntryPoint();

	pAuxVec[3].a_type = AT_BASE;
	pAuxVec[3].a_un.a_val = 0x70000000;

	pAuxVec[4].a_type = AT_NULL;

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


	pAuxVec[1].a_un.a_val = FillPHdr(startingElf, pHdr, 0);
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

int main(int argc, const char **argv)
{
	PrinterUart<PL011> p;
	p.PrintString("hello world\r\n");

	PrinterUart<PL011> *pee = new PrinterUart<PL011>;

	char buffer[100];
	sprintf(buffer, "%p\r\n", pee);
	p.PrintString(buffer);

	for (int count = 0; count < 10; count++)
	{
		p.Print(count);
		p.PrintString("\r\n");
	}

	for (int count = 0; count < argc; count++)
	{
		printf("%d: %s\r\n", count, argv[count]);
	}

	while (1)
	{
		int c = getchar();
		if (c != EOF)
			printf("%c", c);
	}
	return 0;
}
