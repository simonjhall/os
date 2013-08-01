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

namespace USB
{

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

	if (!VirtMem::AllocAndMapVirtContig((void *)v, 6, true, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerNc, 0))
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
	v += 4096;

	m_qtdAllocator.Init((QTD *)v);
	v += 4096;
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

	for (unsigned int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			p << "port " << count << " " << m_pOps->m_portsSc[count] << "\n";

	//turn on power to all ports (if available)
	if ((m_pCaps->m_hcsParams >> 4) & 1)
		for (unsigned int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			m_pOps->m_portsSc[count] |= (1 << 12);

	while (1)
	{
		for (unsigned int count = 0; count < ((m_pCaps->m_hcsParams >> 0) & 0xf); count++)
			p << "port " << count << " " << m_pOps->m_portsSc[count] << "\n";
		DelaySecond();
	}
}

FrameListElement::FrameListElement(void)
{
	//t bit
	m_word = 1;
}

FrameListElement::FrameListElement(ITD *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kIsochronousTransferDescriptor << 1;
}

FrameListElement::FrameListElement(QH *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kQueueHead << 1;
}

FrameListElement::FrameListElement(SITD *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kSplitTransationIsochronousTransferDescriptor << 1;
}

FrameListElement::FrameListElement(FSTN *p)
{
	ASSERT(((unsigned int)p & 31) == 0);
	m_word = (unsigned int)p | (unsigned int)kFrameSpanTraversalNode << 1;
}

FrameListElement::operator ITD*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kIsochronousTransferDescriptor);
	return (ITD *)(m_word & ~31);
}

FrameListElement::operator QH*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kQueueHead);
	return (QH *)(m_word & ~31);
}

FrameListElement::operator SITD*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kSplitTransationIsochronousTransferDescriptor);
	return (SITD *)(m_word & ~31);
}

FrameListElement::operator FSTN*() const
{
	ASSERT((m_word & 1) == 0);
	ASSERT((Type)((m_word >> 1) & 3) == kFrameSpanTraversalNode);
	return (FSTN *)(m_word & ~31);
}

void Ehci::EnableAsync(bool e)
{
	if (e)
	{
		if (!(m_pOps->m_usbCmd & (1 << 5)))
			m_pOps->m_usbCmd |= (1 << 5);			//enable it

		//wait for it to come up
		while (!(m_pOps->m_usbSts & (1 << 15)));
	}
	else
	{
		if (m_pOps->m_usbCmd & (1 << 5))
			m_pOps->m_usbCmd &= ~(1 << 5);			//disable it

		//wait for it to stop
		while (m_pOps->m_usbSts & (1 << 15));
	}
}

void Ehci::EnablePeriodic(bool e)
{
	if (e)
	{
		if (!(m_pOps->m_usbCmd & (1 << 4)))
			m_pOps->m_usbCmd |= (1 << 4);			//enable it

		//wait for it to come up
		while (!(m_pOps->m_usbSts & (1 << 14)));
	}
	else
	{
		if (m_pOps->m_usbCmd & (1 << 4))
			m_pOps->m_usbCmd &= ~(1 << 4);			//disable it

		//wait for it to stop
		while (m_pOps->m_usbSts & (1 << 14));
	}
}

QTD::QTD(QTD* pVirtNext, bool nextTerm, QTD* pVirtAltNext, bool altNextTerm,
		bool dataToggle, unsigned int totalBytes, bool interruptOnComplete,
		unsigned int currentPage, unsigned int errorCounter, Pid pid,
		unsigned int status, void* pVirtBuffer, unsigned int bufferLength)
{
	QTD *pPhysNext, *pPhysAltNext;

	//virt to phys on the qtd pointers
	if (pVirtNext)
	{
		ASSERT(((unsigned int)pVirtNext & 31) == 0);
		if (!VirtMem::VirtToPhys(pVirtNext, &pPhysNext))
			ASSERT(0);
		ASSERT(!nextTerm);
	}
	else
	{
		pPhysNext = 0;
		ASSERT(nextTerm);
	}

	if (pVirtAltNext)
	{
		ASSERT(((unsigned int)pVirtAltNext & 31) == 0);
		if (!VirtMem::VirtToPhys(pVirtAltNext, &pPhysAltNext))
			ASSERT(0);
		ASSERT(!altNextTerm);
	}
	else
	{
		pPhysAltNext = 0;
		ASSERT(altNextTerm);
	}

	ASSERT((totalBytes - ((unsigned int)pVirtBuffer & 4095)) <= 0x5000);
	ASSERT(currentPage <= 4);
	ASSERT(errorCounter <= 3);
	ASSERT(status <= 0xff);

	unsigned int pidCode = 0;
	switch (pid)
	{
	case kOut: pidCode = 0; break;
	case kIn: pidCode = 1; break;
	case kSetup: pidCode = 2; break;
	default: ASSERT(0);
	}

	m_words[0] = (unsigned int)pPhysNext | (unsigned int)nextTerm;
	m_words[1] = (unsigned int)pPhysAltNext | (unsigned int)altNextTerm;
	m_words[2] = ((unsigned int)dataToggle << 31) | (totalBytes << 16) | ((unsigned int)interruptOnComplete << 15) | (currentPage << 12)
			| (errorCounter << 10) | (pidCode << 8) | status;

	//zero the buffer slots
	for (int count = 0; count < 5; count++)
		m_words[3 + count] = 0;

	//fill in the misalignment
	m_words[3] = (unsigned int)pVirtBuffer & 4095;

	//convert the buffer virtual address to physical page addresses
	char *pVirtAlignedBuffer = (char *)((unsigned int)pVirtBuffer & ~4095);

	//for each whole page, rounded up
	for (unsigned int page = 0; page < ((totalBytes + 4095) & ~4095); page++)
	{
		char *pPhys;
		if (!VirtMem::VirtToPhys(&pVirtAlignedBuffer[page * 4096], &pPhys))
			ASSERT(0);

		m_words[3 + page] |= (unsigned int)pPhys;
	}
}

unsigned int QTD::GetBytesToTransfer(void)
{
	return (m_words[2] >> 16) & 0x7fff;
}

unsigned int QTD::GetErrorCount(void)
{
	return (m_words[2] >> 10) & 3;
}

unsigned int QTD::GetStatus(void)
{
	return m_words[2] & 0xff;
}

}
