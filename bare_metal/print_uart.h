/*
 * print_uart.h
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#ifndef PRINT_UART_H_
#define PRINT_UART_H_

#include "print.h"


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
