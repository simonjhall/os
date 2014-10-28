/*
 * memory.cpp
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <sys/uio.h>
//#include <asm-generic/errno-base.h>
//#include <fcntl.h>
//#include <unistd.h>

#include "print_uart.h"
#include "common.h"
#include "virt_memory.h"
#include "translation_table.h"

#include <string.h>



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

void *internal_mmap(void *addr, size_t length, int prot, int flags,
                  BaseDirent *f, off_t offset, bool isPriv)
{
	unsigned int length_rounded = (length + 4095) & ~4095;
	unsigned int length_pages = length_rounded >> 12;

	if ((isPriv && addr < (void *)0x80000000) || (!isPriv && addr >= (void *)0x80000000))
	{
		Printer &p = Printer::Get();
		p << "illegal mmap\n";
		p << addr << "\n";
		p << length << "\n";
		p << prot << "\n";
		p << flags << "\n";
		p << f << "\n";
//		p << offset << "\n";
		p << isPriv << "\n";
		ASSERT(0);
	}

	if (VirtMem::AllocAndMapVirtContig(addr, length_pages, isPriv,
			isPriv ? TranslationTable::kRwRo : TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
	{
		memset(addr, 0, length_pages << 12);
		return addr;
	}
	else
		return (void *)-1;
}

int internal_munmap(void *addr, size_t length)
{
	return 0;
}
