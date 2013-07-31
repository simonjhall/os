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
#include "IoSpace.h"
#include "Ehci.h"

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

    if (!InitMempool((void *)0xa0000000, 256 * 5))		//5MB
		ASSERT(0);

#ifdef PBES
    IoSpace::SetDefaultIoSpace(new OMAP4460::OmapIoSpace((volatile unsigned int *)0xfc000000));
#else
    IoSpace::SetDefaultIoSpace(new VersatilePb::VersIoSpace((volatile unsigned int *)0xfc000000));
#endif
    ASSERT(IoSpace::GetDefaultIoSpace());

    IoSpace::GetDefaultIoSpace()->Map();

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

struct omap_uhh {
	volatile unsigned int rev;	/* 0x00 */
	volatile unsigned int hwinfo;	/* 0x04 */
	volatile unsigned char reserved1[0x8];
	volatile unsigned int sysc;	/* 0x10 */
	volatile unsigned int syss;	/* 0x14 */
	volatile unsigned char reserved2[0x28];
	volatile unsigned int hostconfig;	/* 0x40 */
	volatile unsigned int debugcsr;	/* 0x44 */
};

struct omap_usbtll {
	volatile unsigned int rev;		/* 0x00 */
	volatile unsigned int hwinfo;		/* 0x04 */
	volatile unsigned char reserved1[0x8];
	volatile unsigned int sysc;		/* 0x10 */
	volatile unsigned int syss;		/* 0x14 */
	volatile unsigned int irqst;		/* 0x18 */
	volatile unsigned int irqen;		/* 0x1c */
	volatile unsigned char reserved2[0x10];
	volatile unsigned int shared_conf;	/* 0x30 */
	volatile unsigned char reserved3[0xc];
	volatile unsigned int channel_conf;	/* 0x40 */
};

struct omap_ehci {
	volatile unsigned int hccapbase;		/* 0x00 */
	volatile unsigned int hcsparams;		/* 0x04 */
	volatile unsigned int hccparams;		/* 0x08 */
	volatile unsigned char reserved1[0x04];
	volatile unsigned int usbcmd;		/* 0x10 */
	volatile unsigned int usbsts;		/* 0x14 */
	volatile unsigned int usbintr;		/* 0x18 */
	volatile unsigned int frindex;		/* 0x1c */
	volatile unsigned int ctrldssegment;	/* 0x20 */
	volatile unsigned int periodiclistbase;	/* 0x24 */
	volatile unsigned int asysnclistaddr;	/* 0x28 */
	volatile unsigned char reserved2[0x24];
	volatile unsigned int configflag;		/* 0x50 */
	volatile unsigned int portsc_i;		/* 0x54 */
	volatile unsigned char reserved3[0x38];
	volatile unsigned int insreg00;		/* 0x90 */
	volatile unsigned int insreg01;		/* 0x94 */
	volatile unsigned int insreg02;		/* 0x98 */
	volatile unsigned int insreg03;		/* 0x9c */
	volatile unsigned int insreg04;		/* 0xa0 */
	volatile unsigned int insreg05_utmi_ulpi;	/* 0xa4 */
	volatile unsigned int insreg06;		/* 0xa8 */
	volatile unsigned int insreg07;		/* 0xac */
	volatile unsigned int insreg08;		/* 0xb0 */
} *pEhci;

