/*
 * UART.cpp
 *
 *  Created on: 8 Sep 2014
 *      Author: simon
 */

#include "UART.h"
#include "common.h"

namespace OMAP4460
{

UART::UART(volatile unsigned int* pBase)
: InterruptSource(), Printer(), GenericByteTxRx(),
  m_pBase(pBase)
{
}

UART::~UART()
{
	// TODO Auto-generated destructor stub
}

void UART::EnableFifo(bool e)
{
	m_pBase[sm_fcr >> 2] = (m_pBase[sm_fcr >> 2] & ~1) | (unsigned int)e;
}

void UART::EnableInterrupt(bool e)
{
}

unsigned int UART::GetInterruptNumber(void)
{
	return 74 + 32;
}

bool UART::HasInterruptFired(void)
{
	return false;
}

void UART::ClearInterrupt(void)
{
}

bool UART::ReadByte(unsigned char& b)
{
	if (RxData())
	{
		b = ReadRx();
		return true;
	}
	return false;
}

bool UART::WriteByte(unsigned char b)
{
	if (TxFull())
		return false;

	WriteTx(b);
	return true;
}

bool UART::TxFull(void)
{
	if (m_pBase[sm_ssr >> 2] & 1)
		return true;
	else
		return false;
}

unsigned char UART::ReadRx(void)
{
	return m_pBase[sm_rhr >> 2];
}

bool UART::RxData(void)
{
	if (m_pBase[sm_lsr >> 2] & 1)
		return true;
	else
		return false;
}

void UART::WriteTx(unsigned char c)
{
	m_pBase[sm_thr >> 2] = c;
}

} /* namespace OMAP4460 */

GenericByteTxRx::GenericByteTxRx()
#ifdef __ARM_ARCH_7A__
: m_rxWait(0), m_txWait(0)
#endif
{
}

GenericByteTxRx::~GenericByteTxRx()
{
}
