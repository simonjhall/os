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
#include "Scheduler.h"
#include "GPIO.h"

int main(int argc, const char **argv);

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <fcntl.h>

const unsigned int initial_stack_size = 1024;
unsigned int initial_stack[initial_stack_size];
unsigned int initial_stack_end = (unsigned int)&initial_stack[initial_stack_size];

extern "C" void EnableIrq(bool);
extern "C" void EnableFiq(bool);
extern "C" bool EnableIrqFiqForMode(Mode, bool, bool);

PeriodicTimer *pTimer0 = 0;
PeriodicTimer *pTimer1 = 0;

InterruptController *pPic = 0;
extern unsigned int pMasterClockClear, masterClockClearValue, master_clock;

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
//	static unsigned int clock = 0;
//	PrinterUart<PL011> p;
//	p << "irq " << clock++ << " time " << Formatter<float>(master_clock / 100.0f, 2) << "\n";
//	p << "pic is " << pPic << "\n";

	InterruptSource *pSource;

	ASSERT(pPic);
	while ((pSource = pPic->WhatHasFired()))
	{
		//todo add to list
//		p << "interrupt from " << pSource << " # " << pSource->GetInterruptNumber() << "\n";
		pSource->HandleInterrupt();
	}
//	p << "returning from irq\n";
}


/////////////////////////////////////////////////

extern "C" void _Fiq(void);

extern "C" void InvokeSyscall(unsigned int r7);
extern "C" void ChangeModeAndJump(unsigned int r0, unsigned int r1, unsigned int r2, RfeData *);

extern "C" void _start(void);


extern unsigned int TlsLow, TlsHigh;
extern unsigned int thread_section_begin, thread_section_end, thread_section_mid;

extern unsigned int entry_maybe_misaligned;
extern unsigned int __end__;


extern unsigned int __trampoline_start__;
extern unsigned int __trampoline_end__;

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

    //use the zero lo
    VirtMem::SetL1TableLo(0);

    //IO sections