struct ulpi_regs {
	/* Vendor ID and Product ID: 0x00 - 0x03 Read-only */
	volatile unsigned char	vendor_id_low;
	volatile unsigned char	vendor_id_high;
	volatile unsigned char	product_id_low;
	volatile unsigned char	product_id_high;
	/* Function Control: 0x04 - 0x06 Read */
	volatile unsigned char	function_ctrl;		/* 0x04 Write */
	volatile unsigned char	function_ctrl_set;	/* 0x05 Set */
	volatile unsigned char	function_ctrl_clear;	/* 0x06 Clear */
	/* Interface Control: 0x07 - 0x09 Read */
	volatile unsigned char	iface_ctrl;		/* 0x07 Write */
	volatile unsigned char	iface_ctrl_set;		/* 0x08 Set */
	volatile unsigned char	iface_ctrl_clear;	/* 0x09 Clear */
	/* OTG Control: 0x0A - 0x0C Read */
	volatile unsigned char	otg_ctrl;		/* 0x0A Write */
	volatile unsigned char	otg_ctrl_set;		/* 0x0B Set */
	volatile unsigned char	otg_ctrl_clear;		/* 0x0C Clear */
	/* USB Interrupt Enable Rising: 0x0D - 0x0F Read */
	volatile unsigned char	usb_ie_rising;		/* 0x0D Write */
	volatile unsigned char	usb_ie_rising_set;	/* 0x0E Set */
	volatile unsigned char	usb_ie_rising_clear;	/* 0x0F Clear */
	/* USB Interrupt Enable Falling: 0x10 - 0x12 Read */
	volatile unsigned char	usb_ie_falling;		/* 0x10 Write */
	volatile unsigned char	usb_ie_falling_set;	/* 0x11 Set */
	volatile unsigned char	usb_ie_falling_clear;	/* 0x12 Clear */
	/* USB Interrupt Status: 0x13 Read-only */
	volatile unsigned char	usb_int_status;
	/* USB Interrupt Latch: 0x14 Read-only with auto-clear */
	volatile unsigned char	usb_int_latch;
	/* Debug: 0x15 Read-only */
	volatile unsigned char	debug;
	/* Scratch Register: 0x16 - 0x18 Read */
	volatile unsigned char	scratch;		/* 0x16 Write */
	volatile unsigned char	scratch_set;		/* 0x17 Set */
	volatile unsigned char	scratch_clear;		/* 0x18 Clear */
	/*
	 * Optional Carkit registers:
	 * Carkit Control: 0x19 - 0x1B Read
	 */
	volatile unsigned char	carkit_ctrl;		/* 0x19 Write */
	volatile unsigned char	carkit_ctrl_set;	/* 0x1A Set */
	volatile unsigned char	carkit_ctrl_clear;	/* 0x1B Clear */
	/* Carkit Interrupt Delay: 0x1C Read, Write */
	volatile unsigned char	carkit_int_delay;
	/* Carkit Interrupt Enable: 0x1D - 0x1F Read */
	volatile unsigned char	carkit_ie;		/* 0x1D Write */
	volatile unsigned char	carkit_ie_set;		/* 0x1E Set */
	volatile unsigned char	carkit_ie_clear;	/* 0x1F Clear */
	/* Carkit Interrupt Status: 0x20 Read-only */
	volatile unsigned char	carkit_int_status;
	/* Carkit Interrupt Latch: 0x21 Read-only with auto-clear */
	volatile unsigned char	carkit_int_latch;
	/* Carkit Pulse Control: 0x22 - 0x24 Read */
	volatile unsigned char	carkit_pulse_ctrl;		/* 0x22 Write */
	volatile unsigned char	carkit_pulse_ctrl_set;		/* 0x23 Set */
	volatile unsigned char	carkit_pulse_ctrl_clear;	/* 0x24 Clear */
	/*
	 * Other optional registers:
	 * Transmit Positive Width: 0x25 Read, Write
	 */
	volatile unsigned char	transmit_pos_width;
	/* Transmit Negative Width: 0x26 Read, Write */
	volatile unsigned char	transmit_neg_width;
	/* Receive Polarity Recovery: 0x27 Read, Write */
	volatile unsigned char	recv_pol_recovery;
	/*
	 * Addresses 0x28 - 0x2E are reserved, so we use offsets
	 * for immediate registers with higher addresses
	 */
};

