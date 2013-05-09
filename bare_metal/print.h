/*
 * print.h
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#ifndef PRINT_H_
#define PRINT_H_

class Printer
{
public:
	virtual ~Printer(void) {};

	virtual void PrintString(const char *pString);

	virtual void Print(unsigned int i);
	virtual void Print(int i) { Print((unsigned int)i); };

	virtual void PrintChar(char c) = 0;
private:
	virtual char *NibbleToHexString(char *, unsigned int);
};

#endif /* PRINT_H_ */
