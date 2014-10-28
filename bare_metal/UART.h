/*
 * UART.h
 *
 *  Created on: 8 Sep 2014
 *      Author: simon
 */

#ifndef UART_H_
#define UART_H_

#include "InterruptSource.h"
#include "print.h"
#ifdef __ARM_ARCH_7A__
#include "Process.h"
#endif

class GenericByteTxRx
{
public:
	GenericByteTxRx();
	virtual ~GenericByteTxRx();

	virtual bool ReadByte(unsigned char &b) = 0;
	virtual bool WriteByte(unsigned char b) = 0;

	virtual bool TxFull(void) = 0;
	virtual bool RxData(void) = 0;

#ifdef __ARM_ARCH_7A__
protected:
	Semaphore m_rxWait;
	Semaphore m_txWait;
public:
	inline Semaphore &GetRxWait(void) { return m_rxWait; }
	inline Semaphore &GetTxWait(void) { return m_txWait; }
#endif
};

namespace OMAP4460
{

class UART : public InterruptSource, public Printer, public GenericByteTxRx
{
public:
	UART(volatile unsigned int *pBase);
	virtual ~UART();

	void EnableFifo(bool e);

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	virtual bool ReadByte(unsigned char &b);
	virtual bool WriteByte(unsigned char b);

	virtual void PrintChar(char c)
	{
		while (WriteByte(c) == false);
	}

	virtual bool TxFull(void);
	virtual bool RxData(void);

protected:

	unsigned char ReadRx(void);
	void WriteTx(unsigned char c);

	static const unsigned int sm_rhr = 0x0;
	static const unsigned int sm_thr = 0x0;
	static const unsigned int sm_fcr = 0x8;
	static const unsigned int sm_lsr = 0x14;
	static const unsigned int sm_ssr = 0x44;

	volatile unsigned int *m_pBase;
};

} /* namespace OMAP4460 */
#endif /* UART_H_ */
