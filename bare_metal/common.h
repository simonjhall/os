/*
 * common.h
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

#ifndef COMMON_H_
#define COMMON_H_

//#include "print_uart.h"
#include "print.h"
//#include "UART.h"
#include "malloc.h"

#include <sys/types.h>

class BaseDirent;

extern "C" void EnableIrq(bool);
extern "C" void EnableFiq(bool);
extern "C" bool IsIrqEnabled(void);
extern "C" void InvokeSyscall(unsigned int r7, unsigned int r0 = 0, unsigned int r1 = 0, unsigned int r2 = 0);

extern "C" void assert_func(void);

#ifdef __ARM_ARCH_7A__
#define ASSERT(x) \
	{\
		if (!(x))\
		{\
			Printer &p = Printer::Get();\
			p.PrintString("A9 assert ");\
			p.PrintString(__FILE__);\
			p.PrintString(" ");\
			p.PrintDec(__LINE__, true);\
			assert_func();\
		}\
	}
#endif

#ifdef __ARM_ARCH_7M__
//#define ASSERT(x) { if (!(x)) {OMAP4460::UART p((volatile unsigned int *)0x48020000);p.PrintString("M3 assert ");p.PrintString(__FILE__);p.PrintString(" ");p.PrintDec(__LINE__, true);assert_func();} }
#define ASSERT(x) \
	{\
		if (!(x))\
		{\
			Printer &p = Printer::Get();\
			p.PrintString("A7 assert ");\
			p.PrintString(__FILE__);\
			p.PrintString(" ");\
			p.PrintDec(__LINE__, true);\
			assert_func();\
		}\
	}
#endif

//#define ASSERT(x)

//caches
extern "C" void v7_invalidate_l1(void);
extern "C" void v7_flush_icache_all(void);
extern "C" void v7_flush_dcache_all(void);

bool InitMempool(void *pBase, unsigned int numPages, bool phys, mspace *pPool = 0);

void *internal_mmap(void *addr, size_t length, int prot, int flags,
                  BaseDirent *f, off_t offset, bool isPriv);
int internal_munmap(void *addr, size_t length);

void *GetHighBrk(void);
void SetHighBrk(void *);

extern "C" void DelaySecond(void);
extern "C" void DelayMillisecond(void);
/*inline void DelaySecond(void)
{
	for (volatile int count = 0; count < 3333333; count++);
}

inline void DelayMillisecond(void)
{
	for (volatile int count = 0; count < 3333; count++);
}*/

#endif /* COMMON_H_ */
