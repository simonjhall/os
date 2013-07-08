#include "print_uart.h"
#include "common.h"
#include "PL181.h"
#include "MMCi.h"
#include "FatFS.h"
#include "elf.h"
#include "VirtualFS.h"
#include "WrappedFile.h"
#include "Stdio.h"
#include "TTY.h"
#include "MBR.h"
#include "SP804.h"
#include "PL190.h"
#include "GenericInterruptController.h"
#include "GpTimer.h"
#include "Process.h"
#include "virt_memory.h"
#include "phys_memory.h"

int main(int argc, const char **argv);

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <fcntl.h>

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

extern "C" void EnableIrq(bool);
extern "C" void EnableFiq(bool);
extern "C" bool EnableIrqFiqForMode(Mode, bool, bool);

VersatilePb::SP804 *pTimer0 = 0;
VersatilePb::SP804 *pTimer2 = 0;
OMAP4460::GpTimer *pGpTimer2 = 0, *pGpTimer10 = 0;

InterruptController *pPic = 0;
extern unsigned int pMasterClockClear, masterClockClearValue, master_clock;

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
	return g_stacks[(unsigned int)e].m_stack + FixedStack::sm_size - 2;
}

/////////////////////////////////////////////////
extern "C" void _Und(void);
extern "C" void _Svc(void);
extern "C" void _Prf(void);
extern "C" void _Dat(void);
extern "C" void _Irq(void);

extern "C" void _Irq(void);
extern "C" void Irq(void)
{
	static unsigned int clock = 0;
	PrinterUart<PL011> p;
	p << "irq " << clock++ << " time " << Formatter<float>(master_clock / 100.0f, 2) << "\n";
	p << "pic is " << pPic << "\n";

	InterruptSource *pSource;

	while ((pSource = pPic->WhatHasFired()))
	{
		//todo add to list
		p << "interrupt from " << pSource << " # " << pSource->GetInterruptNumber() << "\n";
		pSource->ClearInterrupt();
	}
	p << "returning from irq\n";
}


/////////////////////////////////////////////////

