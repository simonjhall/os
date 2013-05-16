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
	void(*m_pPc)(void);
	Cpsr m_cpsr;
	unsigned int *m_pSp;
};

const unsigned int initial_stack_size = 1024;
unsigned int initial_stack[initial_stack_size];
unsigned int initial_stack_end;

//testing elf
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_strip_start;
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_strip_size;
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

    PhysPages::AllocL1Table(physEntryPoint);
    TranslationTable::TableEntryL1 *pEntries = PhysPages::GetL1Table();

    //disable the existing phys map
    for (unsigned int i = physEntryPoint >> 20; i < (physEntryPoint + image_length_section_align) >> 20; i++)
    	pEntries[i].fault.Init();

//    MapPhysToVirt((void *)(PhysPages::s_startAddr + 0), 0, 0x10000, TranslationTable::kRwNa, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
//    MapPhysToVirt((void *)(PhysPages::s_startAddr + 0x10000), (void *)(PhysPages::s_startAddr + 0x10000), end - (PhysPages::s_startAddr + 0x10000), TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    SetHighBrk((void *)virt_end);

    //executable top section
    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    //IO sections
#ifdef PBES
    MapPhysToVirt((void *)(1152 * 1048576), (void *)(1152 * 1048576), 1048576, TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#else
    MapPhysToVirt((void *)(257 * 1048576), (void *)(257 * 1048576), 1048576, TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#endif

    //map the trampoline vector one page up from exception vector
    MapPhysToVirt(PhysPages::FindPage(), VectorTable::GetTableAddress(), 4096, TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kShareableDevice, 0);

    unsigned int tramp_phys = virt_phys_offset + ((unsigned int)&__trampoline_start__ - (unsigned int)&entry);
    MapPhysToVirt((void *)tramp_phys, (void *)((unsigned int)VectorTable::GetTableAddress() + 0x1000), 4096, TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kShareableDevice, 0);

    for (unsigned int count = 0; count < 5; count++)
    {
    	PrinterUart<PL011> p;
    	p.Print(count * 1048576);
    	p.PrintString(": ");

    	if (pEntries[count].Print(p))
    	{
    		p.PrintString("\r\n");
    		TranslationTable::TableEntryL2 *pL2 = pEntries[count].pageTable.GetPhysPageTable();

    		for (unsigned int inner = 0; inner < 256; inner++)
    		{
    			p.PrintString("\t");
    			p.Print(count * 1048576 + inner * 4096);
    			p.PrintString(": ");
    			pL2[inner].Print(p);
    		}

    		p.PrintString("\r\n");
    	}
    }
//
//    /* Copy the page table address to cp15 */
//    asm volatile("mcr p15, 0, %0, c2, c0, 0"
//            : : "r" (pEntries) : "memory");
//
//    //change the domain bits
//    asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (1));		//0=no access, 1=client, 3=manager
//
//    EnableMmu(true);

    //copy in the high code
    memcpy((unsigned char *)(0xffff0fe0 - 4), &TlsLow, (unsigned int)TlsHigh - (unsigned int)TlsLow);

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

extern "C" void Setup(unsigned int entryPoint)
{
	VectorTable::SetTableAddress(0);

	SetupMmu(entryPoint);

	PL011::EnableFifo(false);
	PrinterUart<PL011> p;
	p.PrintString("pre-mmu\r\n");

	p.PrintString("mmu enabled\r\n");

	__UndefinedInstruction_addr = (unsigned int)&_UndefinedInstruction;
	__SupervisorCall_addr = (unsigned int)&_SupervisorCall;
	__PrefetchAbort_addr = (unsigned int)&_PrefetchAbort;
	__DataAbort_addr = (unsigned int)&_DataAbort;

	VectorTable::EncodeAndWriteBranch(&__UndefinedInstruction, VectorTable::kUndefinedInstruction, 0);
	VectorTable::EncodeAndWriteBranch(&__SupervisorCall, VectorTable::kSupervisorCall, 0);
	VectorTable::EncodeAndWriteBranch(&__PrefetchAbort, VectorTable::kPrefetchAbort, 0);
	VectorTable::EncodeAndWriteBranch(&__DataAbort, VectorTable::kDataAbort, 0);
	p.PrintString("exception table inserted\r\n");

//	asm volatile ("swi 0");

//	p.PrintString("swi\r\n");

//	asm volatile (".word 0xffffffff\n");
//	InvokeSyscall(1234);

	Elf startingElf;
	startingElf.Load(&_binary__home_simon_workspace_tester_Debug_tester_strip_start,
			_binary__home_simon_workspace_tester_Debug_tester_strip_size);

	RfeData rfe;
	rfe.m_pPc = &_start;
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
	unsigned int aux_size = sizeof(ElfW(auxv_t)) * 3;

	ElfW(Phdr) *pHdr = (ElfW(Phdr) *)((unsigned int)pAuxVec + aux_size);

	pAuxVec[0].a_type = AT_PHDR;
	pAuxVec[0].a_un.a_val = (unsigned int)pHdr;

	pAuxVec[1].a_type = AT_PHNUM;
	pAuxVec[1].a_un.a_val = 1;

	pAuxVec[2].a_type = AT_NULL;

	pHdr->p_align = 2;
	pHdr->p_filesz = (unsigned int)&thread_section_mid - (unsigned int)&thread_section_begin;
	pHdr->p_memsz = (unsigned int)&thread_section_end - (unsigned int)&thread_section_begin;
	pHdr->p_offset = 0;
	pHdr->p_paddr = 0;
	pHdr->p_vaddr = (unsigned int)&thread_section_begin;
	pHdr->p_type = PT_TLS;

	unsigned int hdr_size = sizeof(ElfW(Phdr));

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
