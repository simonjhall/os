#include "print_uart.h"
#include "common.h"

int main(void);

#include <stdio.h>
#include <string.h>

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

	static void EncodeAndWriteBranch(ExceptionVector v, ExceptionType e)
	{
		unsigned int *pBase = 0;

		unsigned int target = (unsigned int)v;
		unsigned int source = (unsigned int)e * 4;
		unsigned int diff = target - source - 8;

		ASSERT(((diff >> 2) & ~0xffffff) == 0);

		unsigned int b = (0xe << 28) | (10 << 24) | (((unsigned int)diff >> 2) & 0xffffff);
		pBase[(unsigned int)e] = b;
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
extern "C" void UndefinedInstruction(void)
{
}

extern "C" void _SupervisorCall(void);
extern "C" unsigned int SupervisorCall(unsigned int r7);

extern "C" void _PrefetchAbort(void);
extern "C" void PrefetchAbort(unsigned int addr)
{
	PrinterUart p;
	p.PrintString("prefetch abort at ");
	p.Print(addr);
	p.PrintString("\r\n");

	ASSERT(0);
}

extern "C" void _DataAbort(void);
extern "C" void DataAbort(unsigned int addr)
{
	PrinterUart p;
	p.PrintString("data abort at ");
	p.Print(addr);
	p.PrintString("\r\n");

	ASSERT(0);
}

extern "C" void Irq(void)
{
}

extern "C" void Fiq(void)
{
}

extern "C" void InvokeSyscall(unsigned int r7);
extern "C" void ChangeModeAndJump(RfeData *);

extern "C" void _start(void);

#define NUM_PAGE_TABLE_ENTRIES 4096 /* 1 entry per 1MB, so this covers 4G address space */
#define CACHE_DISABLED    0x12
#define SDRAM_START       0x0
#define SDRAM_END         (128 * 1024 * 1024)
#define CACHE_WRITEBACK   0x1e



extern unsigned int TlsLow, TlsHigh;
extern unsigned int __init_array_start;


static inline void SetupMmu(void)
{
//    static TranslationTable::TableEntryL2 __attribute__((aligned(16384))) page[256];
    unsigned int i;
    unsigned int reg;

    PhysPages::BlankUsedPages();
    PhysPages::ReservePages(0, 1048576 / 4096);

    PhysPages::AllocL1Table();
    TranslationTable::TableEntryL1 *pEntries = PhysPages::GetL1Table();

    //disable everything
    for (i = 0; i < 4096; i++)
    	pEntries[i].section.Init((unsigned int *)(i * 1048576),
    			TranslationTable::kNaNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);

    MapPhysToVirt(0, 0, 0x10000, TranslationTable::kRwNa, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    MapPhysToVirt((void *)0x10000, (void *)0x10000, 1048576 - 0x10000, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    SetHighBrk((void *)1048576);

    //executable top section
    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    //IO sections
//    pEntries[257].section.Init((unsigned int *)(257 * 1048576),
//    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    MapPhysToVirt((void *)(257 * 1048576), (void *)(257 * 1048576), 1048576, TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);

    /* Copy the page table address to cp15 */
    asm volatile("mcr p15, 0, %0, c2, c0, 0"
            : : "r" (pEntries) : "memory");

    //change the domain bits
    asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (1));		//0=no access, 1=client, 3=manager

    EnableMmu(true);

    //copy in the high code
    memcpy((unsigned char *)(0xffff0fe0 - 4), &TlsLow, (unsigned int)TlsHigh - (unsigned int)TlsLow);

    //set the emulation value
    *(unsigned int *)(0xffff0fe0 - 4) = (unsigned int)&__init_array_start - 16 - 8;
    //set the register value
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" ((unsigned int)&__init_array_start - 16 - 8) : "cc");
}

extern "C" void Setup(void)
{
	PrinterUart p;
	p.PrintString("pre-mmu\r\n");

	SetupMmu();

	p.PrintString("mmu enabled\r\n");

	VectorTable::EncodeAndWriteBranch(&_UndefinedInstruction, VectorTable::kUndefinedInstruction);
	VectorTable::EncodeAndWriteBranch(&_SupervisorCall, VectorTable::kSupervisorCall);
	VectorTable::EncodeAndWriteBranch(&_PrefetchAbort, VectorTable::kPrefetchAbort);
	VectorTable::EncodeAndWriteBranch(&_DataAbort, VectorTable::kDataAbort);

//	asm volatile (".word 0xffffffff\n");
//	InvokeSyscall(1234);
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

	ChangeModeAndJump(&rfe);
}

int main(void)
{
	PrinterUart p;
	p.PrintString("hello world\r\n");

	PrinterUart *pee = new PrinterUart;

	char buffer[100];
	sprintf(buffer, "%p\n", pee);
	p.PrintString(buffer);

	for (int count = 0; count < 10; count++)
	{
		p.Print(count);
		p.PrintString("\r\n");
	}
	return 0;
}
