/*
 * IoSpace.cpp
 *
 *  Created on: 26 Jul 2013
 *      Author: simon
 */

#include "IoSpace.h"
#include "virt_memory.h"

#include <string.h>

IoSpace *IoSpace::m_pSpace = 0;

IoSpace::IoSpace(volatile unsigned int *pVirtBase)
: m_pVirtBase(pVirtBase)
{
}

IoSpace::~IoSpace()
{
}

IoSpace::Entry &IoSpace::Get(const char* pName)
{
	for (auto it = m_all.begin(); it != m_all.end(); it++)
		if (strcmp(it->m_pName, pName) == 0)
			return *it;

	return m_invalid;
}

void IoSpace::Map(void)
{
	volatile unsigned int *p = m_pVirtBase;

	for (auto it = m_all.begin(); it != m_all.end(); it++)
	{
		Entry &r = *it;
		r.m_pVirt = p;
		p += r.m_length >> 2;

		VirtMem::MapPhysToVirt((void *)r.m_pPhys, (void *)r.m_pVirt, r.m_length, true,
			TranslationTable::kRwRw, TranslationTable::kNoExec, TranslationTable::kShareableDevice, 0);
	}
}

void IoSpace::Fill(void)
{
	ASSERT(0);
}

OMAP4460::OmapIoSpace::OmapIoSpace(volatile unsigned int* pVirtBase)
: IoSpace(pVirtBase)
{
	Fill();
}

OMAP4460::OmapIoSpace::~OmapIoSpace(void)
{
}

void OMAP4460::OmapIoSpace::Fill(void)
{
	//page 296
	m_all.push_back(Entry("CM1", 0, (volatile unsigned int *)0x4a004000, 4096));
	m_all.push_back(Entry("CM2", 0, (volatile unsigned int *)0x4a008000, 8192));
	m_all.push_back(Entry("sDMA", 0, (volatile unsigned int *)0x4a056000, 4096));
	m_all.push_back(Entry("HSUSBTLL", 0, (volatile unsigned int *)0x4a062000, 4096));

	//page 297
	m_all.push_back(Entry("HSUSBHOST", 0, (volatile unsigned int *)0x4a064000, 4096));
	m_all.push_back(Entry("FSUSB", 0, (volatile unsigned int *)0x4a0a9000, 4096));
	m_all.push_back(Entry("HSUSBOTG", 0, (volatile unsigned int *)0x4a0ab000, 4096));
	m_all.push_back(Entry("USBPHY", 0, (volatile unsigned int *)0x4a0ad000, 4096));
	m_all.push_back(Entry("Mailbox", 0, (volatile unsigned int *)0x4a0f4000, 4096));
	m_all.push_back(Entry("Spinlock", 0, (volatile unsigned int *)0x4a0f6000, 4096));
	m_all.push_back(Entry("SYSCTRL_PADCONF_CORE", 0, (volatile unsigned int *)0x4a100000, 4096));

	//page 298
	m_all.push_back(Entry("PRM", 0, (volatile unsigned int *)0x4a306000, 8192));
	m_all.push_back(Entry("SCRM", 0, (volatile unsigned int *)0x4a30a000, 4096));

	//page 299
	m_all.push_back(Entry("GPIO1", 0, (volatile unsigned int *)0x4a310000, 4096));

	//page 300
	m_all.push_back(Entry("UART3", 0, (volatile unsigned int *)0x48020000, 4096));
	m_all.push_back(Entry("GPTIMER2", 0, (volatile unsigned int *)0x48032000, 4096));
	m_all.push_back(Entry("GPTIMER3", 0, (volatile unsigned int *)0x48034000, 4096));
	m_all.push_back(Entry("GPTIMER4", 0, (volatile unsigned int *)0x48036000, 4096));
	m_all.push_back(Entry("DSS", 0, (volatile unsigned int *)0x48040000, 64 * 1024));
	m_all.push_back(Entry("GPIO2", 0, (volatile unsigned int *)0x48055000, 4096));
	m_all.push_back(Entry("GPIO3", 0, (volatile unsigned int *)0x48057000, 4096));
	m_all.push_back(Entry("GPIO4", 0, (volatile unsigned int *)0x48059000, 4096));
	m_all.push_back(Entry("GPIO5", 0, (volatile unsigned int *)0x4805b000, 4096));
	m_all.push_back(Entry("GPIO6", 0, (volatile unsigned int *)0x4805d000, 4096));
	m_all.push_back(Entry("I2C3", 0, (volatile unsigned int *)0x48060000, 4096));
	m_all.push_back(Entry("I2C1", 0, (volatile unsigned int *)0x48070000, 4096));
	m_all.push_back(Entry("I2C2", 0, (volatile unsigned int *)0x48072000, 4096));

	//page 301
	m_all.push_back(Entry("HSMMC1", 0, (volatile unsigned int *)0x4809c000, 4096));

	//page 1125
	m_all.push_back(Entry("SCU/GIC_Proc_Interface/Timer", 0, (volatile unsigned int *)0x48240000, 4096));
	m_all.push_back(Entry("GIC_Intr_Distributor", 0, (volatile unsigned int *)0x48241000, 4096));
	m_all.push_back(Entry("PL310", 0, (volatile unsigned int *)0x48242000, 4096));
}

VersatilePb::VersIoSpace::VersIoSpace(volatile unsigned int* pVirtBase)
: IoSpace(pVirtBase)
{
	Fill();
}

VersatilePb::VersIoSpace::~VersIoSpace(void)
{
}

void VersatilePb::VersIoSpace::Fill(void)
{
	//http://infocenter.arm.com/help/topic/com.arm.doc.dui0225d/BBAJIHEC.html
	m_all.push_back(Entry("Vectored Interrupt Controller", 0, (volatile unsigned int *)0x10140000, 64 * 1024));
	m_all.push_back(Entry("UART 0 Interface", 0, (volatile unsigned int *)0x101F1000, 4096));
	m_all.push_back(Entry("Multimedia Card Interface 0 (MMCI0)", 0, (volatile unsigned int *)0x10005000, 4096));
	m_all.push_back(Entry("Timer modules 0 and 1 interface", 0, (volatile unsigned int *)0x101E2000, 4096));
	m_all.push_back(Entry("Timer modules 2 and 3 interface", 0, (volatile unsigned int *)0x101E3000, 4096));


	/* add in sysbus_create_simple("exynos4210-ehci-usb", 0x20000000, pic[31]);
	 * -usb -usbdevice disk:/home/simon/git/os/bare_metal/filesystem
	 */
	m_all.push_back(Entry("EHCI", 0, (volatile unsigned int *)0x20000000, 4096));

}
