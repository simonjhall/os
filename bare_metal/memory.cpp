/*
 * memory.cpp
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

#include "print_uart.h"
#include "common.h"


void *g_brk;
void *GetHighBrk(void)
{
	return g_brk;
}

void SetHighBrk(void *p)
{
	g_brk = p;
}



//void EnableMmu(bool on)
//{
//	unsigned int current, onoff;
//
//	asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (current) : : "cc");
//	onoff = current & 1;
//
//	ASSERT(onoff == !on);
//
//    current = (current & ~1) | (unsigned int)on;
//    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (current) : "cc");
//}