#if 0
int ulpi_wait(unsigned int mask)
{
	PrinterUart<PL011> p;
	p << __FUNCTION__ << " mask " << mask << "\n";
	while (1) {
		p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << "\n";
		if ((pEhci->insreg05_utmi_ulpi & mask))
		{
			p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << " out\n";
			return 0;
		}
	}

	return 1 << 8;
}

int ulpi_wakeup(void)
{
	int err;

	PrinterUart<PL011> p;
	p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << "\n";

	if (pEhci->insreg05_utmi_ulpi & (1 << 31))
	{
		p << __FUNCTION__ << " already awake\n";
		return 0; /* already awake */
	}

	p << __FUNCTION__ << " not awake\n";
	pEhci->insreg05_utmi_ulpi = 1 << 31;

	err = ulpi_wait(1 << 31);
	ASSERT(!err);

	return err;
}

int ulpi_request(unsigned int value)
{
	int err;

	PrinterUart<PL011> p;
	p << __FUNCTION__ << " value " << value << "\n";

	err = ulpi_wakeup();
	if (err)
	{
		p << __FUNCTION__ << " wakeup error\n";
		return err;
	}

	pEhci->insreg05_utmi_ulpi = value;

	err = ulpi_wait(1 << 31);
	ASSERT(!err);

	return err;
}
#endif

int ulpi_write(unsigned int reg, unsigned int value)
{
	/*PrinterUart<PL011> p;
	p << __FUNCTION__ << " reg " << reg << " value " << value << "\n";

	unsigned int val = (1 << 24) |
			(2 << 22) | (reg << 16) | (value & 0xff);

	return ulpi_request(val);*/

	PrinterUart<PL011> p;

	unsigned int val = (1 << 31) | (1 << 24) | (2 << 22) | (reg << 16);
	pEhci->insreg05_utmi_ulpi = val;

	while (pEhci->insreg05_utmi_ulpi & (1 << 31));

	return pEhci->insreg05_utmi_ulpi & 0xff;
}

unsigned int ulpi_read(unsigned int reg)
{
	/*int err;

	PrinterUart<PL011> p;
	p << __FUNCTION__ << " reg " << reg << "\n";

	unsigned int val = (1 << 24) |
			 (3 << 22) | (reg << 16);

	err = ulpi_request(val);
	if (err)
	{
		p << __FUNCTION__ << " request error\n";
		return err;
	}

	p << __FUNCTION__ << " insreg05_utmi_ulpi " << pEhci->insreg05_utmi_ulpi << "\n";
	return pEhci->insreg05_utmi_ulpi & 0xff;*/

	PrinterUart<PL011> p;

	unsigned int val = (1 << 31) | (1 << 24) | (3 << 22) | (reg << 16);
	pEhci->insreg05_utmi_ulpi = val;

	while (pEhci->insreg05_utmi_ulpi & (1 << 31));

	return pEhci->insreg05_utmi_ulpi & 0xff;
}

int ulpi_reset_wait(void)
{
	unsigned int val;

	PrinterUart<PL011> p;
	p << __FUNCTION__ << "\n";

	/* Wait for the RESET bit to become zero */
	while (1) {
		/*
		 * This function is generic and suppose to work
		 * with any viewport, so we cheat here and don't check
		 * for the error of ulpi_read(), if there is one, then
		 * there will be a timeout.
		 */
		val = ulpi_read(offsetof(ulpi_regs, function_ctrl));
		p << __FUNCTION__ << " val " << val << "\n";
		if (!(val & (1 << 5)))
		{
			p << __FUNCTION__ << " val " << val << " out\n";
			return 0;
		}
	}

	p << __FUNCTION__ << " error...!\n";
	return 1 << 8;
}

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

	/*VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, false);
	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, true);*/
	VirtMem::DumpVirtToPhys((void *)0xf0000000, (void *)0xffffffff, true, true, true);

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

#ifdef PBES
	volatile unsigned int *pl310 = IoSpace::GetDefaultIoSpace()->Get("PL310");
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

	/*p << "changing clock speed\n";
	IoSpace::GetDefaultIoSpace()->Get("CM1")[0x16c >> 2] = 0xc07d07;
	p << "done\n";*/

