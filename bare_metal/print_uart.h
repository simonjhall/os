/*
 * print_uart.h
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#ifndef PRINT_UART_H_
#define PRINT_UART_H_

#include "print.h"

class PrinterUart : public Printer
{
public:
#ifdef PBES
	PrinterUart(void) : sm_pTxAddress ((volatile unsigned int *)0x48020000) {};
#else
	PrinterUart(void) : sm_pTxAddress ((volatile unsigned int *)0x101f1000) {};
#endif
	virtual ~PrinterUart(void) {};

	virtual void PrintChar(char c);
private:
	void Wait(void);
	volatile unsigned int *sm_pTxAddress;
};


#endif /* PRINT_UART_H_ */
