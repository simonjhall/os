/*
 * phys_memory.h
 *
 *  Created on: 17 May 2013
 *      Author: simon
 */

#ifndef PHYS_MEMORY_H_
#define PHYS_MEMORY_H_

#include "common.h"

namespace PhysPages
{
#ifdef PBES
	static const unsigned int s_startAddr = 0x80000000;
	static const unsigned int s_startPage = s_startAddr >> 12;
#else
	static const unsigned int s_startAddr = 0x00000000;
	static const unsigned int s_startPage = s_startAddr >> 12;
#endif

	static const unsigned int s_totalPages = 128 * 256;

	void BlankUsedPages(void);
	void ReservePages(unsigned int start, unsigned int num);

	void *FindPage(void);
	void ReleasePage(unsigned int p);

	void *FindMultiplePages(unsigned int num, unsigned int alignOrder);
	void ReleaseMultiplePages(unsigned int p, unsigned int num);
}


#endif /* PHYS_MEMORY_H_ */
