/*
 * common.h
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

#ifndef COMMON_H_
#define COMMON_H_

#include "print_uart.h"

#include <sys/types.h>

class BaseDirent;

static inline void assert_func(void)
{
	volatile bool wait = false;
	while (wait == false);
}

#define ASSERT(x) { if (!(x)) {PrinterUart<PL011> p;p.PrintString("assert ");p.PrintString(__FILE__);p.PrintString(" ");p.PrintDec(__LINE__, true);assert_func();} }

//caches
extern "C" void v7_invalidate_l1(void);
extern "C" void v7_flush_icache_all(void);
extern "C" void v7_flush_dcache_all(void);

bool InitMempool(void *pBase, unsigned int numPages);

void *internal_mmap(void *addr, size_t length, int prot, int flags,
                  BaseDirent *f, off_t offset, bool isPriv);
int internal_munmap(void *addr, size_t length);

void *GetHighBrk(void);
void SetHighBrk(void *);

inline void DelaySecond(void)
{
	for (int count = 0; count < 3333333; count++);
}

inline void DelayMillisecond(void)
{
	for (int count = 0; count < 3333; count++);
}

#endif /* COMMON_H_ */
