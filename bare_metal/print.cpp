/*
 * print.cpp
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#include "print.h"

#include <stdio.h>

void Printer::PrintString(const char *pString)
{
	const char *p = pString;

	while (*p)
			PrintChar(*p++);
}

char *Printer::NibbleToHexString(char *pIn, unsigned int a)
{
	switch (a)
	{
		case 0:
			*pIn = '0'; break;
		case 1:
			*pIn = '1'; break;
		case 2:
			*pIn = '2'; break;
		case 3:
			*pIn = '3'; break;
		case 4:
			*pIn = '4'; break;
		case 5:
			*pIn = '5'; break;
		case 6:
			*pIn = '6'; break;
		case 7:
			*pIn = '7'; break;
		case 8:
			*pIn = '8'; break;
		case 9:
			*pIn = '9'; break;
		case 10:
			*pIn = 'a'; break;
		case 11:
			*pIn = 'b'; break;
		case 12:
			*pIn = 'c'; break;
		case 13:
			*pIn = 'd'; break;
		case 14:
			*pIn = 'e'; break;
		case 15:
			*pIn = 'f'; break;
		default:
			break;
	}

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


