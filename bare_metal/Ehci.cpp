/*
 * Ehci.cpp
 *
 *  Created on: 29 Jul 2013
 *      Author: simon
 */

#include "Ehci.h"
#include "print_uart.h"
#include "common.h"
#include "virt_memory.h"

Ehci::Ehci(volatile void *pBase)
{
	PrinterUart<PL011> p;

	m_pCaps = (volatile Caps *)pBase;
	m_pOps = (volatile Ops *)((unsigned int)pBase + m_pCaps->m_capLength);

	p << "cap length " << m_pCaps->m_capLength << "\n";
	p << "hci version " << m_pCaps->m_hciVersion << "\n";
	p << "port indicator control " << ((m_pCaps->m_hcsParams >> 16) & 1)
			<< " companion controllers " << ((m_pCaps->m_hcsParams >> 12) & 0xf)
			<< " ports per companion controller " << ((m_pCaps->m_hcsParams >> 8) & 0xf)
			<< " port routing rules " << ((m_pCaps->m_hcsParams >> 7) & 1)
			<< " port power control " << ((m_pCaps->m_hcsParams >> 4) & 1)
			<< " num ports " << ((m_pCaps->m_hcsParams >> 0) & 0xf) << "\n";
	p << "eecp " << ((m_pCaps->m_hccParams >> 8) & 0xff)
			<< " iso sched thresh " << ((m_pCaps->m_hccParams >> 4) & 0xf)
			<< " async park cap " << ((m_pCaps->m_hccParams >> 2) & 1)
			<< " programmable frame list flag " << ((m_pCaps->m_hccParams >> 1) & 1)
			<< " 64-bit cap " << ((m_pCaps->m_hccParams >> 0) & 1) << "\n";

	unsigned int v = 0xb0000000;

	if (!VirtMem::AllocAndMapVirtContig((void *)v, 5, true, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerNc, 0))
		ASSERT(0);

	m_pPeriodicFrameList = (FrameListElement *)v;
	v += 4096;

	m_itdAllocator.Init((ITD *)v);
	v += 4096;

	m_qhAllocator.Init((QH *)v);
	v += 4096;

	m_sitdAllocator.Init((SITD *)v);
	v += 4096;

	m_fstnAllocator.Init((FSTN *)v);
}

Ehci::~Ehci()
{
	//needs to free memory
}

void Ehci::Initialise(void)
{
	PrinterUart<PL011> p;

	//reset the controller
	ASSERT(((m_pOps->m_usbSts >> 12) & 1) == 1);
	m_pOps->m_usbCmd = 1 << 1;

	while (m_pOps->m_usbCmd & (1 << 1));

	//32-bit please
	m_pOps->m_ctrlDsSegment = 0;

	//turn off all interrupts
	m_pOps->m_usbIntr = 0;

	//fill in periodic frame list
	//initialise to ignore
	for (int count = 0; count < 1024; count++)
		m_pPeriodicFrameList[count] = FrameListElement();

	FrameListElement *pPhysPFL;
	if (!VirtMem::VirtToPhys(m_pPeriodicFrameList, &pPhysPFL))
		ASSERT(0);

	m_pOps->m_periodicListBase = pPhysPFL;

	//clear async park
	m_pOps->m_usbCmd &= ~(1 << 11);
	//turn on the controller
	m_pOps->m_usbCmd |= 1;

	//make all ports be owned by this controller (ehci)
	m_pOps->m_configFlag = 1;

	for (int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			p << "port " << count << " " << m_pOps->m_portsSc[count] << "\n";

	//turn on power to all ports (if available)
	if ((m_pCaps->m_hcsParams >> 4) & 1)
		for (int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			m_pOps->m_portsSc[count] |= (1 << 12);

	while (1)
	{
		for (int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			p << "port " << count << " " << m_pOps->m_portsSc[count] << "\n";
		DelaySecond();
	}
}

Ehci::FrameListElement::FrameListElement(void)
{
	//t bit
	m_word = 1;
}

Ehci::FrameListElement::FrameListElement(ITD *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kIsochronousTransferDescriptor << 1;
}

Ehci::FrameListElement::FrameListElement(QH *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kQueueHead << 1;
}

Ehci::FrameListElement::FrameListElement(SITD *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kSplitTransationIsochronousTransferDescriptor << 1;
}

Ehci::FrameListElement::FrameListElement(FSTN *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kFrameSpanTraversalNode << 1;
}

Ehci::FrameListElement::operator ITD*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kIsochronousTransferDescriptor);
	return (ITD *)(m_word & ~31);
}

Ehci::FrameListElement::operator QH*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kQueueHead);
	return (QH *)(m_word & ~31);
}

Ehci::FrameListElement::operator SITD*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kSplitTransationIsochronousTransferDescriptor);
	return (SITD *)(m_word & ~31);
}

Ehci::FrameListElement::operator FSTN*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kFrameSpanTraversalNode);
	return (FSTN *)(m_word & ~31);
}
