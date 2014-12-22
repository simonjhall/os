/*
 * PrintfPrinter.h
 *
 *  Created on: 29 Nov 2014
 *      Author: simon
 */

#ifndef PRINTFPRINTER_H_
#define PRINTFPRINTER_H_

#include "print.h"

class PrintfPrinter : public Printer
{
public:
	PrintfPrinter();
	virtual ~PrintfPrinter();

	virtual void PrintChar(char c);
};

#endif /* PRINTFPRINTER_H_ */
