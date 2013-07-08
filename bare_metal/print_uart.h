/*
 * print_uart.h
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#ifndef PRINT_UART_H_
#define PRINT_UART_H_

#include "print.h"

class PL011
{
public:
	static void EnableUart(bool e)
	{
		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;

		unsigned int current = p[sm_control];
		current = current & ~1;						//deselect enable/disable
		current = current | (unsigned int)e;		//add in
		p[sm_control] = current;
	}

	static bool IsUartEnabled(void)
	{
		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;

		unsigned int current = p[sm_control];
		return (bool)(current & 1);
	}

	static void EnableFifo(bool e)
	{
		bool existing = IsUartEnabled();
		EnableUart(false);

		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;

		unsigned int current = p[sm_lineControl];
		current = current & ~(1 << 4);					//deselect the fifo enable bit
		current = current | ((unsigned int)e << 4);		//add in our option
		p[sm_lineControl] = current;

		EnableUart(existing);
	}

	static bool IsFifoEnabled(void)
	{
		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;
		unsigned int current = p[sm_lineControl];
		return (bool)((current >> 4) & 1);
	}

	static bool WriteByte(unsigned char b)
	{
		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;

#ifdef PBES
		while (p[0x44 >> 2] & 1);
#else
		unsigned int transmit_full = (p[sm_flag] >> 5) & 1;
		if (transmit_full)
			return false;
#endif

		p[sm_data] = b;
		return true;
	}

	static bool ReadByte(unsigned char &b)
	{
		volatile unsigned int *p = (volatile unsigned int *)sm_baseAddress;

		unsigned int receive_empty = (p[sm_flag] >> 4) & 1;
		if (receive_empty)
			return false;

		b = p[sm_data];
		return true;
	}

private:
#ifdef PBES
	static const unsigned int sm_baseAddress = 0xfd020000;
//	static const unsigned int sm_baseAddress = 0x48020000;
#else
	static const unsigned int sm_baseAddress = 0xfd0f1000;
#endif
	//offsets in words
	static const unsigned int sm_data = 0;
	static const unsigned int sm_receiveStatusErrorClear = 1;
	static const unsigned int sm_flag = 6;
	static const unsigned int sm_lineControl = 11;
	static const unsigned int sm_control = 12;
};

template <class T>
class PrinterUart : public Printer
{
public:

	virtual ~PrinterUart(void) {};

	virtual void PrintChar(char c)
	{
		while (T::WriteByte(c) == false);
	}
private:
};


#endif /* PRINT_UART_H_ */
