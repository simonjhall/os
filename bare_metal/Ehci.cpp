/*
 * Ehci.cpp
 *
 *  Created on: 29 Jul 2013
 *      Author: simon
 */

#include "Ehci.h"
#include "UsbDevice.h"

#include "print_uart.h"
#include "common.h"
#include "virt_memory.h"

#include <string.h>

namespace USB
{

Ehci::Ehci(volatile void *pBase)
: m_pCaps((volatile Caps *)pBase),
  m_pOps((volatile Ops *)((unsigned int)pBase + m_pCaps->m_capLength)),
  m_rootHub(*this)
{
	Printer &p = Printer::Get();

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

bool Ehci::Initialise(void)
{
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

	return true;
}

void Ehci::Shutdown(void)
{
}

bool Ehci::SubmitControlMessage(EndPoint& rEndPoint, UsbDevice& rDevice,
		void* pBuffer, unsigned int length, SetupPacket setupPacket)
{
	//http://www.usbmadesimple.co.uk/ums_4.htm
	QH *pQh, *pPhysQh;
	QTD *pQtdSetup, *pQtdData;

	if (!m_qhAllocator.Allocate(&pQh))
		ASSERT(0);

	if (!VirtMem::VirtToPhys(pQh, &pPhysQh))
		ASSERT(0);

	if (!m_qtdAllocator.Allocate(&pQtdSetup))
		ASSERT(0);
	if (!m_qtdAllocator.Allocate(&pQtdData))
		ASSERT(0);

	Printer &p = Printer::Get();
	if (rDevice.GetSpeed() != kHighSpeed)
		p << "device speed is " << rDevice.GetSpeed() << "\n";

	ASSERT(rDevice.GetSpeed() == kHighSpeed);
//
//	p << "endpoint size " << rEndPoint.m_descriptor.m_maxPacketSize << " addr " <<
//			rEndPoint.m_descriptor.m_endpointAddress << " dev addr " << rDevice.GetAddress() << "\n";

	pQh->Init(FrameListElement(pQh, FrameListElement::kQueueHead),
			0, false,		//high-speed? then false
			rEndPoint.m_descriptor.m_maxPacketSize,
			true,			//head of reclamation
			true,			//data toggle from incoming qtd
			kHighSpeed,
			rEndPoint.m_descriptor.m_endpointAddress,
			false,			//cannot be true in async queue
			rDevice.GetAddress(),
			1,				//multiplier must be at least one, but is ignored in async
			0, 0, 0,		//ignored
			0,				//zero on async
			pQtdSetup);

	pQtdSetup->Init(pQtdData, false, 0, true,
			false,				//data0
			sizeof(SetupPacket),
			false,
			0,
			1,					//1 error max
			kSetup,
			1 << 7,				//active status
			&setupPacket);

	pQtdData->Init(0, true, 0, true,
			true,				//data1
			length,
			false,
			0,
			1,					//1 error max
			kIn,
			1 << 7,				//active status
			pBuffer);

	EnableAsync(false);

	m_pOps->m_asyncListAddr = (unsigned int)pPhysQh;

	EnableAsync(true);
	EnableAsync(false);

	bool failed = false;

	if ((pQtdSetup->GetStatus() & (1 << 6)) || (pQtdData->GetStatus() & (1 << 6)))
	{
		p << "qtd error for device " << rDevice.GetAddress() << "\n";
		p << "qtd setup status " << pQtdSetup->GetStatus() << "\n";
		p << "qtd data status " << pQtdData->GetStatus() << "\n";

		failed = true;
	}

	m_qtdAllocator.Free(pQtdData);
	m_qtdAllocator.Free(pQtdSetup);
	m_qhAllocator.Free(pQh);

	return !failed;
}

Hub& Ehci::GetRootHub(void)
{
	return m_rootHub;
}

void Ehci::PortPower(unsigned int p, bool o)
{
	if (!((m_pCaps->m_hcsParams >> 4) & 1))
		return;			//no port power

	if (o)
	{
		m_pOps->m_portsSc[p] |= (1 << 12);

		for (int count = 0; count < 64; count++)
			DelayMillisecond();
	}
	else
		m_pOps->m_portsSc[p] &= ~(1 << 12);
}

bool Ehci::IsPortPowered(unsigned int p)
{
	if (!((m_pCaps->m_hcsParams >> 4) & 1))
		return true;			//no port power

	return m_pOps->m_portsSc[p] & (1 << 12);
}

bool Ehci::IsDeviceAttached(unsigned int p)
{
	return m_pOps->m_portsSc[p] & 1;
}

void Ehci::PortReset(unsigned int p)
{
	m_pOps->m_portsSc[p] |= (1 << 8);

	for (int i = 0; i < 64; i++)
		DelayMillisecond();

	m_pOps->m_portsSc[p] &= ~(1 << 8);

	for (int i = 0; i < 64; i++)
		DelayMillisecond();
}

FrameListElement::FrameListElement(void)
{
	//t bit
	m_word = 1;
}

FrameListElement::FrameListElement(void *p, Type t)
{
	ASSERT(((unsigned int)p & 31) == 0);
	void *phys;
	if (!VirtMem::VirtToPhys(p, &phys))
		ASSERT(0);
	m_word = (unsigned int)phys | (unsigned int)t << 1;
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
		unsigned int status, void* pVirtBuffer)
{
	Init(pVirtNext, nextTerm, pVirtAltNext, altNextTerm, dataToggle, totalBytes, interruptOnComplete,
			currentPage, errorCounter, pid, status, pVirtBuffer);
}

void QTD::Init(QTD* pVirtNext, bool nextTerm, QTD* pVirtAltNext, bool altNextTerm,
		bool dataToggle, unsigned int totalBytes, bool interruptOnComplete,
		unsigned int currentPage, unsigned int errorCounter, Pid pid,
		unsigned int status, void* pVirtBuffer)
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

	ASSERT((totalBytes <= 0x5000 - ((unsigned int)pVirtBuffer & 4095)));
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

	if (totalBytes)
		//for each whole page, rounded up
		for (unsigned int page = 0; page < (((totalBytes + 4095) & ~4095) >> 12); page++)
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

QTD::QTD(void)
{
	ASSERT(sizeof(m_words) == sizeof(int) * 8);
	for (int count = 0; count < 8; count++)
		m_words[count] = 0;
}

unsigned int QTD::GetStatus(void)
{
	return m_words[2] & 0xff;
}

QH::QH(FrameListElement link, unsigned int nakReload, bool controlEndpoint,
		unsigned int maxPacketLength, bool headOfList, bool dataToggleControl,
		Speed eps, unsigned int endPt, bool inactiveOnNext,
		unsigned int deviceAddr, unsigned int multiplier,
		unsigned int portNumber, unsigned int hubAddr,
		unsigned int splitCompletionMask, unsigned int interruptSchedMask,
		volatile QTD* pCurrent)
{
	Init(link, nakReload, controlEndpoint, maxPacketLength, headOfList, dataToggleControl,
			eps, endPt, inactiveOnNext, deviceAddr, multiplier, portNumber, hubAddr,
			splitCompletionMask, interruptSchedMask, pCurrent);
}

void QH::Init(FrameListElement link, unsigned int nakReload, bool controlEndpoint,
		unsigned int maxPacketLength, bool headOfList, bool dataToggleControl,
		Speed eps, unsigned int endPt, bool inactiveOnNext,
		unsigned int deviceAddr, unsigned int multiplier,
		unsigned int portNumber, unsigned int hubAddr,
		unsigned int splitCompletionMask, unsigned int interruptSchedMask,
		volatile QTD* pCurrent)
{
	m_fle = link;
	memset((void *)&m_overlay, 0, sizeof(m_overlay));

	m_words[0] = (nakReload << 28) | ((unsigned int)controlEndpoint << 27) | (maxPacketLength << 16) | ((unsigned int)headOfList << 15)
			| ((unsigned int)dataToggleControl << 14) | (endPt << 8) | ((unsigned int)inactiveOnNext << 7) | deviceAddr;
	switch (eps)
	{
	case kFullSpeed:
		break;
	case kLowSpeed:
		m_words[0] |= (1 << 12); break;
	case kHighSpeed:
		m_words[0] |= (2 << 12); break;
	default:
		ASSERT(0);
	}
	ASSERT(multiplier != 0);
	m_words[1] = (multiplier << 30) | (portNumber << 23) | (hubAddr << 16) | (splitCompletionMask << 8) | interruptSchedMask;

	ASSERT(pCurrent);

	volatile QTD *pPhysCurrent;

	ASSERT(((unsigned int)pCurrent & 31) == 0);
	if (!VirtMem::VirtToPhys(pCurrent, &pPhysCurrent))
		ASSERT(0);

	m_pCurrent = 0;
	m_overlay.m_words[0] = (unsigned int)pPhysCurrent;
}

}