#ifdef PBES
    //L4 PER
    VirtMem::MapPhysToVirt((void *)(0x480U * 1048576), (void *)(0xfefU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    //private A9 memory
    VirtMem::MapPhysToVirt((void *)(0x482U * 1048576), (void *)(0xfeeU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    //UART - DUP!
    VirtMem::MapPhysToVirt((void *)(0x480 * 1048576), (void *)(0xfd0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);


    VirtMem::MapPhysToVirt((void *)(0x4a1 * 1048576), (void *)(0xe00U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    VirtMem::MapPhysToVirt((void *)(0x4a3 * 1048576), (void *)(0xe01U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    VirtMem::MapPhysToVirt((void *)(0x4a0 * 1048576), (void *)(0xe02U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);

    //display
    for (int count = 0; count < 16; count++)
		VirtMem::MapPhysToVirt((void *)((0x580 + count) * 1048576), (void *)((0xfc0U + count) * 1048576), 1048576, true,
				TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
/*
    VirtMem::MapPhysToVirt((void *)(0x4a0 * 1048576), (void *)(0x4a0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    VirtMem::MapPhysToVirt((void *)(0x4a3 * 1048576), (void *)(0x4a3U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    VirtMem::MapPhysToVirt((void *)(0x550 * 1048576), (void *)(0x550U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);

    TTB_phys = (unsigned int *)PhysPages::FindMultiplePages(256, 8);
    TTB_virt = (unsigned int *)0x10100000;
    if (!VirtMem::MapPhysToVirt(TTB_phys, TTB_virt, 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0))
    	ASSERT(0);*/
#else
    //sd
    VirtMem::MapPhysToVirt((void *)(256 * 1048576), (void *)(0xfefU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    //uart
    VirtMem::MapPhysToVirt((void *)(257 * 1048576), (void *)(0xfd0U * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
    //timer and primary interrupt controller
    VirtMem::MapPhysToVirt((void *)(0x101U * 1048576), (void *)(0xfeeU * 1048576), 1048576, true,
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
#endif

    //exception vector, plus the executable high secton
    VirtMem::MapPhysToVirt(PhysPages::FindPage(), VectorTable::GetTableAddress(), 4096, true,
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    //executable top section
    VirtMem::MapPhysToVirt(PhysPages::FindPage(), (void *)0xffff0000, 4096, true,
    			TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

    memset((void *)0xffff0000, 0, 4096);


    //copy in the high code
    unsigned char *pHighCode = (unsigned char *)(0xffff0fe0 - 4);
    unsigned char *pHighSource = (unsigned char *)&TlsLow;

    for (unsigned int count = 0; count < (unsigned int)&TlsHigh - (unsigned int)&TlsLow; count++)
    	pHighCode[count] = pHighSource[count];

    //set the emulation value
    *(unsigned int *)(0xffff0fe0 - 4) = 0;
    //set the register value
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (0) : "cc");
    VirtMem::FlushTlb();
}

VirtualFS *vfs;

extern "C" void m3_entry(void);

namespace Dispcc
{
	volatile unsigned int *DISPC_REVISION = (volatile unsigned int *)0xFC001000;
	volatile unsigned int *DISPC_SYSCONFIG = (volatile unsigned int *)0xFC001010;
	volatile unsigned int *DISPC_SYSSTATUS = (volatile unsigned int *)0xFC001014;
	volatile unsigned int *DISPC_IRQSTATUS = (volatile unsigned int *)0xFC001018;
	volatile unsigned int *DISPC_IRQENABLE = (volatile unsigned int *)0xFC00101C;
	volatile unsigned int *DISPC_CONTROL1  = (volatile unsigned int *)0xFC001040;
	volatile unsigned int *DISPC_CONFIG1 = (volatile unsigned int *)0xFC001044;
	volatile unsigned int *DISPC_DEFAULT_COLOR0 = (volatile unsigned int *)0xFC00104C;
	volatile unsigned int *DISPC_DEFAULT_COLOR1 = (volatile unsigned int *)0xFC001050;
	volatile unsigned int *DISPC_TRANS_COLOR0 = (volatile unsigned int *)0xFC001054;
	volatile unsigned int *DISPC_TRANS_COLOR1 = (volatile unsigned int *)0xFC001058;
	volatile unsigned int *DISPC_LINE_STATUS = (volatile unsigned int *)0xFC00105C;
	volatile unsigned int *DISPC_LINE_NUMBER = (volatile unsigned int *)0xFC001060;
	volatile unsigned int *DISPC_TIMING_H1 = (volatile unsigned int *)0xFC001064;
	volatile unsigned int *DISPC_TIMING_V1 = (volatile unsigned int *)0xFC001068;
	volatile unsigned int *DISPC_POL_FREQ1 = (volatile unsigned int *)0xFC00106C;
	volatile unsigned int *DISPC_DIVISOR1 = (volatile unsigned int *)0xFC001070;
	volatile unsigned int *DISPC_GLOBAL_ALPHA = (volatile unsigned int *)0xFC001074;
	volatile unsigned int *DISPC_SIZE_TV = (volatile unsigned int *)0xFC001078;
	volatile unsigned int *DISPC_SIZE_LCD1 = (volatile unsigned int *)0xFC00107C;

	volatile unsigned int *DISPC_GFX_BA_0 = (volatile unsigned int *)(0xFC001080 + 4 * 0);
	volatile unsigned int *DISPC_GFX_BA_1 = (volatile unsigned int *)(0xFC001080 + 4 * 1);
	volatile unsigned int *DISPC_GFX_POSITION = (volatile unsigned int *)0xFC001088;
	volatile unsigned int *DISPC_GFX_SIZE = (volatile unsigned int *)0xFC00108C;
	volatile unsigned int *DISPC_GFX_ATTRIBUTES = (volatile unsigned int *)0xFC0010A0;
	volatile unsigned int *DISPC_GFX_BUF_THRESHOLD = (volatile unsigned int *)0xFC0010A4;
	volatile unsigned int *DISPC_GFX_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC0010A8;
	volatile unsigned int *DISPC_GFX_ROW_INC = (volatile unsigned int *)0xFC0010AC;
	volatile unsigned int *DISPC_GFX_PIXEL_INC = (volatile unsigned int *)0xFC0010B0;
	volatile unsigned int *DISPC_GFX_TABLE_BA = (volatile unsigned int *)0xFC0010B8;

	volatile unsigned int *DISPC_VID1_BA_0 = (volatile unsigned int *)(0xFC0010BC + 4 * 0);
	volatile unsigned int *DISPC_VID1_BA_1 = (volatile unsigned int *)(0xFC0010BC + 4 * 1);
	volatile unsigned int *DISPC_VID1_POSITION = (volatile unsigned int *)0xFC0010C4;
	volatile unsigned int *DISPC_VID1_SIZE = (volatile unsigned int *)0xFC0010C8;
	volatile unsigned int *DISPC_VID1_ATTRIBUTES = (volatile unsigned int *)0xFC0010CC;
	volatile unsigned int *DISPC_VID1_BUF_THRESHOLD = (volatile unsigned int *)0xFC0010D0;
	volatile unsigned int *DISPC_VID1_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC0010D4;
	volatile unsigned int *DISPC_VID1_ROW_INC = (volatile unsigned int *)0xFC0010D8;
	volatile unsigned int *DISPC_VID1_PIXEL_INC = (volatile unsigned int *)0xFC0010DC;
	volatile unsigned int *DISPC_VID1_FIR = (volatile unsigned int *)0xFC0010E0;
	volatile unsigned int *DISPC_VID1_PICTURE_SIZE = (volatile unsigned int *)0xFC0010E4;
	volatile unsigned int *DISPC_VID1_ACCU_0 = (volatile unsigned int *)(0xFC0010E8 + 4 * 0);
	volatile unsigned int *DISPC_VID1_ACCU_1 = (volatile unsigned int *)(0xFC0010E8 + 4 * 1);
	volatile unsigned int *DISPC_VID1_FIR_COEF_H_0 = (volatile unsigned int *)(0xFC0010F0 + 8 * 0);
	volatile unsigned int *DISPC_VID1_FIR_COEF_H_1 = (volatile unsigned int *)(0xFC0010F0 + 8 * 1);
	volatile unsigned int *DISPC_VID1_FIR_COEF_HV_0 = (volatile unsigned int *)(0xFC0010F4 + 8 * 0);
	volatile unsigned int *DISPC_VID1_FIR_COEF_HV_1 = (volatile unsigned int *)(0xFC0010F4 + 8 * 1);
//	volatile unsigned int *DISPC_VID1_CONV_COEF0 = (volatile unsigned int *)0xFC001130;
//	volatile unsigned int *DISPC_VID1_CONV_COEF1 = (volatile unsigned int *)0xFC001134;
//	volatile unsigned int *DISPC_VID1_CONV_COEF2 = (volatile unsigned int *)0xFC001138;
//	volatile unsigned int *DISPC_VID1_CONV_COEF3 = (volatile unsigned int *)0xFC00113C;
//	volatile unsigned int *DISPC_VID1_CONV_COEF4 = (volatile unsigned int *)0xFC001140;

	volatile unsigned int *DISPC_VID2_BA_0 = (volatile unsigned int *)(0xFC00114C + 4 * 0);
	volatile unsigned int *DISPC_VID2_BA_1 = (volatile unsigned int *)(0xFC00114C + 4 * 1);
	volatile unsigned int *DISPC_VID2_POSITION = (volatile unsigned int *)0xFC001154;
	volatile unsigned int *DISPC_VID2_SIZE = (volatile unsigned int *)0xFC001158;
	volatile unsigned int *DISPC_VID2_ATTRIBUTES = (volatile unsigned int *)0xFC00115C;
	volatile unsigned int *DISPC_VID2_BUF_THRESHOLD = (volatile unsigned int *)0xFC001160;
	volatile unsigned int *DISPC_VID2_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001164;
	volatile unsigned int *DISPC_VID2_ROW_INC = (volatile unsigned int *)0xFC001168;
	volatile unsigned int *DISPC_VID2_PIXEL_INC = (volatile unsigned int *)0xFC00116C;
	volatile unsigned int *DISPC_VID2_FIR = (volatile unsigned int *)0xFC001170;
	volatile unsigned int *DISPC_VID2_PICTURE_SIZE = (volatile unsigned int *)0xFC001174;
	volatile unsigned int *DISPC_VID2_ACCU_0 = (volatile unsigned int *)(0xFC001178 + 4 * 0);
	volatile unsigned int *DISPC_VID2_ACCU_1 = (volatile unsigned int *)(0xFC001178 + 4 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_H_0 = (volatile unsigned int *)(0xFC001180 + 8 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_H_1 = (volatile unsigned int *)(0xFC001180 + 8 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_HV_0 = (volatile unsigned int *)(0xFC001184 + 8 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_HV_1 = (volatile unsigned int *)(0xFC001184 + 8 * 1);

	volatile unsigned int *DISPC_VID2_CONV_COEF0 = (volatile unsigned int *)0xFC0011C0;
	volatile unsigned int *DISPC_VID2_CONV_COEF1 = (volatile unsigned int *)0xFC0011C4;
	volatile unsigned int *DISPC_VID2_CONV_COEF2 = (volatile unsigned int *)0xFC0011C8;
	volatile unsigned int *DISPC_VID2_CONV_COEF3 = (volatile unsigned int *)0xFC0011CC;
	volatile unsigned int *DISPC_VID2_CONV_COEF4 = (volatile unsigned int *)0xFC0011D0;

	volatile unsigned int *DISPC_DATA1_CYCLE1 = (volatile unsigned int *)0xFC0011D4;
	volatile unsigned int *DISPC_DATA1_CYCLE2 = (volatile unsigned int *)0xFC0011D8;
	volatile unsigned int *DISPC_DATA1_CYCLE3 = (volatile unsigned int *)0xFC0011DC;

//	volatile unsigned int *DISPC_VID1_FIR_COEF_V_0 = (volatile unsigned int *)(0xFC0011E0 + 4 * 0);
//	volatile unsigned int *DISPC_VID1_FIR_COEF_V_1 = (volatile unsigned int *)(0xFC0011E0 + 4 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_V_0 = (volatile unsigned int *)(0xFC001200 + 4 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_V_1 = (volatile unsigned int *)(0xFC001200 + 4 * 1);

	volatile unsigned int *DISPC_CPR1_COEF_R = (volatile unsigned int *)0xFC001220;
	volatile unsigned int *DISPC_CPR1_COEF_G = (volatile unsigned int *)0xFC001224;
	volatile unsigned int *DISPC_CPR1_COEF_B = (volatile unsigned int *)0xFC001228;

	volatile unsigned int *DISPC_GFX_PRELOAD = (volatile unsigned int *)0xFC00122C;
	volatile unsigned int *DISPC_VID1_PRELOAD = (volatile unsigned int *)0xFC001230;
	volatile unsigned int *DISPC_VID2_PRELOAD = (volatile unsigned int *)0xFC001234;

	volatile unsigned int *DISPC_CONTROL2 = (volatile unsigned int *)0xFC001238;

	volatile unsigned int *DISPC_VID3_ACCU_0 = (volatile unsigned int *)(0xFC001300 + 4 * 0);
	volatile unsigned int *DISPC_VID3_ACCU_1 = (volatile unsigned int *)(0xFC001300 + 4 * 1);
	volatile unsigned int *DISPC_VID3_BA_0 = (volatile unsigned int *)(0xFC001308 + 4 * 0);
	volatile unsigned int *DISPC_VID3_BA_1 = (volatile unsigned int *)(0xFC001308 + 4 * 1);
	volatile unsigned int *DISPC_VID3_ATTRIBUTES = (volatile unsigned int *)0xFC001370;
	volatile unsigned int *DISPC_VID3_CONV_COEF0 = (volatile unsigned int *)0xFC001374;
	volatile unsigned int *DISPC_VID3_CONV_COEF1 = (volatile unsigned int *)0xFC001378;
	volatile unsigned int *DISPC_VID3_CONV_COEF2 = (volatile unsigned int *)0xFC00137C;
	volatile unsigned int *DISPC_VID3_CONV_COEF3 = (volatile unsigned int *)0xFC001380;
	volatile unsigned int *DISPC_VID3_CONV_COEF4 = (volatile unsigned int *)0xFC001384;
	volatile unsigned int *DISPC_VID3_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001388;
	volatile unsigned int *DISPC_VID3_BUF_THRESHOLD = (volatile unsigned int *)0xFC00138C;
	volatile unsigned int *DISPC_VID3_FIR = (volatile unsigned int *)0xFC001390;
	volatile unsigned int *DISPC_VID3_PICTURE_SIZE = (volatile unsigned int *)0xFC001394;
	volatile unsigned int *DISPC_VID3_PIXEL_INC = (volatile unsigned int *)0xFC001398;
	volatile unsigned int *DISPC_VID3_POSITION = (volatile unsigned int *)0xFC00139C;
	volatile unsigned int *DISPC_VID3_PRELOAD = (volatile unsigned int *)0xFC0013A0;
	volatile unsigned int *DISPC_VID3_ROW_INC = (volatile unsigned int *)0xFC0013A4;
	volatile unsigned int *DISPC_VID3_SIZE = (volatile unsigned int *)0xFC0013A8;
	volatile unsigned int *DISPC_DEFAULT_COLOR2 = (volatile unsigned int *)0xFC0013AC;
	volatile unsigned int *DISPC_TRANS_COLOR2 = (volatile unsigned int *)0xFC0013B0;
	volatile unsigned int *DISPC_CPR2_COEF_B = (volatile unsigned int *)0xFC0013B4;
	volatile unsigned int *DISPC_CPR2_COEF_G = (volatile unsigned int *)0xFC0013B8;
	volatile unsigned int *DISPC_CPR2_COEF_R = (volatile unsigned int *)0xFC0013BC;
	volatile unsigned int *DISPC_DATA2_CYCLE1 = (volatile unsigned int *)0xFC0013C0;
	volatile unsigned int *DISPC_DATA2_CYCLE2 = (volatile unsigned int *)0xFC0013C4;
	volatile unsigned int *DISPC_DATA2_CYCLE3 = (volatile unsigned int *)0xFC0013C8;
	volatile unsigned int *DISPC_SIZE_LCD2 = (volatile unsigned int *)0xFC0013CC;
	volatile unsigned int *DISPC_TIMING_H2 = (volatile unsigned int *)0xFC001400;
	volatile unsigned int *DISPC_TIMING_V2 = (volatile unsigned int *)0xFC001404;
	volatile unsigned int *DISPC_POL_FREQ2 = (volatile unsigned int *)0xFC001408;
	volatile unsigned int *DISPC_DIVISOR2 = (volatile unsigned int *)0xFC00140C;

	volatile unsigned int *DISPC_WB_ACCU_0 = (volatile unsigned int *)(0xFC001500 + 4 * 0);
	volatile unsigned int *DISPC_WB_ACCU_1 = (volatile unsigned int *)(0xFC001500 + 4 * 1);
	volatile unsigned int *DISPC_WB_BA_0 = (volatile unsigned int *)(0xFC001508 + 4 * 0);
	volatile unsigned int *DISPC_WB_BA_1 = (volatile unsigned int *)(0xFC001508 + 4 * 1);
	volatile unsigned int *DISPC_WB_ATTRIBUTES = (volatile unsigned int *)0xFC001570;
	volatile unsigned int *DISPC_WB_CONV_COEF0 = (volatile unsigned int *)0xFC001574;
	volatile unsigned int *DISPC_WB_CONV_COEF1 = (volatile unsigned int *)0xFC001578;
	volatile unsigned int *DISPC_WB_CONV_COEF2 = (volatile unsigned int *)0xFC00157C;
	volatile unsigned int *DISPC_WB_CONV_COEF3 = (volatile unsigned int *)0xFC001580;
	volatile unsigned int *DISPC_WB_CONV_COEF4 = (volatile unsigned int *)0xFC001584;
	volatile unsigned int *DISPC_WB_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001588;
	volatile unsigned int *DISPC_WB_BUF_THRESHOLD = (volatile unsigned int *)0xFC00158C;
	volatile unsigned int *DISPC_WB_FIR = (volatile unsigned int *)0xFC001590;
	volatile unsigned int *DISPC_WB_PICTURE_SIZE = (volatile unsigned int *)0xFC001594;
	volatile unsigned int *DISPC_WB_PIXEL_INC = (volatile unsigned int *)0xFC001598;
	volatile unsigned int *DISPC_WB_ROW_INC = (volatile unsigned int *)0xFC0015A4;
	volatile unsigned int *DISPC_WB_SIZE = (volatile unsigned int *)0xFC0015A8;

	volatile unsigned int *DISPC_VID1_BA_UV_0 = (volatile unsigned int *)(0xFC001600 + 4 * 0);
	volatile unsigned int *DISPC_VID1_BA_UV_1 = (volatile unsigned int *)(0xFC001600 + 4 * 1);
	volatile unsigned int *DISPC_VID2_BA_UV_0 = (volatile unsigned int *)(0xFC001608 + 4 * 0);
	volatile unsigned int *DISPC_VID2_BA_UV_1 = (volatile unsigned int *)(0xFC001608 + 4 * 1);
	volatile unsigned int *DISPC_VID3_BA_UV_0 = (volatile unsigned int *)(0xFC001610 + 4 * 0);
	volatile unsigned int *DISPC_VID3_BA_UV_1 = (volatile unsigned int *)(0xFC001610 + 4 * 1);
	volatile unsigned int *DISPC_WB_BA_UV_0 = (volatile unsigned int *)(0xFC001618 + 4 * 0);
	volatile unsigned int *DISPC_WB_BA_UV_1 = (volatile unsigned int *)(0xFC001618 + 4 * 1);

	volatile unsigned int *DISPC_CONFIG2 = (volatile unsigned int *)0xFC001620;
	volatile unsigned int *DISPC_VID1_ATTRIBUTES2 = (volatile unsigned int *)0xFC001624;
	volatile unsigned int *DISPC_VID2_ATTRIBUTES2 = (volatile unsigned int *)0xFC001628;
	volatile unsigned int *DISPC_VID3_ATTRIBUTES2 = (volatile unsigned int *)0xFC00162C;
	volatile unsigned int *DISPC_GAMMA_TABLE0 = (volatile unsigned int *)0xFC001630;
	volatile unsigned int *DISPC_GAMMA_TABLE1 = (volatile unsigned int *)0xFC001634;
	volatile unsigned int *DISPC_GAMMA_TABLE2 = (volatile unsigned int *)0xFC001638;
	volatile unsigned int *DISPC_VID1_FIR2 = (volatile unsigned int *)0xFC00163C;

	volatile unsigned int *DISPC_GLOBAL_BUFFER = (volatile unsigned int *)0xFC001800;
	volatile unsigned int *DISPC_DIVISOR = (volatile unsigned int *)0xFC001804;
	volatile unsigned int *DISPC_WB_ATTRIBUTES2 = (volatile unsigned int *)0xFC001810;
}

OMAP4460::GPIO *g_pGpios[6];
inline bool PinToGpio(unsigned int gpio, unsigned int &outGpio, unsigned int &outPin)
{
	int want_pin = gpio;
	int want_gpio = want_pin / 32;
	want_pin %= 32;

	outGpio = want_gpio;
	outPin = want_pin;

	return true;
}

OMAP4460::GPIO::Pin &GetPin(unsigned int p)
{
	unsigned int gpio, pin;
	ASSERT(PinToGpio(p, gpio, pin));

	return g_pGpios[gpio]->GetPin(pin);
}

unsigned int conv_u(unsigned int u)
{
	if ((u >> 20) == 0xe02)
		return (u & 1048575) | (0x4a0 << 20);
	else if ((u >> 20) == 0xe01)
		return (u & 1048575) | (0x4a3 << 20);
	else
		ASSERT(0);
}

extern "C" void Setup(unsigned int entryPoint)
{
	MapKernel(entryPoint);


	PrinterUart<PL011> p;
	PL011::EnableFifo(false);
	PL011::EnableUart(true);

	p << "mmu and uart enabled\n";

	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, false);
	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, true);

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


#ifdef PBES__
	p << "DeviceName OMAP4460_ES1.x\n";
	unsigned int u;

	u = 0xe0204100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204120; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204124; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204128; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020412c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204130; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204134; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204138; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020413c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204140; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204144; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204148; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020414c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204150; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204160; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204164; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204168; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020416c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204170; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204188; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020418c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020419c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041ac; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041b8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041bc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041c8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041cc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041dc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041ec; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041f0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041f4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020420c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204260; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204264; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204270; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204280; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204404; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204408; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204420; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204500; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204520; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204528; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204530; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204538; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204540; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204548; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204550; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204558; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204560; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204568; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204570; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204578; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204580; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204588; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208114; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208118; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020811c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208124; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208128; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020812c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208130; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208138; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208140; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208144; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208148; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020814c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208150; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208154; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208158; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020815c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208160; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208164; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208180; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208184; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208188; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020818c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208190; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02081b4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208600; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208628; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208630; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208638; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208640; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208700; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208708; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208720; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208800; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208808; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208820; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208828; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208830; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208900; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208904; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208908; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208920; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b38; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b40; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d38; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e40; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209000; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209004; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209008; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209020; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209028; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209120; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209200; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209204; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209220; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209300; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209304; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209308; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209328; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209330; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209338; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209358; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209360; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209368; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02093d0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02093e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209408; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209428; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209430; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209438; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209440; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209448; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209450; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209458; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209460; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209468; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209470; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209478; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209480; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209488; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094a0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094a8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094b0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094b8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094c0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094f0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094f8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209500; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209508; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209520; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209528; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209538; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209540; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209548; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209550; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209558; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209560; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0106100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0106108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010610c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0106110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107800; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107820; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107830; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107838; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107840; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107850; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107860; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107878; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107888; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a000; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a11c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a204; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a210; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a214; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a218; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a21c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a220; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a224; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a234; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a310; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a314; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a318; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a31c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a320; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a324; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a41c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a420; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a510; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a514; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a51c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	while(1);
#endif

//	p << "was " << *(volatile unsigned int *)0xe020815c << "\n";
//	*(volatile unsigned int *)0xe020815c = 0x21e;
//	DelaySecond();
//	p << "now " << *(volatile unsigned int *)0xe020815c << "\n";
//	DelaySecond();

#ifdef PBES
	volatile unsigned int *pl310 = (volatile unsigned int *)0xfee42000;
	p << pl310[0] << "\n";
	p << pl310[4 >> 2] << "\n";
	p << pl310[0x100 >> 2] << "\n";
	p << pl310[0x104 >> 2] << "\n";

	unsigned int r0, r1;
	r0 = 1;
	r1 = 0;
	VirtMem::Omap4460::OmapSmc(&r0, &r1, VirtMem::Omap4460::kL2EnableDisableCache);

	p << pl310[0] << "\n";
	p << pl310[4 >> 2] << "\n";
	p << pl310[0x100 >> 2] << "\n";
	p << pl310[0x104 >> 2] << "\n";

//	while(1);
#endif

#ifdef PBES
	//led pin mux
	*(volatile unsigned int *)0xe00000fc = 0x11b;
	for (int count = 2; count < 7; count++)
		g_pGpios[count - 1] = new OMAP4460::GPIO((volatile unsigned int *)(0xfef55000 + (count - 2) * 0x2000), count);

	g_pGpios[0] = new OMAP4460::GPIO((volatile unsigned int *)(0xe0110000), 1);


	auto gpio0 = GetPin(0);
	auto gpio110 = GetPin(110);
	auto gpio122 = GetPin(122);
	gpio0.SetAs(OMAP4460::GPIO::kOutput);
	gpio110.SetAs(OMAP4460::GPIO::kOutput);
	gpio122.SetAs(OMAP4460::GPIO::kInput);

	gpio0.Write(true);
/*	bool flash = false;

	while (1)
	{
		p << gpio122.Read() << "\n";
		gpio110.Write(flash);
		flash = !flash;
		DelaySecond();
	}

	while(1)
	{
		for (int count = 1; count < 7; count++)
		{
			p.PrintDec((count - 1) * 32, false);
			p << " " << g_pGpios[count - 1]->Read() << " ";
		}
		p << "\n";
		DelaySecond();
	}*/
#endif

#if 1
	/*const int width = 800;
	const int horiz_front_porch = 40;
	const int horiz_sync = 128;
	const int horiz_back_porch = 88;
	const bool horiz_pol = false;			//false = +ve

	const int height = 600;
	const int vert_front_porch = 1;
	const int vert_sync = 4;
	const int vert_back_porch = 23;
	const bool vert_pol = false;

	*(volatile unsigned int *)0xe020815c = 0x204;
	const int div1 = 2;
	const int div2 = 5;*/

	const int width = 640;
	const int horiz_front_porch = 16;
	const int horiz_sync = 96;
	const int horiz_back_porch = 48;
	const bool horiz_pol = true;			//false = +ve

	const int height = 480;
	const int vert_front_porch = 11;
	const int vert_sync = 2;
	const int vert_back_porch = 31;
	const bool vert_pol = true;

	*(volatile unsigned int *)0xe020815c = 0x21e;
	const int div1 = 2;
	const int div2 = 1;

	/*const int width = 1920;
	const int horiz_front_porch = 44;
	const int horiz_sync = 88;
	const int horiz_back_porch = 148;
	const bool horiz_pol = true;			//false = +ve, true = -ve

	const int height = 1034;
	const int vert_front_porch = 5;
	const int vert_sync = 5;
	const int vert_back_porch = 35;
	const bool vert_pol = true;

	const int div = 2;
	bool interlace = true;*/

	//dma configuration
	*Dispcc::DISPC_GFX_BA_0 = (unsigned int)PhysPages::FindMultiplePages(1024, 0);
//	*Dispcc::DISPC_GFX_BA_1 = (unsigned int)PhysPages::FindMultiplePages(1024, 0);			//not necessary?

//	*Dispcc::DISPC_GFX_PIXEL_INC = 4;
//	*Dispcc::DISPC_GFX_ROW_INC = 5;

	unsigned int phys = *Dispcc::DISPC_GFX_BA_0;
	unsigned int virt = 0xd0000000;

	void *plut = PhysPages::FindPage();
	unsigned int *vlut = (unsigned int *)(virt + 1024 * 4096);

	for (int count = 0; count < 1024; count++)
		ASSERT(VirtMem::MapPhysToVirt((void *)(phys + count * 4096),
				(void *)(virt + count * 4096),
				4096,
				true, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerNc, 0));

	ASSERT(VirtMem::MapPhysToVirt((void *)plut,
				(void *)(virt + 1024 * 4096),
				4096,
				true, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerNc, 0));

	unsigned int *fb = (unsigned int *)0xd0000000;
	/*for (int count = 0; count < 640 * 480; count++)
//		fb[count] = (0x4000 * count) & 0xff00;
		fb[count] = 0xffffffff;*/

	memset(fb, 0, width * height * 4);
	memset(vlut, 0xff, 4096);

	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			fb[y * width + x] = (unsigned char)x | ((unsigned char)y << 8);

	for (int y = 0; y < height; y += 16)
		for (int x = 0; x < width; x++)
		{
			fb[y * width + x] = 0xffffffff;
		}

	for (int x = 0; x < width; x += 16)
		for (int y = 0; y < height; y++)
		{
			fb[y * width + x] = 0xffffffff;
		}

	/*for (int count = 0; count < 256; count++)
	{
		*Dispcc::DISPC_GAMMA_TABLE0 = (count << 24) | 0xffffff;
		*Dispcc::DISPC_GAMMA_TABLE1 = (count << 24) | 0xffffff;
		*Dispcc::DISPC_GAMMA_TABLE2 = (count << 24) | 0xffffff;
	}*/

	p << "base addrs\n" << *Dispcc::DISPC_GFX_BA_0 << " " << *Dispcc::DISPC_GFX_BA_1 << "\n";

	p << "row inc \n" << *Dispcc::DISPC_GFX_ROW_INC << " pixel inc " << *Dispcc::DISPC_GFX_PIXEL_INC << "\n";

	//configure gfx window
	*Dispcc::DISPC_GFX_ATTRIBUTES |= (0xc << 1);		//select the format of image
	*Dispcc::DISPC_GFX_SIZE = (width - 1) | ((height - 1) << 16);		//set the x/y size of image

	//configure gfx pipeline processing
//	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 5);			//replication
//	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 9);			//nibble mode
//	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 24);			//antiflicker
	*Dispcc::DISPC_CONFIG1 |= (1 << 3);					//lut as gamma table
	*Dispcc::DISPC_GFX_TABLE_BA = (unsigned int)plut;

//	*Dispcc::DISPC_CONFIG1 |= (1 << 9);

	//pipeline layer output
//	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 8);			//channelout, for tv
	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 30);			//channelout2, for lcd2
//	*Dispcc::DISPC_CONTROL1 |= (1 << 6);				//gotv
	*Dispcc::DISPC_CONTROL2 |= (1 << 5);				//golcd 2
	*Dispcc::DISPC_GFX_ATTRIBUTES |= 1;					//enable graphics pipeline

	//lcd2 panel background colour, DISPC_DEFAULT_COLOR2 2706
//	*Dispcc::DISPC_DEFAULT_COLOR2 = 0xffffff;

	//DISPC_CONTROL2[12] overlay optimisation, 2689
//	*Dispcc::DISPC_CONTROL2 &= ~(1 << 12);

	//DISPC_CONFIG2[18] alpha blender, 2731
//	*Dispcc::DISPC_CONFIG1 |= (1 << 18);

	//transparency tv colour key selection DISPC_CONFIG2[11]
//	*Dispcc::DISPC_CONFIG2 &= ~(1 << 11);

	//set transparency colour value, DISPC_TRANS_COLOR2 2706
//	*Dispcc::DISPC_TRANS_COLOR2 = 0xffffffff;

	//transparency colour key disabled DISPC_CONFIG2[10]
//	*Dispcc::DISPC_CONFIG1 |= (1 << 10);
//
//	*Dispcc::DISPC_CONFIG1 |= (1 << 9);			//gamma table enable


	p << "irq stat " << *Dispcc::DISPC_IRQSTATUS << "\n";
	p << "turning on\n";

	/**Dispcc::DISPC_SIZE_TV = 0x02CF04FF;
	*Dispcc::DISPC_CONTROL1 |= (1 << 6);
	*Dispcc::DISPC_CONTROL1 |= (1 << 1);*/

	//http://www.epanorama.net/faq/vga2rgb/calc.html

	*Dispcc::DISPC_POL_FREQ2 = ((int)vert_pol << 13) | ((int)horiz_pol << 12);			//-vsync, -hsync
	*Dispcc::DISPC_DIVISOR2 = (div1 << 16) | div2;					//170.6/1/6=~25 MHz
	*Dispcc::DISPC_SIZE_LCD2 = ((height - 1) << 16) | (width - 1);				//640x480
	*Dispcc::DISPC_CONTROL2 |= (1 << 3);						//active tft
	*Dispcc::DISPC_TIMING_V2 = (vert_back_porch << 20) | (vert_front_porch << 8) | vert_sync;		//31 back porch, 11 front porch, 2 sync width
	*Dispcc::DISPC_TIMING_H2 = (horiz_back_porch << 20) | (horiz_front_porch << 8) | horiz_sync;		//48 back, 16 front, 96 sync

	*Dispcc::DISPC_CONTROL2 |= (1 << 5);						//golcd2
	*Dispcc::DISPC_CONTROL2 |= (1 << 0);						//lcdenable

	p << "default colour " << *Dispcc::DISPC_DEFAULT_COLOR2 << "\n";
	p << "control " << *Dispcc::DISPC_CONTROL2 << "\n";
	p << "config1 " << *Dispcc::DISPC_CONFIG1 << "\n";
	p << "config2 " << *Dispcc::DISPC_CONFIG2 << "\n";
	p << "trans colour " << *Dispcc::DISPC_TRANS_COLOR2 << "\n";

	/*p << "supposed to be on\n";
//	p << "is " << *Dispcc::DISPC_CONTROL1 << "\n";
	while (1)
	{
		p << "irq stat " << *Dispcc::DISPC_IRQSTATUS << "\n";

		for (int count = 2; count < 7; count++)
		{
			p.PrintDec((count - 1) * 32, false);
			p << " " << g_pGpios[count - 2]->Read() << " ";
		}
		p << "\n";
		DelaySecond();
	}
*/

	while(1);
#endif

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
    		TranslationTable::kRwRo, TranslationTable::kExec, TranslationTable::kShareableDevice, 0))
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
			TranslationTable::kShareableDevice, 0);

	for (int count = 0; count < 4096; count++)
		TTB_virt[count] = combined.i;

	combined.s.Init((void *)(0x480 * 1048576),
    		TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
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

	Scheduler::SetMasterScheduler(*new RoundRobin);

	Thread *pHandler = new Thread((unsigned int)&Handler, 0, true, 1, 0);
	ASSERT(pHandler);
	pHandler->SetName("handler");
	Scheduler::GetMasterScheduler().AddSpecialThread(*pHandler, Scheduler::kHandlerThread);

	Thread *pIdle = new Thread((unsigned int)&IdleThread, 0, true, 1, 0);
	ASSERT(pIdle);
	pIdle->SetName("idle");
	Scheduler::GetMasterScheduler().AddSpecialThread(*pIdle, Scheduler::kIdleThread);


#ifdef PBES
	pPic = new CortexA9MPCore::GenericInterruptController((volatile unsigned int *)0xfee40100, (volatile unsigned int *)0xfee41000);
	p << "pic is " << pPic << "\n";
	pTimer1 = new OMAP4460::GpTimer((volatile unsigned int *)0xfef32000, 0, 2);
	ASSERT(pTimer1);
	p << "pTimer1 is " << pTimer1 << "\n";
	pTimer1->SetFrequencyInMicroseconds(1 * 1000 * 1000);
//	pTimer1->Enable(true);

//	pTimer10 = new OMAP4460::GpTimer((volatile unsigned int *)0xfef86000, 0, 10);
//	pTimer10->SetFrequencyInMicroseconds(1 * 1000 * 1000);
//	pTimer10->Enable(true);

	p << "timer interrupt " << pTimer1->GetInterruptNumber() << "\n";
//	p << "timer interrupt " << pTimer10->GetInterruptNumber() << "\n";

	pPic->RegisterInterrupt(*pTimer1, InterruptController::kIrq);
//	pPic->RegisterInterrupt(*pTimer10, InterruptController::kIrq);

	int swi = 0;

	p << "go\n";

//	while(1)
//		pPic->SoftwareInterrupt((swi++) & 0xf);
#else
	pTimer0 = new VersatilePb::SP804((volatile unsigned int *)0xfeee2000, 0);
	pTimer0->SetFrequencyInMicroseconds(10 * 1000);

	pTimer1 = new VersatilePb::SP804((volatile unsigned int *)0xfeee3000, 1);
	pTimer1->SetFrequencyInMicroseconds(1 * 1000 * 1000);

	pMasterClockClear = (unsigned int)pTimer0->GetFiqClearAddress();
	ASSERT(pMasterClockClear);
	masterClockClearValue = pTimer0->GetFiqClearValue();

	pPic = new VersatilePb::PL190((volatile unsigned int *)0xfee40000);
	p << "pic is " << pPic << "\n";
	pPic->RegisterInterrupt(*pTimer0, InterruptController::kFiq);
	pPic->RegisterInterrupt(*pTimer1, InterruptController::kIrq);

#endif

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

	BaseDirent *pLoader = vfs->OpenByName("/Volumes/sd/minimal/lib/ld-minimal.so", O_RDONLY);
	if (!pLoader)
		pLoader = vfs->OpenByName("/Volumes/sd/minimal/lib/ld-linux.so.3", O_RDONLY);
	ASSERT(pLoader);
	ASSERT(pLoader->IsDirectory() == false);

	for (int count = 0; count < 1; count++)
	{
		Process *pBusybox1 = new Process("/Volumes/sd/minimal", "/",
				"/bin/busybox", *vfs, *(File *)pLoader);
		pBusybox1->SetDefaultStdio();
		pBusybox1->SetEnvironment("LAD_DEBUG=all");
		pBusybox1->AddArgument("find");
//		pBusybox1->AddArgument("-l");
		pBusybox1->AddArgument("/");

//		pBusybox1->AddArgument("dd");

		pBusybox1->MakeRunnable();

		pBusybox1->Schedule(Scheduler::GetMasterScheduler());
	}


	Thread *pBlocked;
	Thread *pThread = Scheduler::GetMasterScheduler().PickNext(&pBlocked);
	ASSERT(!pBlocked);
	ASSERT(pThread);

	p << "pTimer1 is " << pTimer1 << "\n";

//	pTimer0->Enable(true);
	pTimer1->Enable(true);

	pThread->Run();

	ASSERT(0);
	while(1);
}
