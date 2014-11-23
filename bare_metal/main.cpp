#include "print.h"
#include "PL011.h"
#include "UART.h"
#include "PrintFb.h"
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
#include "CachedBlockDevice.h"
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
#include "UsbDevice.h"
#include "UsbHub.h"
#include "M3Control.h"
#include "Mailbox.h"
#include "SemiHosting.h"
#include "slip.h"
#include "LfRing.h"
#include "ipv4.h"
#include "ICMP.h"
#include "UDP.h"
#include "UdpPrinter.h"
#include "TimeFromBoot.h"

#include <stdio.h>
#include <string.h>
#include <link.h>
#include <elf.h>
#include <fcntl.h>

#include <algorithm>
#include <vector>

Printer *Printer::sm_defaultPrinter = 0;
GenericByteTxRx *g_pGenericTxRx;

const unsigned int initial_stack_size = 1024;
unsigned int initial_stack[initial_stack_size];
unsigned int initial_stack_end = (unsigned int)&initial_stack[initial_stack_size];

PeriodicTimer *pTimer0 = 0;
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
//	Printer &p = Printer::Get();
//	*p << "irq " << clock++ << " time " << Formatter<float>(master_clock / 100.0f, 2) << "\n";
//	*p << "pic is " << pPic << "\n";

	InterruptSource *pSource;

	ASSERT(pPic);
	while ((pSource = pPic->WhatHasFired()))
	{
		//todo add to list
//		*p << "interrupt from " << pSource << " # " << pSource->GetInterruptNumber() << "\n";
		pSource->HandleInterrupt();
	}
//	*p << "returning from irq\n";
}


/////////////////////////////////////////////////

extern "C" void _Fiq(void);

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

VirtualFS *vfs;
SD *pSd;
FatFS *pFat;
MBR *pMbr;
CachedBlockDevice *pCachedSd;

mspace s_uncachedPool, s_blockCache;

mspace GetUncachedPool(void)
{
	ASSERT(s_uncachedPool);
	return s_uncachedPool;
}

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
	Printer &p = Printer::Get();
	*p << __FUNCTION__ << " mask " << mask << "\n";
	while (1) {
		*p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << "\n";
		if ((pEhci->insreg05_utmi_ulpi & mask))
		{
			*p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << " out\n";
			return 0;
		}
	}

	return 1 << 8;
}

int ulpi_wakeup(void)
{
	int err;

	Printer &p = Printer::Get();
	*p << __FUNCTION__ << " " << pEhci->insreg05_utmi_ulpi << "\n";

	if (pEhci->insreg05_utmi_ulpi & (1 << 31))
	{
		*p << __FUNCTION__ << " already awake\n";
		return 0; /* already awake */
	}

	*p << __FUNCTION__ << " not awake\n";
	pEhci->insreg05_utmi_ulpi = 1 << 31;

	err = ulpi_wait(1 << 31);
	ASSERT(!err);

	return err;
}

