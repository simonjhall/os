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
#ifdef PBES
				PrintChar('\r');
#endif
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
#ifdef PBES
				PrintChar('\r');
#endif
				PrintChar('\n');
			}
			else
				PrintChar(c);
	}
}

void Printer::PrintDec(unsigned int i, bool leading)
{
	bool has_printed = leading;
	for (int count = 9; count >= 0; count--)
	{
		unsigned int divide_by = 1;
		for (unsigned int inner = 0; inner < count; inner++)
			divide_by *= 10;

		unsigned int div = i / divide_by;
		i = i % divide_by;

		if (div || has_printed)
		{
			has_printed = true;
			PrintChar(div + '0');
		}
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

void Printer::PrintHex(unsigned int i)
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

void Printer::PrintHex(unsigned long long i)
{
	char temp[17];

	for (unsigned int count = 0; count < 8; count++)
	{
		unsigned int a = (i >> ((7 - count) * 8 + 4)) & 0xf;
		NibbleToHexString(&temp[count * 2], a);

		unsigned int b = (i >> ((7 - count) * 8)) & 0xf;
		NibbleToHexString(&temp[count * 2 + 1], b);
	}

	temp[16] = 0;

	PrintString(temp);
}


