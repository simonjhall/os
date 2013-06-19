/*
 * print.cpp
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#include "print.h"

#include <stdio.h>

void Printer::PrintString(const char *pString, bool with_len, size_t len)
{
	const char *p = pString;


	if (with_len)
	{
		for (size_t count = 0; count < len; count++)
		{
			char c = p[count];
			if (c == '\n')
			{
				PrintChar('\r');
				PrintChar('\n');
			}
			else
				PrintChar(c);
		}
	}
	else
		while (*p)
		{
			char c = *p++;
			if (c == '\n')
			{
				PrintChar('\r');
				PrintChar('\n');
			}
			else
				PrintChar(c);
	}
}

char *Printer::NibbleToHexString(char *pIn, unsigned int a)
{
	if (a < 10)
		*pIn = a + '0';
	else
		*pIn = (a - 10) + 'a';

	return pIn + 1;
}

void Printer::Print(unsigned int i)
{
	char temp[9];

	for (unsigned int count = 0; count < 4; count++)
	{
		unsigned int a = (i >> ((3 - count) * 8 + 4)) & 0xf;
		NibbleToHexString(&temp[count * 2], a);

		unsigned int b = (i >> ((3 - count) * 8)) & 0xf;
		NibbleToHexString(&temp[count * 2 + 1], b);
	}

	temp[8] = 0;

	PrintString(temp);
}