int ulpi_request(unsigned int value)
{
	int err;

	Printer &p = Printer::Get();
	*p << __FUNCTION__ << " value " << value << "\n";

	err = ulpi_wakeup();
	if (err)
	{
		*p << __FUNCTION__ << " wakeup error\n";
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
	/*Printer &p = Printer::Get();
	*p << __FUNCTION__ << " reg " << reg << " value " << value << "\n";

	unsigned int val = (1 << 24) |
			(2 << 22) | (reg << 16) | (value & 0xff);

	return ulpi_request(val);*/

//	Printer &p = Printer::Get();

	unsigned int val = (1 << 31) | (1 << 24) | (2 << 22) | (reg << 16);
	pEhci->insreg05_utmi_ulpi = val;

	while (pEhci->insreg05_utmi_ulpi & (1 << 31));

	return pEhci->insreg05_utmi_ulpi & 0xff;
}

unsigned int ulpi_read(unsigned int reg)
{
	/*int err;

	Printer &p = Printer::Get();
	*p << __FUNCTION__ << " reg " << reg << "\n";

	unsigned int val = (1 << 24) |
			 (3 << 22) | (reg << 16);

	err = ulpi_request(val);
	if (err)
	{
		*p << __FUNCTION__ << " request error\n";
		return err;
	}

	*p << __FUNCTION__ << " insreg05_utmi_ulpi " << pEhci->insreg05_utmi_ulpi << "\n";
	return pEhci->insreg05_utmi_ulpi & 0xff;*/

//	Printer &p = Printer::Get();

	unsigned int val = (1 << 31) | (1 << 24) | (3 << 22) | (reg << 16);
	pEhci->insreg05_utmi_ulpi = val;

	while (pEhci->insreg05_utmi_ulpi & (1 << 31));

	return pEhci->insreg05_utmi_ulpi & 0xff;
}

int ulpi_reset_wait(void)
{
	unsigned int val;

	Printer &p = Printer::Get();
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

void change_clock_enable_l2(Printer *p)
{
	volatile unsigned int *pl310 = IoSpace::GetDefaultIoSpace()->Get("PL310");
	*p << pl310[0] << "\n";
	*p << pl310[4 >> 2] << "\n";
	*p << pl310[0x100 >> 2] << "\n";
	*p << pl310[0x104 >> 2] << "\n";

	/*unsigned int r0, r1;
	r0 = 1;
	r1 = 0;
	VirtMem::OMAP4460::OmapSmc(&r0, &r1, VirtMem::OMAP4460::kL2EnableDisableCache);*/

	*p << pl310[0] << "\n";
	*p << pl310[4 >> 2] << "\n";
	*p << pl310[0x100 >> 2] << "\n";
	*p << pl310[0x104 >> 2] << "\n";


	/***
	 * change to 1.2 GHz
	 * todo change voltage too?
	 */

//	*p << "changing clock speed\n";
//	IoSpace::GetDefaultIoSpace()->Get("CM1")[0x16c >> 2] = 0xc07d07;
//	*p << "done\n";
}

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

    //clear any misalignment (those section entries need to be cleared)
    for (unsigned int i = 0; i < ((((unsigned int)&entry_maybe_misaligned) + 4) & 16383) >> 2; i++)			//4b for the branch
    	VirtMem::GetL1TableVirt(false)[i].fault.Init();

    //disable the existing phys map
    for (unsigned int i = physEntryPoint >> 20; i < (physEntryPoint + image_length_section_align) >> 20; i++)
    	VirtMem::GetL1TableVirt(false)[i].fault.Init();

    //use the zero lo
    VirtMem::SetL1TableLo(0);

    if (!InitMempool((void *)0xa0000000, 256 * 5, false))		//5MB
		ASSERT(0);

    //block cache
	if (!InitMempool((void *)0xa0700000, (1048576 * 10) >> 12, false, &s_blockCache))		//10 MB
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

namespace VirtMem
{
	extern FixedSizeAllocator<TranslationTable::SmallPageActual, 1048576 / sizeof(TranslationTable::SmallPageActual)> g_sysThreadStacks;
	extern FixedSizeAllocator<TranslationTable::L1Table, 1048576 / sizeof(TranslationTable::L1Table)> g_masterTables;
}

#ifdef PBES
extern char _binary_m3_bin_kernel_elf_bin_start;
extern char _binary_m3_bin_kernel_elf_bin_end;

static void MapM3Kernel(Printer *p)
{
	int size_needed = &_binary_m3_bin_kernel_elf_bin_end - &_binary_m3_bin_kernel_elf_bin_start;
	*p << "need " << size_needed << " bytes for m3 image\n";
	size_needed = (size_needed + 4095) & ~4095;
	size_needed = size_needed >> 12;
	*p << "need " << size_needed << " pages\n";


	void *pPhysM3Image = PhysPages::FindMultiplePages(size_needed, 8);
	ASSERT(pPhysM3Image != (void *)-1);

	*p << "m3 image at " << pPhysM3Image << "\n";

	if (!VirtMem::MapPhysToVirt(pPhysM3Image, (void *)0x90000000, size_needed << 12, true,
			TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0))
		ASSERT(0);

	*p << "mapped\n";

	memcpy((void *)0x90000000, &_binary_m3_bin_kernel_elf_bin_start, &_binary_m3_bin_kernel_elf_bin_end - &_binary_m3_bin_kernel_elf_bin_start);

	unsigned int *prog = (unsigned int *)0x90000000;
	unsigned int prev;
	while (1)
	{
		prev = *prog;
		if (prog[1] == 0xffffffff)
			break;
		prog++;
	}

	prog++;
	*p << "found blank interrupt at " << prog << "\n";
	unsigned int wrote = 0;
	while (*prog == 0xffffffff)
	{
		*prog++ = prev;
		wrote++;
	}
	*p << "wrote " << wrote << " entries\n";

	for (unsigned int count = 0; count < 40; count += 4)
		*p << (0x90000000 + count) << " " << *((unsigned int *)(0x90000000 + count)) << "\n";

	*p << "copied\n";

	void *pPhysM3Stack = PhysPages::FindPage();
	ASSERT(pPhysM3Stack != (void *)-1);
	*(unsigned int *)0x90000000 = 0x24001000;//(unsigned int)pPhysM3Stack;

//		VirtMem::UnmapVirt((void *)0x90000000, size_needed << 12);

	TranslationTable::L1Table *pVirtTable, *pPhysTable;
	if (!VirtMem::g_masterTables.Allocate(&pVirtTable))
		ASSERT(0);
	VirtMem::VirtToPhys(pVirtTable, &pPhysTable);

	//clear the memory space
	for (unsigned int count = 0; count < 4096; count++)
		pVirtTable->e[count].fault.Init();

	//map our code and vector low
	pVirtTable->e[0].section.Init(pPhysM3Image, TranslationTable::kRwRw, TranslationTable::kExec,
			TranslationTable::kOuterInnerWtNoWa, 0);
	//and where we will want it to go after the remap by the m3
	pVirtTable->e[0x240].section.Init(pPhysM3Image, TranslationTable::kRwRw, TranslationTable::kExec,
			TranslationTable::kOuterInnerWbWa, 0);

	//remap the l4 per section
	for (int count = 0; count < 16; count++)
		pVirtTable->e[0x480 + count].section.Init((void *)((0x480 + count) * 1048576), TranslationTable::kRwRw, TranslationTable::kNoExec,
				TranslationTable::kShareableDevice, 0);

	//remap the l4 cfg section
	for (int count = 0; count < 16; count++)
		pVirtTable->e[0x4a0 + count].section.Init((void *)((0x4a0 + count) * 1048576), TranslationTable::kRwRw, TranslationTable::kNoExec,
				TranslationTable::kShareableDevice, 0);

	//remap the m3 section (prob not needed)
	for (int count = 0; count < 16; count++)
		pVirtTable->e[0x550 + count].section.Init((void *)((0x550 + count) * 1048576), TranslationTable::kRwRw, TranslationTable::kNoExec,
				TranslationTable::kOuterInnerWbWa, 0);

	pVirtTable->e[(unsigned int)pPhysTable >> 20].section.Init((void *)((unsigned int)pPhysTable & ~1048575),
			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kOuterInnerWtNoWa, 0);

	void *pCachedHeap = PhysPages::FindMultiplePages(256, 8);
	ASSERT(pCachedHeap != (void *)-1);

	void *pUncachedHeap = PhysPages::FindMultiplePages(256, 8);
	ASSERT(pUncachedHeap != (void *)-1);

	//cached
	pVirtTable->e[0x600].section.Init(pCachedHeap, TranslationTable::kRwRw, TranslationTable::kNoExec,
			TranslationTable::kOuterInnerWbWa, 0);
	//uncached
	pVirtTable->e[0xa00].section.Init(pUncachedHeap, TranslationTable::kRwRw, TranslationTable::kNoExec,
			TranslationTable::kShareableDevice, 0);

	//just behind the main heap
	if (!VirtMem::MapPhysToVirt(pUncachedHeap, (void *)0xa0600000, 1048576, true,
		TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0))
		ASSERT(!"could not map uncached heap\n");

	if (!InitMempool((void *)0xa0680000, 128 * 1, true, &s_uncachedPool))		//512KB
		ASSERT(0);

	pVirtTable->e[0].section.Print(*p);

	*p << "installing m3 table, virt addr " << pVirtTable << "\n";
	*p << "phys addr " << pPhysTable << "\n";
	if (!OMAP4460::M3::SetL1Table(&pVirtTable->e[0]))
		ASSERT(0);
}
#endif

void EnableDisplayAndMapFb(Printer *p, PrinterFb *fb)
{
	//order seems to matter here!
	auto gpio1 = GetPin(1);
	gpio1.SetAs(OMAP4460::GPIO::kOutput);
	//enable ethernet/usb hub ldo power (and the display?)
	gpio1.Write(true);

	auto gpio0 = GetPin(0);
	gpio0.SetAs(OMAP4460::GPIO::kOutput);
	gpio0.Write(true);			//dvi

	Modeline::SetDefaultModesList(new std::list<Modeline>);
	Modeline::AddDefaultModes();

	//turn on the display
	OMAP4460::DSS *dss = new OMAP4460::DSS;
	Modeline m;
	if (Modeline::FindModeline("1280x1024@60", m))
		dss->EnableDisplay(m);
	else
		ASSERT(0);

	*p << "display should be enabled\n";

	//allocate memory for the fb
	const int pages_needed = (((fb->GetWidth() * fb->GetHeight() * 4) + 4095) & ~ 4095) >> 12;

	void *pPhysFb = PhysPages::FindMultiplePages(pages_needed, 0);
	OMAP4460::Gfx g(pPhysFb, OMAP4460::Gfx::kxRGB24_8888,
			fb->GetWidth(), fb->GetHeight(), 1, 1, 0, 0);
	g.Attach();

	if (!VirtMem::MapPhysToVirt(pPhysFb, (void *)fb->GetVirtBase(), pages_needed * 4096, true,
			TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0))
		ASSERT(!"could not map framebuffer\n");

	*p << "framebuffer mapped at phys " << pPhysFb << " virt " << fb->GetVirtBase() << "\n";

	memset(fb->GetVirtBase(), 0, pages_needed * 4096);
}

void CreateVfs(Printer *p)
{
	*p << "creating VFS\n";
	vfs = new VirtualFS();
	//////
	vfs->Mkdir("/", "Devices");

	Stdio *in = new Stdio(Stdio::kStdin, *g_pGenericTxRx, *vfs);
	vfs->AddOrphanFile("/Devices", *in);

	Stdio *out = new Stdio(Stdio::kStdout, *g_pGenericTxRx, *vfs);
	vfs->AddOrphanFile("/Devices", *out);

	Stdio *err = new Stdio(Stdio::kStderr, *g_pGenericTxRx, *vfs);
	vfs->AddOrphanFile("/Devices", *err);
	////

	vfs->Mkdir("/", "Volumes");
}

void MountSd(Printer *p)
{
#ifdef PBES
	pSd = new OMAP4460::MMCi(IoSpace::GetDefaultIoSpace()->Get("HSMMC1"), 1);
#else
	pSd = new VersatilePb::PL181(IoSpace::GetDefaultIoSpace()->Get("Multimedia Card Interface 0 (MMCI0)"));
#endif

	if (!pSd->Reset())
		*p << "reset failed\n";

	pSd->GoIdleState();

	if (!pSd->GoReadyState())
	{
		*p << "no card\n";
		ASSERT(0);
	}
	if (!pSd->GoIdentificationState())
	{
		*p << "definitely no card\n";
		ASSERT(0);
	}

	unsigned int rca;
	bool ok = pSd->GetCardRcaAndGoStandbyState(rca);
	ASSERT(ok);

	ok = pSd->GoTransferState(rca);
	ASSERT(ok);

	pCachedSd = new CachedBlockDevice;
	ASSERT(pCachedSd);
	pCachedSd->Init(*pSd, s_blockCache, 512);

	//speed test
	if (1)
	{
		*p << "beginning reading\n";
		unsigned long long start_time = TimeFromBoot::GetMicroseconds();
		for (int count = 0; count < (1048576 >> 9); count++)
		{
			char buf[512];
			pCachedSd->ReadDataFromLogicalAddress(count << 9, buf, 512);
		}
		unsigned long long end_time = TimeFromBoot::GetMicroseconds();
		*p << "done in "; p->PrintDec((unsigned int)(end_time - start_time), false); *p << " us\n";
	}
	if (1)
	{
		*p << "beginning reading\n";
		unsigned long long start_time = TimeFromBoot::GetMicroseconds();
		for (int count = 0; count < (1048576 >> 9); count++)
		{
			char buf[512];
			pCachedSd->ReadDataFromLogicalAddress(count << 9, buf, 512);
		}
		unsigned long long end_time = TimeFromBoot::GetMicroseconds();
		*p << "done in "; p->PrintDec((unsigned int)(end_time - start_time), false); *p << " us\n";
	}

	pMbr = new MBR(*pCachedSd);

	vfs->Mkdir("/Volumes", "sd");

	*p << "creating FAT\n";

#ifdef PBES
	pFat = new FatFS(*pMbr->GetPartition(0));
#else
	pFat = new FatFS(*pCachedSd);
#endif

	*p << "attaching FAT\n";
	vfs->Attach(*pFat, "/Volumes/sd");
}

void DumpDir(Printer *p, Directory *d, int depth)
{
	for (unsigned int count = 0; count < d->GetNumChildren(); count++)
	{
		BaseDirent *ent = d->GetChild(count);

		for (int i = 0; i < depth; i++)
			*p << "\t";

		*p << ent->GetName() << "\n";

		if (ent->IsDirectory())
		{
			auto fs = ent->GetFilesystem().IsAttachment((Directory *)ent);
			if (fs)
			{
				BaseDirent *next = fs->OpenByName("/", O_RDONLY);
				ASSERT(next->IsDirectory());
				DumpDir(p, (Directory *)next, depth + 1);
			}
			else
				DumpDir(p, (Directory *)ent, depth + 1);
		}
	}
}

Net::IPv4::LfRingType *g_pPackRecvRing, *g_pPackSendRing;
Net::IPv4 *g_pIpv4;
Net::ICMP *g_pIcmp;
Net::UDP *g_pUdp;
Slip::SlipLink *g_pLink;
UdpPrinter *g_udpPrinter;

void SlipRecvThreadFunc(Slip::SlipLink &rLink)
{
	int packet_count = 0;

	Thread *pThisThread = Scheduler::GetMasterScheduler().WhatIsRunning();
	ASSERT(pThisThread);

	Net::MaxPacket *pBuffer = new Net::MaxPacket;
	ASSERT(pBuffer);

	while (1)
	{
		unsigned int len;
		LinkAddrIntf *from;

		if (rLink.Recv(&from, &pBuffer->x, len, rLink.GetMtu()))
		{
			pBuffer->m_size = len;
			pBuffer->m_startOffset = 0;

			g_pPackRecvRing->Push(*pBuffer);
			packet_count++;
		}
	}
}

void SlipSendThreadFunc(Slip::SlipLink &rLink)
{
	int packet_count = 0;

	Thread *pThisThread = Scheduler::GetMasterScheduler().WhatIsRunning();
	ASSERT(pThisThread);

	mspace pool = s_uncachedPool;

	Net::MaxPacket *pBuffer;
	if (pool)
		pBuffer = (Net::MaxPacket *)mspace_malloc(pool, sizeof(Net::MaxPacket));
	else
		pBuffer = new Net::MaxPacket;
	ASSERT(pBuffer);

	while (1)
	{
		g_pPackSendRing->Pop(*pBuffer);
		packet_count++;

		rLink.SendTo(*rLink.GetPtpAddr(), &pBuffer->x[pBuffer->m_startOffset], pBuffer->m_size - pBuffer->m_startOffset);
	}
}

void PingReplyThreadFunc(Net::ICMP::LfRingType &rRecvRing)
{
	Thread *pThisThread = Scheduler::GetMasterScheduler().WhatIsRunning();
	ASSERT(pThisThread);

	Net::ICMP::IcmpPacket *pInBE = new Net::ICMP::IcmpPacket;
	ASSERT(pInBE);

	while (1)
	{
		rRecvRing.Pop(*pInBE);

		unsigned char *pDataBegin = (unsigned char *)&pInBE->m_bytes.x[pInBE->m_bytes.m_startOffset];
		//set the type to zero, for a reply
		pDataBegin[0] = 0;

		g_pIcmp->EnqueuePacket(*g_pIpv4, *pInBE->m_pSourceAddr, *pInBE);

		delete pInBE->m_pSourceAddr;

//		*g_udpPrinter << "this is a longer test than 15 characters\n";
	}
}



PrinterFb *g_pFb;

extern "C" void TimingTest(void);

extern "C" void Setup(unsigned int entryPoint)
{
//	volatile bool pause = true;
//	while (pause);

	v7_invalidate_l1();
	v7_flush_icache_all();

//	EnableCaches();

	MapKernel(entryPoint);

#ifdef PBES
    OMAP4460::UART &uart3 = *new OMAP4460::UART(IoSpace::GetDefaultIoSpace()->Get("UART3").m_pVirt);
    Printer::sm_defaultPrinter = &uart3;
    g_pGenericTxRx = &uart3;

    uart3.EnableFifo(true);
#else
    VersatilePb::PL011 &pl011 = *new VersatilePb::PL011(IoSpace::GetDefaultIoSpace()->Get("UART 0 Interface"));
	Printer::sm_defaultPrinter = &pl011;
	g_pGenericTxRx = &pl011;

	pl011.EnableFifo(true);
	pl011.EnableUart(true);
#endif

	Printer *p = &Printer::Get();

	unsigned int *pVirtFb = (unsigned int *)0xb0000000;
	const int width = 640;
	const int height = 480;

	PrinterFb fb(pVirtFb, width, height, PrinterFb::kxRGB8888, p);
	g_pFb = &fb;

	*p << "build date " __DATE__ " and time " __TIME__ "\n";
	*p << "phys entry point is " << entryPoint << "\n";

//	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, true, true, true);
	VirtMem::DumpVirtToPhys(0, (void *)0xffffffff, false, true, true);
//	VirtMem::DumpVirtToPhys((void *)0xf0000000, (void *)0xffffffff, true, true, true);

	VectorTable::EncodeAndWriteBkpt(VectorTable::kReset);
	VectorTable::EncodeAndWriteLiteralLoad(&_Und, VectorTable::kUndefinedInstruction);
	VectorTable::EncodeAndWriteLiteralLoad(&_Svc, VectorTable::kSupervisorCall);
	VectorTable::EncodeAndWriteLiteralLoad(&_Prf, VectorTable::kPrefetchAbort);
	VectorTable::EncodeAndWriteLiteralLoad(&_Dat, VectorTable::kDataAbort);
	VectorTable::EncodeAndWriteBkpt(VectorTable::kHypTrap);
	VectorTable::EncodeAndWriteLiteralLoad(&_Irq, VectorTable::kIrq);
	VectorTable::EncodeAndWriteLiteralLoad(&_Fiq, VectorTable::kFiq);

	*p << "exception table inserted\n";

	EnableFpu(true);

	*p << "fpu enabled\n";

	*p << "enabling interrupts\n";

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

	*p << "making timer and interrupt controller\n";

#ifdef PBES
	pPic = new CortexA9MPCore::GenericInterruptController(
			&IoSpace::GetDefaultIoSpace()->Get("SCU/GIC_Proc_Interface/Timer")[0x100 >> 2],
			IoSpace::GetDefaultIoSpace()->Get("GIC_Intr_Distributor"));

	pTimer0 = new OMAP4460::GpTimer(IoSpace::GetDefaultIoSpace()->Get("GPTIMER2"), 0, 2);

	TimeFromBoot::Init(new OMAP4460::GpTimer(IoSpace::GetDefaultIoSpace()->Get("GPTIMER1"), 0, 1), pPic);

#else
	pPic = new VersatilePb::PL190(IoSpace::GetDefaultIoSpace()->Get("Vectored Interrupt Controller"));
	pTimer0 = new VersatilePb::SP804(IoSpace::GetDefaultIoSpace()->Get("Timer modules 0 and 1 interface"), 0);

	TimeFromBoot::Init(new VersatilePb::SP804(IoSpace::GetDefaultIoSpace()->Get("Timer modules 0 and 1 interface"), 1), pPic);
#endif

	pTimer0->SetFrequencyInMicroseconds(10 * 1000);
	pPic->RegisterInterrupt(*pTimer0, InterruptController::kIrq);


	//create vfs and attach the sd card
	CreateVfs(p);
	MountSd(p);

	BaseDirent *f = vfs->OpenByName("/", O_RDONLY);
	ASSERT(f->IsDirectory());

	Directory *d = (Directory *)f;
	DumpDir(p, d, 0);

	//load busybox
	if (1)
	{
		BaseDirent *pLoader = vfs->OpenByName("/Volumes/sd/minimal/lib/ld-minimal.so", O_RDONLY);
		if (!pLoader)
			pLoader = vfs->OpenByName("/Volumes/sd/minimal/lib/ld-linux.so.3", O_RDONLY);
		ASSERT(pLoader);
		ASSERT(pLoader->IsDirectory() == false);

		for (int count = 0; count < 1; count++)
		{
//			Process *pBusybox1 = new Process("/Volumes/sd/minimal", "/",
//					"/bin/busybox_new", *vfs, *(File *)pLoader);
			Process *pBusybox1 = new Process("/", "/",
					"/Volumes/sd/minimal/bin/busybox_new", *vfs, *(File *)pLoader);
			pBusybox1->SetDefaultStdio();
//			pBusybox1->SetEnvironment("LD_BIND_NOW=yes");
			pBusybox1->SetEnvironment("LD_DEBUG=all");
			pBusybox1->AddArgument("ash");
			pBusybox1->AddArgument("-i");
//			pBusybox1->AddArgument("-f");
//			pBusybox1->AddArgument("-v");
//			pBusybox1->AddArgument("tftp");
//			pBusybox1->AddArgument("-r");
//			pBusybox1->AddArgument("uImage");
//			pBusybox1->AddArgument("-g");
//			pBusybox1->AddArgument("192.168.2.2");
//			pBusybox1->AddArgument("find");
	//		pBusybox1->AddArgument("-l");
//			pBusybox1->AddArgument("/");

//			pBusybox1->AddArgument("reboot");
//			pBusybox1->AddArgument("192.168.2.1");

	//		pBusybox1->AddArgument("dd");

			pBusybox1->MakeRunnable();

			pBusybox1->Schedule(Scheduler::GetMasterScheduler());
		}
	}

#ifdef PBES
	//led pin mux; configure the switch to work
	IoSpace::GetDefaultIoSpace()->Get("SYSCTRL_PADCONF_CORE")[0xfc >> 2] = 0x11b;

	//make the gpios
	g_pGpios[0] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO1"), 1);
	g_pGpios[1] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO2"), 2);
	g_pGpios[2] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO3"), 3);
	g_pGpios[3] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO4"), 4);
	g_pGpios[4] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO5"), 5);
	g_pGpios[5] = new OMAP4460::GPIO(IoSpace::GetDefaultIoSpace()->Get("GPIO6"), 6);


	auto gpio41 = GetPin(41);
	auto gpio60 = GetPin(60);
	auto gpio110_led = GetPin(110);
	auto gpio122 = GetPin(122);
	auto s2_switch = GetPin(113);

	gpio110_led.SetAs(OMAP4460::GPIO::kOutput);
	gpio122.SetAs(OMAP4460::GPIO::kInput);
	s2_switch.SetAs(OMAP4460::GPIO::kInput);

	volatile bool pause = true;
	while (pause)
	{
		static int count = 0;
		static bool state = false;
		count++;

		if ((count % 500000) == 0)
		{
			state = !state;
			gpio110_led.Write(state);
		}

		if (!s2_switch.Read())
			break;
	}

	//turn on the tv
	EnableDisplayAndMapFb(p, &fb);

	p = &fb;

//	OMAP4460::Mailbox mailbox(IoSpace::GetDefaultIoSpace()->Get("Mailbox"), true);

	OMAP4460::M3::SetPowerState(false);
	OMAP4460::M3::SetPowerState(true);
	OMAP4460::M3::ResetAll();
	OMAP4460::M3::ReleaseCacheMmu();
	OMAP4460::M3::ResetAll();
	OMAP4460::M3::ReleaseCacheMmu();

	OMAP4460::M3::MmuInterrupt m3mmu(IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU"));
	pPic->RegisterInterrupt(m3mmu, InterruptController::kIrq);
	m3mmu.OnInterrupt([](InterruptSource &rSource)
			{
				DelaySecond();
				Printer &p = Printer::Get();
				OMAP4460::M3::MmuInterrupt *pMmu = (OMAP4460::M3::MmuInterrupt *)&rSource;
				p << "m3 mmu interrupt, " << pMmu->ReadIrqStatus() << "\n";
				while(1);
			});
	m3mmu.EnableInterrupt(true);

	MapM3Kernel(p);

	*p << "enabling m3 mmu\n";
	OMAP4460::M3::EnableMmu(true);
	OMAP4460::M3::EnableBusError(true);
	*p << "enabling m3 cpu 1\n";

	fb.SetChainedPrinter(0);
	OMAP4460::M3::EnableCpu(1);
#endif

	{
		g_pPackRecvRing = new Net::IPv4::LfRingType;
		ASSERT(g_pPackRecvRing);

		g_pPackSendRing = new Net::IPv4::LfRingType;
		ASSERT(g_pPackSendRing);

#ifdef PBES
		g_pLink = new Slip::MailboxSlipLink(pPic);
#else
		pPic->RegisterInterrupt(pl011);
		pl011.OnInterrupt([](InterruptSource &rSource)
				{
					VersatilePb::PL011 *p = (VersatilePb::PL011 *)&rSource;

					if (p->HandleInterruptDisable())
						p->GetRxWait().SignalAtomic();
				});
		pl011.EnableInterrupt(true);

		g_pLink = new Slip::SynchronousSlipLink(pl011);
#endif
		ASSERT(g_pLink);

		g_pIpv4 = new Net::IPv4(*g_pPackRecvRing, *g_pPackSendRing);
		g_pIpv4->AddAddr(192, 168, 2, 2);

		g_pIcmp = new Net::ICMP;
		ASSERT(g_pIcmp);
		g_pIpv4->RegisterProtocol(*g_pIcmp);

		g_pUdp = new Net::UDP;
		ASSERT(g_pUdp);
		g_pIpv4->RegisterProtocol(*g_pUdp);

		Thread *pSlipRecv = new Thread((unsigned int)&SlipRecvThreadFunc, 0, true, 1, 0);
		ASSERT(pSlipRecv);
		pSlipRecv->SetName("slip recv");
		pSlipRecv->SetArg(0, g_pLink);

		Thread *pSlipSend = new Thread((unsigned int)&SlipSendThreadFunc, 0, true, 1, 0);
		ASSERT(pSlipSend);
		pSlipSend->SetName("slip send");
		pSlipSend->SetArg(0, g_pLink);

		Thread *pIpDemux = new Thread((unsigned int)&Net::InternetIntf::DemuxThreadFuncEntry, 0, true, 1, 0);
		ASSERT(pIpDemux);
		pIpDemux->SetName("ip demux");
		pIpDemux->SetArg(0, g_pIpv4);

		Thread *pPingReply = new Thread((unsigned int)&PingReplyThreadFunc, 0, true, 1, 0);
		ASSERT(pPingReply);
		pPingReply->SetName("ping reply");
		pPingReply->SetArg(0, &g_pIcmp->GetPingRequestListener());

		g_udpPrinter = new UdpPrinter(*g_pIpv4, *g_pUdp, Net::IPv4Addr(192, 168, 2, 1), 514);
		ASSERT(g_udpPrinter);

		Scheduler::GetMasterScheduler().AddThread(*pSlipRecv);
		Scheduler::GetMasterScheduler().AddThread(*pSlipSend);
		Scheduler::GetMasterScheduler().AddThread(*pIpDemux);
		Scheduler::GetMasterScheduler().AddThread(*pPingReply);
	}

//	pTimer0->Enable(true);

	Thread *pBlocked;
	Thread *pThread = Scheduler::GetMasterScheduler().PickNext(&pBlocked);
	ASSERT(!pBlocked);
	ASSERT(pThread);
	pThread->Run();

	while(1);
}

void something(Printer *p)
{

#ifdef PBES
	//enable port clocks
	volatile unsigned int * const pCM_L3INIT_HSUSBHOST_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x1358 >> 2];
	volatile unsigned int * const pCM_L3INIT_HSUSBTLL_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x1368 >> 2];
	*pCM_L3INIT_HSUSBHOST_CLKCTRL |= (1 << 24) | 2;
	*pCM_L3INIT_HSUSBTLL_CLKCTRL |= 1;
	*p << "port clocks " << *pCM_L3INIT_HSUSBHOST_CLKCTRL << "\n";

	//put the phy in reset
	auto gpio1 = GetPin(1);
	gpio1.Write(false);
	auto gpio62 = GetPin(62);
	gpio62.SetAs(OMAP4460::GPIO::kOutput);
	gpio62.Write(false);
	for (int count = 0; count < 10; count++)
		DelayMillisecond();

	omap_uhh *pUhh = (omap_uhh *)(volatile unsigned int *)IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST");
	omap_usbtll *pUsbTll = (omap_usbtll *)(volatile unsigned int *)IoSpace::GetDefaultIoSpace()->Get("HSUSBTLL");
	pEhci = (omap_ehci *)(volatile unsigned int *)&IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST")[0xc00 >> 2];

	*p << "uhh revision " << pUhh->rev << "\n";

	*p << "resetting uhh\n";
	//soft reset of uhh
	pUhh->sysc = 1;
	*p << pUhh->sysc << "\n";
	while (!(pUhh->syss & (1 << 2)));

	*p << "resetting tll\n";

	//soft reset of tll
	pUsbTll->sysc = 1 << 1;
	while (!(pUsbTll->syss & 1));

	*p << "reset done\n";

	/*writel(OMAP_USBTLL_SYSCONFIG_ENAWAKEUP |
		OMAP_USBTLL_SYSCONFIG_SIDLEMODE |
		OMAP_USBTLL_SYSCONFIG_CACTIVITY, &usbtll->sysc);*/
	pUsbTll->sysc |= (1 << 2) | (1 << 3) | (1 << 8);

	/* Put UHH in NoIdle/NoStandby mode
	writel(OMAP_UHH_SYSCONFIG_VAL, &uhh->sysc);*/
	pUhh->sysc |= (1 << 2) | (1 << 4);

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	//set burst and phy for 1/2, start clock on ohci suspend
	pUhh->hostconfig = (pUhh->hostconfig & ~((3 << 16) | (3 << 18) | (1 << 5))) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 31);
	*p << "hostconfig is " << pUhh->hostconfig << "\n";

	//put the phy in reset
	for (int count = 0; count < 10; count++)
		DelayMillisecond();

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	*p << "phys is in reset\n";

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	gpio1.Write(true);

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	gpio62.Write(true);

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	*p << "gpios are go\n";

	//"undocumented feature"
	pEhci->insreg04 = 1 << 5;

//	for (int count = 0; count < 10; count++) DelaySecond();
//	*p << __LINE__ << "\n";

	*p << "undocu feature is set\n";

	//reset each port
	if (ulpi_write(offsetof(ulpi_regs, function_ctrl_set), 1 << 5))
		ASSERT(0);

	*p << "resetting the port\n";

	ulpi_reset_wait();

	*p << "reset wait done\n";

	*p << "vendor low " << ulpi_read(offsetof(ulpi_regs, vendor_id_low)) << "\n";
	*p << "vendor high " << ulpi_read(offsetof(ulpi_regs, vendor_id_high)) << "\n";
	*p << "product low " << ulpi_read(offsetof(ulpi_regs, product_id_low)) << "\n";
	*p << "product high" << ulpi_read(offsetof(ulpi_regs, product_id_high)) << "\n";


	USB::Ehci e((volatile unsigned int *)&IoSpace::GetDefaultIoSpace()->Get("HSUSBHOST")[0xc00 >> 2]);
#else
	USB::Ehci e(IoSpace::GetDefaultIoSpace()->Get("EHCI"));
#endif
	e.Initialise();

	/*int s = 0;
	while(1)
	{
		*p << "second " << s << "\n";
		DelaySecond();
		s++;
	}*/

	std::list<USB::UsbDevice *> devices;
	std::list<USB::Hub *> hubs;
	std::list<USB::Port *> ports;

	hubs.push_back(&e.GetRootHub());

	*p << e.GetRootHub().GetNumPorts() << " ports on root hub\n";

	//add root hub ports to list
	for (unsigned int count = 0; count < e.GetRootHub().GetNumPorts(); count++)
		ports.push_back(e.GetRootHub().GetPort(count));

	for (auto it = ports.begin(); it != ports.end(); it++)
	{
		ASSERT(*it);

		//power cycle it
		(*it)->PowerOn(false);

		if ((*it)->PowerOn(true) && (*it)->IsDeviceAttached())
		{
			int addr = e.AllocateBusAddress();
			if (addr <= 0)
				break;		//no spare addresses

			*p << "found device on port " << *it << "\n";
			USB::UsbDevice dev(e, *(*it));
			if (dev.BasicInit(addr) == false)
			{
				*p << "BasicInit failed\n";
				(*it)->PowerOn(false);
				continue;
			}

			unsigned short vendor, product, version;
			if (!dev.ProductIdentify(vendor, product, version))
			{
				*p << "Identify failed\n";
				(*it)->PowerOn(false);
				continue;
			}

			*p << "vendor " << vendor << " product " << product << " version " << version << "\n";
			dev.DumpDescriptors(*p);

			USB::UsbHub *d = new USB::UsbHub(dev);
			if (d->Valid() && d->FullInit())
			{
				*p << "it's a hub!\n";
				d->DumpDescriptors(*p);

				devices.push_back(d);

				*p << "adding " << d->GetNumPorts() << " ports\n";

				for (unsigned int count = 0; count < d->GetNumPorts(); count++)
					ports.push_back(d->GetPort(count));
			}
			else
			{
				delete d;
				*p << "not a hub\n\n";

//				(*it)->PowerOn(false);
/*
				USB::UsbDevice *def = new USB::UsbDevice(dev);

				ASSERT(def);
				ASSERT(def->Valid());
				dev.Cloned();

				devices.push_back(def);*/
			}
		}
		else
		{
			(*it)->PowerOn(false);
			*p << "no device found on port\n\n";
		}
	}

	*p << "done\n";
	while(1);


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

	pTimer0->Enable(true);

//	dss->EnableInterrupt(true);

	pThread->Run();

	ASSERT(0);
	while(1);
}
