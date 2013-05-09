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
	PrinterUart(void) : sm_pTxAddress ((volatile unsigned int *)0x101f1000) {};
	virtual ~PrinterUart(void) {};

	virtual void PrintChar(char c);
private:
	volatile unsigned int *sm_pTxAddress;
};


#endif /* PRINT_UART_H_ */
