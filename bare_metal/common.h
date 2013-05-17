/*
 * common.h
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

#ifndef COMMON_H_
#define COMMON_H_

#include "print_uart.h"

static inline void assert_func(void)
{
	volatile bool wait = false;
	while (wait == false);
}

#define ASSERT(x) { if (!(x)) {PrinterUart<PL011> p;p.PrintString("assert ");p.PrintString(__FILE__);p.PrintString(" ");p.Print(__LINE__);assert_func();} }

#include "phys_memory.h"
#include "virt_memory.h"
#include "translation_table.h"


void *GetHighBrk(void);
void SetHighBrk(void *);

#endif /* COMMON_H_ */