//	while(1);

	//led pin mux
	IoSpace::GetDefaultIoSpace()->Get("SYSCTRL_PADCONF_CORE")[0xfc >> 2] = 0x11b;

	g_pGpios[0] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO1"), 1);
	g_pGpios[1] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO2"), 2);
	g_pGpios[2] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO3"), 3);
	g_pGpios[3] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO4"), 4);
	g_pGpios[4] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO5"), 5);
	g_pGpios[5] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO6"), 6);


	auto gpio0 = GetPin(0);
	auto gpio1 = GetPin(1);
	auto gpio62 = GetPin(62);
	auto gpio110 = GetPin(110);
	auto gpio122 = GetPin(122);
	auto s2_switch = GetPin(113);
	gpio0.SetAs(OMAP4460::GPIO::kOutput);
	gpio1.SetAs(OMAP4460::GPIO::kOutput);
	gpio62.SetAs(OMAP4460::GPIO::kOutput);
	gpio110.SetAs(OMAP4460::GPIO::kOutput);
	gpio122.SetAs(OMAP4460::GPIO::kInput);
	s2_switch.SetAs(OMAP4460::GPIO::kInput);

	gpio0.Write(true);
	/*bool flash = false;

	while (1)
	{
		p << s2_switch.Read() << "\n";
		gpio110.Write(flash);
		flash = !flash;
		DelaySecond();
	}*/
/*
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
#endif


	p << "enabling interrupts\n";

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

	p << "making timer and interrupt controller\n";


#ifdef PBES
	pPic = new CortexA9MPCore::GenericInterruptController(
			&IoSpace::GetDefaultIoSpace()->Get("SCU/GIC_Proc_Interface/Timer")[0x100 >> 2],
			IoSpace::GetDefaultIoSpace()->Get("GIC_Intr_Distributor"));

	pTimer1 = new OMAP4460::GpTimer(IoSpace::GetDefaultIoSpace()->Get("GPTIMER2"), 0, 2);
	ASSERT(pTimer1);
	pTimer1->SetFrequencyInMicroseconds(1 * 1000 * 1000);

	pPic->RegisterInterrupt(*pTimer1, InterruptController::kIrq);
#else
	pTimer0 = new VersatilePb::SP804(IoSpace::GetDefaultIoSpace()->Get("Timer modules 0 and 1 interface"), 0);
	pTimer0->SetFrequencyInMicroseconds(10 * 1000);

	pTimer1 = new VersatilePb::SP804(IoSpace::GetDefaultIoSpace()->Get("Timer modules 2 and 3 interface"), 1);
	pTimer1->SetFrequencyInMicroseconds(1 * 1000 * 1000);

	pMasterClockClear = (unsigned int)pTimer0->GetFiqClearAddress();
	ASSERT(pMasterClockClear);
	masterClockClearValue = pTimer0->GetFiqClearValue();

	pPic = new VersatilePb::PL190(IoSpace::GetDefaultIoSpace()->Get("Vectored Interrupt Controller"));
	p << "pic is " << pPic << "\n";
	pPic->RegisterInterrupt(*pTimer0, InterruptController::kFiq);
	pPic->RegisterInterrupt(*pTimer1, InterruptController::kIrq);

#endif

#ifdef PBES
	p << "registering interrupts\n";
	pPic->RegisterInterrupt(*dss, InterruptController::kIrq);
	dss->OnInterrupt([](InterruptSource &)
			{
				PrinterUart<PL011> p;
				p << "vsync\n";
			});
	dss->SetInterruptMask(/*OMAP4460::DSS::sm_vSync2*/-1);
