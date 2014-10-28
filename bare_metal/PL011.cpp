/*
 * PL011.cpp
 *
 *  Created on: 25 Oct 2014
 *      Author: Simon
 */

#include "PL011.h"

namespace VersatilePb
{

PL011::PL011(volatile unsigned int *pBase)
: InterruptSource(),
  m_pBase(pBase),
  m_tempDisabledIrq(false)
{
}

PL011::~PL011()
{
}

void PL011::EnableInterrupt(bool e)
{
	if (e)
		m_pBase[sm_intMaskSetClear] |= 1 << 4;			//receive interrupt
	else
		m_pBase[sm_intMaskSetClear] &= ~(1 << 4);
}


unsigned int PL011::GetInterruptNumber(void)
{
	return 12;
}


bool PL011::HasInterruptFired(void)
{
	return m_pBase[sm_intMaskedStatus] & (1 << 4);
}


void PL011::ClearInterrupt(void)
{
	m_pBase[sm_intClear] = 1 << 4;
}

bool PL011::HandleInterrupt(void)
{
	if (InterruptSource::HandleInterrupt())
		return true;

	HandleInterruptDisable();
	return true;
}

bool PL011::HandleInterruptDisable(void)
{
	if (!RxData())
		return false;			//it's from before

	ASSERT(!m_tempDisabledIrq);

	EnableInterrupt(false);
	m_tempDisabledIrq = true;

	ClearInterrupt();
	return true;
}

bool PL011::TxFull(void)
{
	volatile unsigned int *p = m_pBase;
	unsigned int transmit_full = (p[sm_flag] >> 5) & 1;

	if (transmit_full)
		return true;
	else
		return false;
}

bool PL011::RxData(void)
{
	volatile unsigned int *p = m_pBase;
	unsigned int receive_empty = (p[sm_flag] >> 4) & 1;

	if (receive_empty)
		return false;
	else
		return true;
}

bool PL011::WriteByte(unsigned char b)
{
	volatile unsigned int *p = m_pBase;

	if (TxFull())
		return false;

	if (m_tempDisabledIrq)
	{
		m_tempDisabledIrq = false;
		EnableInterrupt(true);
	}

	p[sm_data] = b;
	return true;
}

bool PL011::ReadByte(unsigned char &b)
{
	volatile unsigned int *p = m_pBase;

	if (!RxData())
		return false;

	b = p[sm_data];

	if (m_tempDisabledIrq)
	{
		m_tempDisabledIrq = false;
		EnableInterrupt(true);
	}

	return true;
}

}
