/*
 * GenericInterruptController.cpp
 *
 *  Created on: 23 Jun 2013
 *      Author: simon
 */

#include "GenericInterruptController.h"
#include "common.h"
#include "print_uart.h"

namespace CortexA9MPCore
{

GenericInterruptController::GenericInterruptController(
		volatile unsigned int* pProcBase, volatile unsigned int* pDistBase)
: InterruptController(),
  m_pProcBase(pProcBase), m_pDistBase(pDistBase)
{
	PrinterUart<PL011> p;
	//see which interrupts work

	//disable forwarding of interrupts to CPU interfaces
	p << "disabling forwarding\n";
	EnableForwarding(false);

	//find the max number of interrupts the the GIC supports
	unsigned int max = ((m_pDistBase[sm_icdictr] & 31) + 1) * 32;
	p << "supports max " << max << " lines\n";

	//test which interrupts can be enabled
	p << "what goes high\n";
	for (unsigned int count = 0; count < (max >> 5); count++)
	{
		p << "ICDISER " << count;
		m_pDistBase[sm_icdiserBase + count] = 0xffffffff;
		p << " working " << m_pDistBase[sm_icdiserBase + count] << "\n";
	}
	//test which interrupts are always enabled
	p << "what goes low\n";
	for (unsigned int count = 0; count < (max >> 5); count++)
	{
		p << "ICDICER " << count;
		m_pDistBase[sm_icdicerBase + count] = 0xffffffff;
		p << " working " << m_pDistBase[sm_icdicerBase + count] << "\n";
	}

	p << "enabling forwarding\n";
	EnableForwarding(true);
	p << "enable receiving\n";
	EnableReceiving(true);
}

GenericInterruptController::~GenericInterruptController()
{
	// TODO Auto-generated destructor stub
}

bool GenericInterruptController::Enable(InterruptType interruptType,
		unsigned int i, bool e)
{
	//find which word it is in
	unsigned int w = i >> 5;
	//find which interrupt within a word it is
	i = i - (w << 5);

	//enable it with icdiser
	if (e)
		m_pDistBase[sm_icdiserBase + w] |= (1 << i);
	//disable with icdicer
	else
		m_pDistBase[sm_icdicerBase + w] |= (1 << i);

	return true;
}

void GenericInterruptController::Clear(unsigned int unsignedInt)
{
}

void GenericInterruptController::SoftwareInterrupt(unsigned int i)
{
	m_pDistBase[sm_icdsgir] = i;
}

unsigned int GenericInterruptController::GetFiredMask(void)
{
	return 1 << m_pProcBase[sm_icciar];
}

void GenericInterruptController::EnableForwarding(bool e)
{
	m_pDistBase[sm_icddcr] = (m_pDistBase[sm_icddcr] & ~1) | (unsigned int)e;
}

void GenericInterruptController::EnableReceiving(bool e)
{
	m_pProcBase[sm_iccicr] = (m_pProcBase[sm_iccicr] & ~1) | (unsigned int)e;
}



} /* namespace CortexA9MPCore */
