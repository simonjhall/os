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
#include "Modeline.h"
#include "DSS.h"

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <fcntl.h>

#include <algorithm>
#include <vector>

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
	v7_flush_icache_all();
    v7_flush_dcache_all();

	//entry may be pushed up a little bit
	unsigned int entry_aligned_back = (unsigned int)&entry_maybe_misaligned & ~4095;
//	unsigned int virt_phys_offset = entry_aligned_back - physEntryPoint;
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

extern "C" void EnableCaches(void);

extern "C" void Setup(unsigned int entryPoint)
{
	v7_invalidate_l1();
	v7_flush_icache_all();

	EnableCaches();

	MapKernel(entryPoint);

	PrinterUart<PL011> p;
	PL011::EnableFifo(false);
	PL011::EnableUart(true);

	p << "mmu and uart enabled\n";

	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, false);
	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, true);

	VectorTable::EncodeAndWriteBkpt(VectorTable::kReset);
	VectorTable::EncodeAndWriteLiteralLoad(&_Und, VectorTable::kUndefinedInstruction);
	VectorTable::EncodeAndWriteLiteralLoad(&_Svc, VectorTable::kSupervisorCall);
	VectorTable::EncodeAndWriteLiteralLoad(&_Prf, VectorTable::kPrefetchAbort);
	VectorTable::EncodeAndWriteLiteralLoad(&_Dat, VectorTable::kDataAbort);
	VectorTable::EncodeAndWriteBkpt(VectorTable::kHypTrap);
	VectorTable::EncodeAndWriteLiteralLoad(&_Irq, VectorTable::kIrq);
	VectorTable::EncodeAndWriteLiteralLoad(&_Fiq, VectorTable::kFiq);

	p << "exception table inserted\n";

	EnableFpu(true);

	p << "fpu enabled\n";

	if (!InitMempool((void *)0xa0000000, 256 * 5))		//5MB
		ASSERT(0);

	p << "memory pool initialised\n";


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
	VirtMem::OMAP4460::OmapSmc(&r0, &r1, VirtMem::OMAP4460::kL2EnableDisableCache);

	p << pl310[0] << "\n";
	p << pl310[4 >> 2] << "\n";
	p << pl310[0x100 >> 2] << "\n";
	p << pl310[0x104 >> 2] << "\n";


	/***
	 * change to 1.2 GHz
	 */

//	p << "changing clock speed\n";
//	*(volatile unsigned int *)0xe020416c = 0xc07d07;
//	p << "done\n";

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
	auto s2_switch = GetPin(113);
	gpio0.SetAs(OMAP4460::GPIO::kOutput);
	gpio110.SetAs(OMAP4460::GPIO::kOutput);
	gpio122.SetAs(OMAP4460::GPIO::kInput);
	s2_switch.SetAs(OMAP4460::GPIO::kInput);

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

	Modeline::SetDefaultModesList(new std::list<Modeline>);
	Modeline::AddDefaultModes();

	for (auto it = Modeline::GetDefaultModesList()->begin(); it != Modeline::GetDefaultModesList()->end(); it++)
		it->Print(p);

	OMAP4460::DSS *dss = new OMAP4460::DSS;
	Modeline m;
	if (Modeline::FindModeline("1920x1080@60", m))
		dss->EnableDisplay(m);
	else
		ASSERT(0);

	p << "display should be enabled\n";

	const int width = 1920 / 2;
	const int height = 1080 / 2;
	void *pPhysFb = PhysPages::FindMultiplePages((width * height * 4) / 4096 + 1, 0);
	OMAP4460::Gfx g(pPhysFb, OMAP4460::Gfx::kxRGB24_8888,
			width, height, 1, 1, 0, 0);
	g.Attach();


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

//	int swi = 0;

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

	for (int count = 0; count < 16; count++)
	{
		p << VectorTable::GetTableAddress()[count] << "\n";
	}

	pPic->RegisterInterrupt(*dss, InterruptController::kIrq);
	dss->OnInterrupt([](InterruptSource &)
			{
				PrinterUart<PL011> p;
				p << "vsync\n";
			});
	dss->SetInterruptMask(/*OMAP4460::DSS::sm_vSync2*/-1);
	dss->EnableInterrupt(true);

	while(1)
	{
		p << "still alive\n";
		DelaySecond();
	}


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

	dss->EnableInterrupt(true);

	pThread->Run();

	ASSERT(0);
	while(1);
}
