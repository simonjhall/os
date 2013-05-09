#include "print_uart.h"

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
	return g_stacks[(unsigned int)e].m_stack + FixedStack::sm_size - 1;
}


extern "C" void _UndefinedInstruction(void);
extern "C" void UndefinedInstruction(void)
{
}

extern "C" void _SupervisorCall(void);
extern "C" unsigned int SupervisorCall(unsigned int r7);

extern "C" void _PrefetchAbort(void);
extern "C" void PrefetchAbort(void)
{
}

extern "C" void _DataAbort(void);
extern "C" void DataAbort(void)
{
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

namespace TranslationTable
{
	//properties
	enum MemRegionType
	{
		kStronglyOrdered = 0,
		kShareableDevice = 1,
		kOuterInnerWtNoWa = 2,
		kOuterInnerWbNoWa = 3,

		kOuterInnerNc = 4,
		kOuterInnerWbWa = 7,

		kNonShareableDevice = 8,
	};

	enum ExecuteNever
	{
		kExec = 0,
		kNoExec = 1,
	};

	enum AccessPerm
	{
		kNaNa = 0,
		kRwNa = 1,
		kRwRo = 2,
		kRwRw = 3,
		kRoNa = 5,
		kRoRo = 7,
	};

	//first level (fault is also good for second level)

	struct Fault
	{
		inline Fault(void) : m_zero(0) {};

		unsigned int m_ignore:30;
		unsigned int m_zero:2;
	};

	struct PageTable
	{
		inline PageTable() {};

		inline PageTable(unsigned int *pBase, unsigned int domain)
		:  m_pageTableBase ((unsigned int)pBase >> 10),
		   m_domain (domain),
		   m_sbz (0),
		   m_ns (0),
		   m_pxn (0),
		   m_zeroOne (1)
		{};

		unsigned int m_pageTableBase:22;
		unsigned int m_ignore1:1;
		unsigned int m_domain:4;
		unsigned int m_sbz:1;
		unsigned int m_ns:1;
		unsigned int m_pxn:1;
		unsigned int m_zeroOne:2;
	};

	struct Section
	{
		inline Section() {};

		inline Section(unsigned int *pBase, AccessPerm perm, ExecuteNever xn, MemRegionType type, unsigned int domain)
		: m_pxn(0),
		  m_one(1),
		  m_b((unsigned int)type & 1),
		  m_c(((unsigned int)type > 1) & 1),
		  m_xn((unsigned int)xn),
		  m_domain(domain),
		  m_ap((unsigned int)perm & 3),
		  m_tex((unsigned int)type >> 2),
		  m_ap2((unsigned int)perm >> 2),
		  m_s(0),
		  m_ng(0),
		  m_zero(0),
		  m_ns(0),
		  m_sectionBase ((unsigned int)pBase >> 20)
		{
		}
		unsigned int m_pxn:1;
		unsigned int m_one:1;
		unsigned int m_b:1;
		unsigned int m_c:1;
		unsigned int m_xn:1;
		unsigned int m_domain:4;
		unsigned int m_ignore1:1;
		unsigned int m_ap:2;
		unsigned int m_tex:3;
		unsigned int m_ap2:1;
		unsigned int m_s:1;
		unsigned int m_ng:1;
		unsigned int m_zero:1;
		unsigned int m_ns:1;
		unsigned int m_sectionBase:12;
	};

	//second level
	struct SmallPage
	{
		inline SmallPage() {};

		inline SmallPage(unsigned int *pBase, AccessPerm perm, ExecuteNever xn, MemRegionType type) :
		  m_xn((unsigned int)xn),
		  m_one(1),
		  m_b((unsigned int)type & 1),
		  m_c(((unsigned int)type > 1) & 1),
		  m_ap((unsigned int)perm & 3),
		  m_tex((unsigned int)type >> 2),
		  m_ap2((unsigned int)perm >> 2),
		  m_s(0),
		  m_ng(0),
		  m_pageBase ((unsigned int)pBase >> 12)
		{
		};

		unsigned int m_xn:1;
		unsigned int m_one:1;
		unsigned int m_b:1;
		unsigned int m_c:1;
		unsigned int m_ap:2;
		unsigned int m_tex:3;
		unsigned int m_ap2:1;
		unsigned int m_s:1;
		unsigned int m_ng:1;
		unsigned int m_pageBase:20;
	};
}

extern unsigned int TlsLow, TlsHigh;

static inline void enable_mmu(void)
{
    static unsigned int __attribute__((aligned(16384))) page_table[NUM_PAGE_TABLE_ENTRIES];
    unsigned int i;
    unsigned int reg;

    TranslationTable::Section *pSections = (TranslationTable::Section *)page_table;

    for (i = 0; i < 4096; i++)
    	pSections[i] = TranslationTable::Section((unsigned int *)(i * 1048576),
    			TranslationTable::kNaNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);

    for (i = SDRAM_START >> 20; i < SDRAM_END >> 20; i++)
    {
    	pSections[i] = TranslationTable::Section((unsigned int *)(i * 1048576),
    			TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    }

    //executable top section
    pSections[4095] = TranslationTable::Section((unsigned int *)(2 * 1048576),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pSections[4094] = TranslationTable::Section((unsigned int *)(3 * 1048576),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    //IO sections
    pSections[257] = TranslationTable::Section((unsigned int *)(257 * 1048576),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);

    /* Copy the page table address to cp15 */
    asm volatile("mcr p15, 0, %0, c2, c0, 0"
            : : "r" (page_table) : "memory");

    //change the domain bits
    asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (1));		//0=no access, 1=client, 3=manager

    /* Enable the MMU */
    asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg) : : "cc");
    reg|=0x1;
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (reg) : "cc");

    //copy in the high code
    memcpy((unsigned char *)(0xffff0fe0 - 4), &TlsLow, (unsigned int)TlsHigh - (unsigned int)TlsLow);
    *(unsigned int *)(0xffff0fe0 - 4) = 4094U * 1048576;		//make tls start at the low end of the stack
}

extern "C" void Setup(void)
{
	PrinterUart p;
	p.PrintString("pre-mmu\r\n");

	enable_mmu();

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
