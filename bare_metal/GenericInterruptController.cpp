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
	m_maxInterrupt = max;

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

	//set all CPUs receive interrupts
	p << "CPU targets\n";
	for (unsigned int count = 0; count < (max >> 2); count++)
	{
		p << "ICDIPTR " << count << " " ;
		m_pDistBase[sm_icdiptrBase + count] = 0;
		p << m_pDistBase[sm_icdiptrBase + count] << " ";
		m_pDistBase[sm_icdiptrBase + count] = 0xffffffff;
		p << m_pDistBase[sm_icdiptrBase + count] << "\n";
	}

	p << "enabling forwarding\n";
	EnableForwarding(true);
	p << "enable receiving\n";
	EnableReceiving(true);
	//may not be ready to handle interrupts yet!
}

GenericInterruptController::~GenericInterruptController()
{
	// TODO Auto-generated destructor stub
}

bool GenericInterruptController::Enable(InterruptType interruptType,
		unsigned int i, bool e)
{
	if (i >= m_maxInterrupt)
		return false;

//	PrinterUart<PL011> p;
//	p << "enabling interrupt " << i << "\n";

	//find which word it is in
	unsigned int w = i >> 5;
	//find which interrupt within a word it is
	unsigned int b = i - (w << 5);

//	p << "word " << w << " bit " << b << "\n";

	//enable it with icdiser
	if (e)
	{
		m_pDistBase[sm_icdiserBase + w] |= (1 << b);

		//todo include cpu id too
	}
	//disable with icdicer
	else
		m_pDistBase[sm_icdicerBase + w] |= (1 << b);

//	p << "success\n";
	return true;
}

void GenericInterruptController::Clear(unsigned int i)
{
	//todo include cpu mask too
	m_pProcBase[sm_icceoir] = i;
}

bool GenericInterruptController::SoftwareInterrupt(unsigned int i)
{
	if (i < 16)
		m_pDistBase[sm_icdsgir] = i | (1 << 16);		//and send it to cpu 0
	else
		return false;

	return true;
}

int GenericInterruptController::GetFiredId(void)
{
	return m_pProcBase[sm_icciar] & 1023;			//todo return CPU id too
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