//	dss->EnableInterrupt(true);
//	pTimer1->Enable(true);
//	pTimer1->EnableInterrupt(true);


	//enable port clocks
	volatile unsigned int * const pCM_L3INIT_HSUSBHOST_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x1358 >> 2];
	volatile unsigned int * const pCM_L3INIT_HSUSBTLL_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x1368 >> 2];
	*pCM_L3INIT_HSUSBHOST_CLKCTRL |= (1 << 24) | 2;
	*pCM_L3INIT_HSUSBTLL_CLKCTRL |= 1;
	p << "port clocks " << *pCM_L3INIT_HSUSBHOST_CLKCTRL << "\n";

	//put the phy in reset
	gpio1.Write(false);
	gpio62.Write(false);
	for (int count = 0; count < 10; count++)
		DelayMillisecond();

	omap_uhh *pUhh = (omap_uhh *)(volatile unsigned int *)IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST");
	omap_usbtll *pUsbTll = (omap_usbtll *)(volatile unsigned int *)IoSpace::GetDefaultIoSpace()->Get("HSUSBTLL");
	pEhci = (omap_ehci *)(volatile unsigned int *)&IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST")[0xc00 >> 2];

	p << "uhh revision " << pUhh->rev << "\n";

	p << "resetting uhh\n";
	//soft reset of uhh
	pUhh->sysc = 1;
	p << pUhh->sysc << "\n";
	while (!(pUhh->syss & (1 << 2)));

	p << "resetting tll\n";

	//soft reset of tll
	pUsbTll->sysc = 1 << 1;
	while (!(pUsbTll->syss & 1));

	p << "reset done\n";

	/*writel(OMAP_USBTLL_SYSCONFIG_ENAWAKEUP |
		OMAP_USBTLL_SYSCONFIG_SIDLEMODE |
		OMAP_USBTLL_SYSCONFIG_CACTIVITY, &usbtll->sysc);*/
	pUsbTll->sysc |= (1 << 2) | (1 << 3) | (1 << 8);

	/* Put UHH in NoIdle/NoStandby mode
	writel(OMAP_UHH_SYSCONFIG_VAL, &uhh->sysc);*/
	pUhh->sysc |= (1 << 2) | (1 << 4);

	//set burst and phy for 1/2, start clock on ohci suspend
	pUhh->hostconfig = (pUhh->hostconfig & ~((3 << 16) | (3 << 18) | (1 << 5))) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 31);
	p << "hostconfig is " << pUhh->hostconfig << "\n";

	//put the phy in reset
	for (int count = 0; count < 10; count++)
		DelayMillisecond();
	gpio1.Write(true);
	gpio62.Write(true);

	//"undocumented feature"
	pEhci->insreg04 = 1 << 5;

	//reset each port
	if (ulpi_write(offsetof(ulpi_regs, function_ctrl_set), 1 << 5))
		ASSERT(0);

	ulpi_reset_wait();

	p << "vendor low " << ulpi_read(offsetof(ulpi_regs, vendor_id_low)) << "\n";
	p << "vendor high " << ulpi_read(offsetof(ulpi_regs, vendor_id_high)) << "\n";
	p << "product low " << ulpi_read(offsetof(ulpi_regs, product_id_low)) << "\n";
	p << "product high" << ulpi_read(offsetof(ulpi_regs, product_id_high)) << "\n";


	Ehci e((volatile unsigned int *)&IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST")[0xc00 >> 2]);
#else
	Ehci e(IoSpace::GetDefaultIoSpace()->Get("EHCI"));
#endif
	e.Initialise();


//	{
#ifdef PBES
		OMAP4460::MMCi sd(IoSpace::GetDefaultIoSpace()->Get("HSMMC1"), 1);

		p << "resetting\n";
		if (!sd.Reset())
			p << "reset failed\n";
#else
		VersatilePb::PL181 sd(IoSpace::GetDefaultIoSpace()->Get("Multimedia Card Interface 0 (MMCI0)"));
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

//	dss->EnableInterrupt(true);

	pThread->Run();

	ASSERT(0);
	while(1);
}
