/*
 * VectorTable.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "VectorTable.h"
#include "common.h"

namespace VectorTable
{


void EncodeAndWriteBranch(ExceptionVector v, ExceptionType e, unsigned int subtract)
{
	unsigned int *pBase = GetTableAddress();

	unsigned int target = (unsigned int)v - subtract;
	unsigned int source = (unsigned int)pBase + (unsigned int)e * 4;
	unsigned int diff = target - source - 8;

	ASSERT(((diff >> 2) & ~0xffffff) == 0);

	unsigned int b = (0xe << 28) | (10 << 24) | (((unsigned int)diff >> 2) & 0xffffff);
	pBase[(unsigned int)e] = b;
}


void EncodeAndWriteLiteralLoad(ExceptionVector v, ExceptionType e)
{
	unsigned int *pBase = GetTableAddress();

	//store the address 8*4 bytes on
	pBase[8 + (unsigned int)e] = (unsigned int)v;

	//write the load
	pBase[(unsigned int)e] = 0xe59ff000 + 8 * 4 - 8;
}

unsigned int *GetTableAddress(void)
{
	return (unsigned int *)0xffff0000;
}

void EncodeAndWriteBkpt(ExceptionType e)
{
	unsigned int *pBase = GetTableAddress();
	pBase[(unsigned int)e] = 0xe1200070;
}

}
