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

//testing elf
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_start;
extern unsigned int _binary__home_simon_workspace_tester_Debug_tester_size;
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
extern "C" void ChangeModeAndJump(unsigned int r0, unsigned int r1, unsigned int r2, RfeData *);

extern "C" void _start(void);

#define NUM_PAGE_TABLE_ENTRIES 4096 /* 1 entry per 1MB, so this covers 4G address space */
#define CACHE_DISABLED    0x12
#define SDRAM_START       0x0
#define SDRAM_END         (128 * 1024 * 1024)
#define CACHE_WRITEBACK   0x1e



extern unsigned int TlsLow, TlsHigh;
extern unsigned int thread_section_begin, thread_section_end, thread_section_mid;
extern unsigned int _end;

static inline void SetupMmu(void)
{
    unsigned int i;

    PhysPages::BlankUsedPages();
    PhysPages::ReservePages(PhysPages::s_startPage, 1048576 / 4096);

    PhysPages::AllocL1Table();
    TranslationTable::TableEntryL1 *pEntries = PhysPages::GetL1Table();

    //disable everything
    for (i = 0; i < 4096; i++)
    	pEntries[i].section.Init((unsigned int *)(i * 1048576),
    			TranslationTable::kNaNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);

    unsigned int end = ((unsigned int)&_end + 4095) & ~4095;

    MapPhysToVirt((void *)(PhysPages::s_startAddr + 0), 0, 0x10000, TranslationTable::kRwNa, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    MapPhysToVirt((void *)(PhysPages::s_startAddr + 0x10000), (void *)(PhysPages::s_startAddr + 0x10000), end - (PhysPages::s_startAddr + 0x10000), TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    SetHighBrk((void *)end);

    //executable top section
    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);
    //process stack
    pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    //IO sections
    //MapPhysToVirt((void *)(257 * 1048576), (void *)(257 * 1048576), 1048576, TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    MapPhysToVirt((void *)(1152 * 1048576), (void *)(1152 * 1048576), 1048576, TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);

    /* Copy the page table address to cp15 */
    asm volatile("mcr p15, 0, %0, c2, c0, 0"
            : : "r" (pEntries) : "memory");

    //change the domain bits
    asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (1));		//0=no access, 1=client, 3=manager

    EnableMmu(true);

    //copy in the high code
    memcpy((unsigned char *)(0xffff0fe0 - 4), &TlsLow, (unsigned int)TlsHigh - (unsigned int)TlsLow);

    //set the emulation value
    *(unsigned int *)(0xffff0fe0 - 4) = 0;
    //set the register value
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (0) : "cc");
}

extern "C" void Setup(void)
{
	PrinterUart p;
	p.PrintString("pre-mmu\r\n");

	SetupMmu();

//	while (1)
	p.PrintString("mmu enabled\r\n");

	VectorTable::EncodeAndWriteBranch(&_UndefinedInstruction, VectorTable::kUndefinedInstruction);
	VectorTable::EncodeAndWriteBranch(&_SupervisorCall, VectorTable::kSupervisorCall);
	VectorTable::EncodeAndWriteBranch(&_PrefetchAbort, VectorTable::kPrefetchAbort);
	VectorTable::EncodeAndWriteBranch(&_DataAbort, VectorTable::kDataAbort);
	p.PrintString("exception table inserted\r\n");

//	asm volatile (".word 0xffffffff\n");
//	InvokeSyscall(1234);

//	Elf startingElf;
//	startingElf.Load(&_binary__home_simon_workspace_tester_Debug_tester_start,
//			_binary__home_simon_workspace_tester_Debug_tester_size);

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

	for (int count = 0; count < argc; count++)
	{
		printf("%d: %s\r\n", count, argv[count]);
	}
	return 0;
}
