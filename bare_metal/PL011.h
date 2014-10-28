/*
 * PL011.h
 *
 *  Created on: 25 Oct 2014
 *      Author: Simon
 */

#ifndef PL011_H_
#define PL011_H_

#include "IoSpace.h"
#include "UART.h"

namespace VersatilePb
{

class PL011 : public InterruptSource, public Printer, public GenericByteTxRx
{
public:
	PL011(volatile unsigned int *pBase);
	virtual ~PL011();

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	virtual bool HandleInterrupt(void);
	bool HandleInterruptDisable(void);

	virtual bool ReadByte(unsigned char &b);
	virtual bool WriteByte(unsigned char b);

	virtual void PrintChar(char c)
	{
		while (WriteByte(c) == false);
	}

	virtual bool TxFull(void);
	virtual bool RxData(void);

	inline void EnableUart(bool e)
	{
		volatile unsigned int *p = m_pBase;

		unsigned int current = p[sm_control];
		current = current & ~1;						//deselect enable/disable
		current = current | (unsigned int)e;		//add in
		p[sm_control] = current;
	}

	inline bool IsUartEnabled(void)
	{
		volatile unsigned int *p = m_pBase;

		unsigned int current = p[sm_control];
		return (bool)(current & 1);
	}

	inline void EnableFifo(bool e)
	{
		bool existing = IsUartEnabled();
		EnableUart(false);

		volatile unsigned int *p = m_pBase;

		unsigned int current = p[sm_lineControl];
		current = current & ~(1 << 4);					//deselect the fifo enable bit
		current = current | ((unsigned int)e << 4);		//add in our option
		p[sm_lineControl] = current;

		EnableUart(existing);
	}

	inline bool IsFifoEnabled(void)
	{
		volatile unsigned int *p = m_pBase;
		unsigned int current = p[sm_lineControl];
		return (bool)((current >> 4) & 1);
	}

private:
	volatile unsigned int *m_pBase;
	bool m_tempDisabledIrq;

	//offsets in words
	static const unsigned int sm_data = 0;
	static const unsigned int sm_receiveStatusErrorClear = 1;
	static const unsigned int sm_flag = 6;
	static const unsigned int sm_lineControl = 11;
	static const unsigned int sm_control = 12;
	static const unsigned int sm_intMaskSetClear = 0x38 >> 2;
	static const unsigned int sm_intMaskedStatus = 0x40 >> 2;
	static const unsigned int sm_intClear = 0x44 >> 2;
};

}

#endif /* PL011_H_ */