#if 0
extern "C" void _UndefinedInstruction(void);
extern "C" void UndefinedInstruction(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("undefined instruction at ");
	p.PrintHex(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.PrintHex(pRegisters[count]);
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
	p.PrintHex(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.PrintHex(pRegisters[count]);
		p.PrintString("\n");
	}

	ASSERT(0);
}

extern "C" void _DataAbort(void);
extern "C" void DataAbort(unsigned int addr, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	p.PrintString("data abort at ");
	p.PrintHex(addr);
	p.PrintString("\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.PrintHex(pRegisters[count]);
		p.PrintString("\n");
	}

	ASSERT(0);
}
#endif

extern "C" void _Fiq(void);
extern "C" void Fiq(void)
{
}

extern "C" void InvokeSyscall(unsigned int r7);
extern "C" void ChangeModeAndJump(unsigned int r0, unsigned int r1, unsigned int r2, RfeData *);

extern "C" void _start(void);


extern unsigned int TlsLow, TlsHigh;
extern unsigned int thread_section_begin, thread_section_end, thread_section_mid;

extern unsigned int entry_maybe_misaligned;
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

//unsigned int __attribute__((aligned(0x4000))) TTB[4096];
unsigned int *TTB_virt;
unsigned int *TTB_phys;

static void MapKernel(unsigned int physEntryPoint)
{
	//entry may be pushed up a little bit
	unsigned int entry_aligned_back = (unsigned int)&entry_maybe_misaligned & ~4095;
	unsigned int virt_phys_offset = entry_aligned_back - physEntryPoint;
    //the end of the program, rounded up to the nearest phys page
    unsigned int virt_end = ((unsigned int)&__end__ + 4095) & ~4095;
    unsigned int image_length_4k_align = virt_end - entry_aligned_back;

    ASSERT((image_length_4k_align & 4095) == 0);
    unsigned int image_length_section_align = (image_length_4k_align + 1048575) & ~1048575;

    PhysPages::BlankUsedPages();
    PhysPages::ReservePages(physEntryPoint >> 12, image_length_section_align >> 12);

    VirtMem::AllocL1Table(physEntryPoint);

    if (!VirtMem::InitL1L2Allocators())
    	ASSERT(0);

    //clear any misalignment
    for (unsigned int i = 0; i < ((unsigned int)&entry_maybe_misaligned & 16383) >> 2; i++)
    	VirtMem::GetL1TableVirt(false)[i].fault.Init();

    //disable the existing phys map
    for (unsigned int i = physEntryPoint >> 20; i < (physEntryPoint + image_length_section_align) >> 20; i++)
    	VirtMem::GetL1TableVirt(false)[i].fault.Init();

    //IO sections
#ifdef PBES
    //L4 PER
    VirtMem::MapPhysToVirt((void *)(0x480U * 1048576), (void *)(0xfefU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    //private A9 memory
    VirtMem::MapPhysToVirt((void *)(0x482U * 1048576), (void *)(0xfeeU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    //UART - DUP!
    VirtMem::MapPhysToVirt((void *)(0x480 * 1048576), (void *)(0xfd0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);

    //display
    for (int count = 0; count < 16; count++)
		VirtMem::MapPhysToVirt((void *)((0x580 + count) * 1048576), (void *)((0xfc0U + count) * 1048576), 1048576, true,
				TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
/*
    VirtMem::MapPhysToVirt((void *)(0x4a0 * 1048576), (void *)(0x4a0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    VirtMem::MapPhysToVirt((void *)(0x4a3 * 1048576), (void *)(0x4a3U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    VirtMem::MapPhysToVirt((void *)(0x550 * 1048576), (void *)(0x550U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);

    TTB_phys = (unsigned int *)PhysPages::FindMultiplePages(256, 8);
    TTB_virt = (unsigned int *)0x10100000;
    if (!VirtMem::MapPhysToVirt(TTB_phys, TTB_virt, 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0))
    	ASSERT(0);*/
#else
    //sd
    VirtMem::MapPhysToVirt((void *)(256 * 1048576), (void *)(0xfefU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    //uart
    VirtMem::MapPhysToVirt((void *)(257 * 1048576), (void *)(0xfd0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
    //timer and primary interrupt controller
    VirtMem::MapPhysToVirt((void *)(0x101U * 1048576), (void *)(0xfeeU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
#endif

    //exception vector, plus the executable high secton
    VirtMem::MapPhysToVirt(PhysPages::FindPage(), VectorTable::GetTableAddress(), 4096, true,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

//    unsigned int tramp_phys = (unsigned int)&__trampoline_start__ - virt_phys_offset;
//    VirtMem::MapPhysToVirt((void *)tramp_phys, (void *)((unsigned int)VectorTable::GetTableAddress() + 0x1000), 4096,
//    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    //executable top section
//    pEntries[4095].section.Init(PhysPages::FindMultiplePages(256, 8),
//    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

  /*  pEntries[4094].section.Init(PhysPages::FindMultiplePages(256, 8),
    			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0);
    memset((void *)(4094U * 1048576), 0, 1048576);


    //copy in the high code
    unsigned char *pHighCode = (unsigned char *)(0xffff0fe0 - 4);
    unsigned char *pHighSource = (unsigned char *)&TlsLow;

    for (unsigned int count = 0; count < (unsigned int)&TlsHigh - (unsigned int)&TlsLow; count++)
    	pHighCode[count] = pHighSource[count];
*/
    //set the emulation value
    *(unsigned int *)(0xffff0fe0 - 4) = 0;
    //set the register value
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (0) : "cc");
    VirtMem::FlushTlb();
}


char *LoadElf(Elf &elf, unsigned int voffset, bool &has_tls, unsigned int &tls_memsize, unsigned int &tls_filesize, unsigned int &tls_vaddr)
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

ProcessFS *pfsA;
ProcessFS *pfsB;
VirtualFS *vfs;

extern "C" void m3_entry(void);

extern "C" void Setup(unsigned int entryPoint)
{
//	VectorTable::SetTableAddress(0);

	MapKernel(entryPoint);

	PrinterUart<PL011> p;
	PL011::EnableFifo(false);
	PL011::EnableUart(true);

	p << "mmu and uart enabled\n";

	__UndefinedInstruction_addr = (unsigned int)&_Und;
	__SupervisorCall_addr = (unsigned int)&_Svc;
	__PrefetchAbort_addr = (unsigned int)&_Prf;
	__DataAbort_addr = (unsigned int)&_Dat;
	__Irq_addr = (unsigned int)&_Irq;
	__Fiq_addr = (unsigned int)&_Fiq;

	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, false);
	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, true);

	/*static const unsigned int qemu_offset = 0x0000c000;
	//static const unsigned int qemu_offset = 0;

	VectorTable::EncodeAndWriteBranch(&__UndefinedInstruction, VectorTable::kUndefinedInstruction, 0xf000b000 + qemu_offset);
	VectorTable::EncodeAndWriteBranch(&__SupervisorCall, VectorTable::kSupervisorCall, 0xf000b000 + qemu_offset);
	VectorTable::EncodeAndWriteBranch(&__PrefetchAbort, VectorTable::kPrefetchAbort, 0xf000b000 + qemu_offset);
	VectorTable::EncodeAndWriteBranch(&__DataAbort, VectorTable::kDataAbort, 0xf000b000 + qemu_offset);
	VectorTable::EncodeAndWriteBranch(&__Irq, VectorTable::kIrq, 0xf000b000 + qemu_offset);
	VectorTable::EncodeAndWriteBranch(&__Fiq, VectorTable::kFiq, 0xf000b000 + qemu_offset);*/

	VectorTable::EncodeAndWriteLiteralLoad(&_Und, VectorTable::kUndefinedInstruction);
	VectorTable::EncodeAndWriteLiteralLoad(&_Svc, VectorTable::kSupervisorCall);
	VectorTable::EncodeAndWriteLiteralLoad(&_Prf, VectorTable::kPrefetchAbort);
	VectorTable::EncodeAndWriteLiteralLoad(&_Dat, VectorTable::kDataAbort);
	VectorTable::EncodeAndWriteLiteralLoad(&_Irq, VectorTable::kIrq);
	VectorTable::EncodeAndWriteLiteralLoad(&_Fiq, VectorTable::kFiq);

	p << "exception table inserted\n";

	EnableFpu(true);

	p << "fpu enabled\n";

	if (!InitMempool((void *)0xa0000000, 256 * 5))		//5MB
		ASSERT(0);

	p << "memory pool initialised\n";

	//lcd2 panel background colour, DISPC_DEFAULT_COLOR2 2706
	*(volatile unsigned int *)0xfc0013ac = 0xffffff;
	//DISPC_CONTROL2[12] overlay optimisation, 2689
	*(volatile unsigned int *)0xfc001238 &= ~(1 << 12);
	//DISPC_CONFIG2[18] alpha blender, 2731
//	*(volatile unsigned int *)0xfc001620 |= (1 << 18);
	//transparency colour key selection DISPC_CONFIG2[11]
	*(volatile unsigned int *)0xfc001620 &= ~(1 << 11);
	//set transparency colour value, DISPC_TRANS_COLOR2 2706
	*(volatile unsigned int *)0xfc0013b0 = 0;
	//transparency colour key disabled DISPC_CONFIG2[10]
	*(volatile unsigned int *)0xfc001620 &= ~(1 << 10);


	//DISPC_POL_FREQ2, 2713
	*(volatile unsigned int *)0xfc001408 = (1 << 14) | (1 << 13) | (1 << 12);

	*(volatile unsigned int *)0xfc001620 |= 1;


	while(1);

#if 0
	extern unsigned int m3_magic;
	p << "magic " << m3_magic << "\n";

//	m3_entry();

	volatile unsigned int *CM_MPU_M3_CLKSTCTRL = (volatile unsigned int *)0x4a008900;		//930
	volatile unsigned int *CM_MPU_M3_MPU_M3_CLKCTRL = (volatile unsigned int *)0x4a008920;	//933
	volatile unsigned int *RM_MPU_M3_RSTCTRL = (volatile unsigned int *)0x4a306910;		//630
	volatile unsigned int *MMU_CNTL = (volatile unsigned int *)0x55082044;		//4464
	volatile unsigned int *MMU_TTB = (volatile unsigned int *)0x5508204c;		//4465

	void *phys_entry = PhysPages::FindMultiplePages(256, 8);
	if (!VirtMem::MapPhysToVirt(phys_entry, (void *)0x10000000, 1048576,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kStronglyOrdered, 0))
		ASSERT(0);

	p << " phys entry " << (unsigned int)phys_entry << "\n";

	memcpy((void *)0x10000000, (const void *)((unsigned int)&m3_entry & ~1), 4096);

	p << "filling table\n";
	union
	{
		TranslationTable::Section s;
		unsigned int i;
	} combined;

	combined.s.Init(phys_entry, TranslationTable::kRwRw, TranslationTable::kExec,
			TranslationTable::kStronglyOrdered, 0);

	for (int count = 0; count < 4096; count++)
		TTB_virt[count] = combined.i;

	combined.s.Init((void *)(0x480 * 1048576),
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kStronglyOrdered, 0);
	TTB_virt[0x480] = combined.i;

	asm volatile ("dsb");

	//sequence 375
//	p << __LINE__ << " " << *CM_MPU_M3_MPU_M3_CLKCTRL << "\n";
	*CM_MPU_M3_MPU_M3_CLKCTRL = 1;
//	p << __LINE__ << " " << *CM_MPU_M3_MPU_M3_CLKCTRL << "\n";

	asm volatile ("dsb");

//	p << __LINE__ << " " << *CM_MPU_M3_CLKSTCTRL << "\n";
	*CM_MPU_M3_CLKSTCTRL = 2;
//	p << __LINE__ << " " << *CM_MPU_M3_CLKSTCTRL << "\n";

	asm volatile ("dsb");

//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";
	*RM_MPU_M3_RSTCTRL = 0;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
	*RM_MPU_M3_RSTCTRL = 2;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
	*RM_MPU_M3_RSTCTRL = 7;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
	*RM_MPU_M3_RSTCTRL = 3;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");

//	p << __LINE__ << " " << *MMU_TTB << "\n";
	*MMU_TTB = (unsigned int)TTB_phys;
//	p << __LINE__ << " " << *MMU_TTB << "\n";

	asm volatile ("dsb");

//	p << __LINE__ << " " << *MMU_CNTL << "\n";
	*MMU_CNTL = 6;
//	p << __LINE__ << " " << *MMU_CNTL << "\n";

	asm volatile ("dsb");
	*RM_MPU_M3_RSTCTRL = 2;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");

//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";
//	*RM_MPU_M3_RSTCTRL = 0;
//	p << __LINE__ << " " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");

//	*CM_MPU_M3_MPU_M3_CLKCTRL = 0x01;
//	*CM_MPU_M3_CLKSTCTRL = 0x02;
//	*RM_MPU_M3_RSTCTRL &= ~0x4;
//	*MMU_TTB = (unsigned int)TTB_phys;
//	*MMU_CNTL = 0x6;
//	*RM_MPU_M3_RSTCTRL &= ~1;

	DelaySecond();

	for (int count = 0; count < 10; count++)
		p << count << " " << *(volatile unsigned int *)(0x10000000 + count * 4) << "\n";

	/*for (int count = 0; count < 10; count++)
	{
		for (int inner = 0; inner < 100; inner++)
			DelayMillisecond();
		p << "magic " << *(volatile unsigned int *)0x1000000c << "\n";
	}*/

	while(1);
#endif


	//enable interrupts
	EnableIrq(true);
	EnableFiq(true);

#ifdef PBES
	pPic = new CortexA9MPCore::GenericInterruptController((volatile unsigned int *)0xfee40100, (volatile unsigned int *)0xfee41000);
	p << "pic is " << pPic << "\n";
	pGpTimer2 = new OMAP4460::GpTimer((volatile unsigned int *)0xfef32000, 0, 2);
	pGpTimer2->SetFrequencyInMicroseconds(1 * 1000 * 1000);
	pGpTimer2->Enable(true);

	pGpTimer10 = new OMAP4460::GpTimer((volatile unsigned int *)0xfef86000, 0, 10);
	pGpTimer10->SetFrequencyInMicroseconds(1 * 1000 * 1000);
	pGpTimer10->Enable(true);

	p << "timer interrupt " << pGpTimer2->GetInterruptNumber() << "\n";
	p << "timer interrupt " << pGpTimer10->GetInterruptNumber() << "\n";

	pPic->RegisterInterrupt(*pGpTimer2, InterruptController::kIrq);
	pPic->RegisterInterrupt(*pGpTimer10, InterruptController::kIrq);

	/*while (1)
	{
		p << pGpTimer2->GetCurrentValue() << " fired " << pGpTimer2->HasInterruptFired() << "\n";
	}*/

	int swi = 0;

	p << "go\n";

//	while(1)
//		pPic->SoftwareInterrupt((swi++) & 0xf);
#else
	pTimer0 = new VersatilePb::SP804((volatile unsigned int *)0xfeee2000, 0);
	pTimer0->SetFrequencyInMicroseconds(10 * 1000);
	pTimer0->Enable(true);

	pTimer2 = new VersatilePb::SP804((volatile unsigned int *)0xfeee3000, 1);
	pTimer2->SetFrequencyInMicroseconds(1 * 1000 * 1000);
	pTimer2->Enable(true);

	pMasterClockClear = (unsigned int)pTimer0->GetFiqClearAddress();
	ASSERT(pMasterClockClear);
	masterClockClearValue = pTimer0->GetFiqClearValue();

	pPic = new VersatilePb::PL190((volatile unsigned int *)0xfee40000);
	pPic->RegisterInterrupt(*pTimer0, InterruptController::kFiq);
	pPic->RegisterInterrupt(*pTimer2, InterruptController::kIrq);
#endif

	while(1);

//	{
#ifdef PBES
		OMAP4460::MMCi sd((volatile void *)(0xfefU * 1048576 + 0x9c000), 1);

		p << "resetting\n";
		if (!sd.Reset())
			p << "reset failed\n";
#else
		VersatilePb::PL181 sd((volatile void *)(0xfefU * 1048576 + 0x5000));
#endif

		p << "go idle state\n";
		sd.GoIdleState();
		p << "go ready state\n";
		if (!sd.GoReadyState())
		{
			p << "no card\n";
			ASSERT(0);
		}
		p << "go identification state\n";
		if (!sd.GoIdentificationState())
		{
			p << "definitely no card\n";
			ASSERT(0);
		}
		p << "get card rca\n";
		unsigned int rca;
		bool ok = sd.GetCardRcaAndGoStandbyState(rca);
		ASSERT(ok);

		p << "go transfer state with rca " << rca << "\n";
		ok = sd.GoTransferState(rca);
		ASSERT(ok);
		p << "finished\n";

		MBR mbr(sd);

		/*unsigned int *buffer = new unsigned int[32768/4];
		ok = sd.ReadDataFromLogicalAddress(508, buffer, 0x5000);
		ASSERT(ok);

		for (int outer = 0; outer < 0x5000 / 64; outer++)
		{
			p << outer * 64 << ": ";
			for (int inner = 0; inner < 16; inner++)
				p << buffer[outer * 16 + inner] << " ";
			p << "\n";
		}

		while(1);*/

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

//		VirtualFS vfs;
//		vfs.Mkdir("/", "Volumes");
//		vfs.Mkdir("/Volumes", "sd");
//
//		FatFS fat(sd);
//
//		vfs.Attach(fat, "/Volumes/sd");
//
////		BaseDirent *b = vfs.Locate("/");
////		b = vfs.Locate("/Volumes");
////		b = vfs.Locate("/Volumes/sd");
////		b = vfs.Locate("/Volumes/sd/Libraries");
////
////		b = vfs.Locate("/Volumes/sd/Libraries/ld-2.15.so");
////
////		b = vfs.Locate("/Volumes/sd/Programs/tester");
////
//		fat.Attach(vfs, "/Programs");
////
////		b = vfs.Locate("/Volumes/sd/Programs/Volumes/sd/Libraries/ld-2.15.so");
//
//		BaseDirent *f = vfs.OpenByName("/Volumes/sd/Programs/Volumes/sd/Libraries/ld-2.17.so", O_RDONLY);
//		ASSERT(f);
//		ProcessFS pfs("/Volumes", "/sd/Programs");
//
//		char string[500];
//		pfs.BuildFullPath("/sd/Programs/Volumes/sd/Libraries/ld-2.17.so", string, 500);
//		pfs.BuildFullPath("Volumes/sd/Libraries/ld-2.17.so", string, 500);
//		pfs.Chdir("Volumes");
//		pfs.Chdir("/sd");
//
//		int fd = pfs.Open(*f);
//		ASSERT(fd >= 0);
//		int fd2 = pfs.Dup(fd);
//		int fd3 = pfs.Dup(fd2);
//		WrappedFile *w = pfs.GetFile(fd2);
//		pfs.Close(fd2);
//		w = pfs.GetFile(fd3);
//		pfs.Close(fd3);
//		pfs.Close(fd);
//		w = pfs.GetFile(fd);
//
//		vfs.Mkdir("/", "Devices");
//
//		PL011 uart;
//		Stdio in(Stdio::kStdin, uart, vfs);
//		vfs.AddOrphanFile("/Devices", in);
//
//		BaseDirent *pIn = vfs.OpenByName("/Devices/stdin", O_WRONLY);
//
//		Stdio out(Stdio::kStdout, uart, vfs);
//		vfs.AddOrphanFile("/Devices", out);
//
//		BaseDirent *pOut = vfs.OpenByName("/Devices/stdout", O_WRONLY);
//		((File *)pOut)->WriteTo("hello", strlen("hello"), 0);
//
//		TTY tty(in, out, vfs);
//		vfs.AddOrphanFile("/Devices", tty);
//
//		int ld = pfs.Open(*vfs.OpenByName(pfs.BuildFullPath("/sd/Programs/Volumes/sd/Libraries/ld-2.15.so", string, 500),
//				O_RDONLY));
//
//		char elf_header[100];
//		pfs.GetFile(ld)->Read(elf_header, 100);

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

	p << "creating VFS\n";
	vfs = new VirtualFS();
	//////
	vfs->Mkdir("/", "Devices");
	PL011 uart;
	Stdio in(Stdio::kStdin, uart, *vfs);
	vfs->AddOrphanFile("/Devices", in);

	Stdio out(Stdio::kStdout, uart, *vfs);
	vfs->AddOrphanFile("/Devices", out);

	Stdio err(Stdio::kStderr, uart, *vfs);
	vfs->AddOrphanFile("/Devices", err);
	////
	vfs->Mkdir("/", "Volumes");
	vfs->Mkdir("/Volumes", "sd");

	p << "creating FAT\n";

#ifdef PBES
	FatFS fat(*mbr.GetPartition(0));
#else
	FatFS fat(sd);
#endif

	p << "attaching FAT\n";
	vfs->Attach(fat, "/Volumes/sd");

	pfsA = new ProcessFS("/Volumes/sd/minimal", "/");
	char string[500];

	ASSERT(pfsA->Open(*vfs->OpenByName("/Devices/stdin", O_RDONLY)) == 0);
	ASSERT(pfsA->Open(*vfs->OpenByName("/Devices/stdout", O_RDONLY)) == 1);
	ASSERT(pfsA->Open(*vfs->OpenByName("/Devices/stderr", O_RDONLY)) == 2);

	int exe = pfsA->Open(*vfs->OpenByName(pfsA->BuildFullPath("/bin/busybox", string, 500),
			O_RDONLY));
	int ld = pfsA->Open(*vfs->OpenByName(pfsA->BuildFullPath("/lib/ld-linux.so.3", string, 500),
			O_RDONLY));

	pfsB = new ProcessFS("/Volumes/sd", "/");


//	char string[500];

//	int exe = pfsB.Open(*vfs.OpenByName(pfsB.BuildFullPath("/Programs/tester", string, 500),
//			O_RDONLY));
//	int ld = pfsB->Open(*vfs->OpenByName(pfsB->BuildFullPath("/Libraries/ld_stripped.so", string, 500),
//			O_RDONLY));

//	char elf_header[100];
	stat64 exe_stat, ld_stat;
	pfsA->GetFile(exe)->Fstat(exe_stat);
	pfsA->GetFile(ld)->Fstat(ld_stat);

	unsigned char *pProgramData, *pInterpData;
	pProgramData = new unsigned char[exe_stat.st_size];
	ASSERT(pProgramData);
	pInterpData = new unsigned char[ld_stat.st_size];
	ASSERT(pInterpData);

	pfsA->GetFile(exe)->Read(pProgramData, exe_stat.st_size);
	pfsA->GetFile(ld)->Read(pInterpData, ld_stat.st_size);

	Elf startingElf, interpElf;

	startingElf.Load(pProgramData, exe_stat.st_size);

	interpElf.Load(pInterpData, ld_stat.st_size);

	bool has_tls = false;
	unsigned int tls_memsize, tls_filesize, tls_vaddr;

	//////////////////////////////////////
	LoadElf(interpElf, 0x70000000, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	ASSERT(has_tls == false);
	char *pInterpName = LoadElf(startingElf, 0, has_tls, tls_memsize, tls_filesize, tls_vaddr);
	SetHighBrk((void *)0x10000000);
	//////////////////////////////////////

	RfeData rfe;
//	rfe.m_pPc = &_start;
	if (pInterpName)
		rfe.func = (void *)((unsigned int)interpElf.GetEntryPoint() + 0x70000000);
	else
		rfe.func = (void *)((unsigned int)startingElf.GetEntryPoint());
	rfe.m_pSp = (unsigned int *)0xffeffff8;		//4095MB-8b

	memset(&rfe.m_cpsr, 0, 4);
	rfe.m_cpsr.m_mode = kUser;
	rfe.m_cpsr.m_a = 1;
	rfe.m_cpsr.m_i = 0;
	rfe.m_cpsr.m_f = 0;

	if ((unsigned int)rfe.m_pPc & 1)		//thumb
		rfe.m_cpsr.m_t = 1;

	//move down enough for some stuff
	rfe.m_pSp = rfe.m_pSp - 128;
	//fill in argc
	rfe.m_pSp[0] = 3;
	//fill in argv
	const char *pElfName = "busybox";
//	const char *pElfName = "pwd";
	const char *pEnv = "LAD_DEBUG=all";

	ElfW(auxv_t) *pAuxVec = (ElfW(auxv_t) *)&rfe.m_pSp[7];
	unsigned int aux_size = sizeof(ElfW(auxv_t)) * 6;

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

	pAuxVec[4].a_type = AT_PAGESZ;
	pAuxVec[4].a_un.a_val = 4096;

	pAuxVec[5].a_type = AT_NULL;
	pAuxVec[5].a_un.a_val = 0;

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
	unsigned int b = (unsigned int)strcpy((char *)text_start_addr + strlen(pEnv) + 1 + strlen(pElfName) + 1, "ls");
	unsigned int c = (unsigned int)strcpy((char *)text_start_addr + strlen(pEnv) + 1 + strlen(pElfName) + 1 + strlen("ls") + 1, "-l");

	rfe.m_pSp[1] = a;
	rfe.m_pSp[2] = b;
	rfe.m_pSp[3] = c;
	rfe.m_pSp[4] = 0;
	rfe.m_pSp[5] = e;
	rfe.m_pSp[6] = 0;

	ChangeModeAndJump(0, 0, 0, &rfe);
}
