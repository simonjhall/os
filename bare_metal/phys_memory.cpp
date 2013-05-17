/*
 * phys_memory.cpp
 *
 *  Created on: 17 May 2013
 *      Author: simon
 */

#include "phys_memory.h"

namespace PhysPages
{

//all the free 4k phys pages in the system
bool g_usedPage [s_totalPages];

//find one free physical page
void *FindPage(void)
{
	for (unsigned int count = 0; count < s_totalPages; count++)
		if (g_usedPage[count] == false)
		{
			g_usedPage[count] = true;
			return (void *)((count + s_startPage) * 4096);
		}
	return (void *)-1;
}

//find X free physical pages, (1<<Y) * 4096 byte aligned
void *FindMultiplePages(unsigned int num, unsigned int alignOrder)
{
	unsigned int pageStep = 1 << alignOrder;

	for (unsigned int count = 0; count < s_totalPages; count += pageStep)
		if (g_usedPage[count] == false)
		{
			unsigned int found = 0;
			for (unsigned int inner = 0; inner < num; inner++)
			{
				if (g_usedPage[count + inner] == false)
					found++;
				else
					break;
			}

			if (found == num)
				for (unsigned int inner = 0; inner < num; inner++)
				{
					if (g_usedPage[count + inner] == false)
						g_usedPage[count + inner] = true;
				}
			else
				continue;

			return (void *)((count + s_startPage) * 4096);
		}
	return (void *)-1;
}

void ReleasePage(unsigned int p)
{
	p -= s_startPage;
	unsigned int page = p >> 12;

	ASSERT(p < s_totalPages);
	ASSERT(g_usedPage[page] == true);
	g_usedPage[page] = false;
}

void ReleaseMultiplePages(unsigned int p, unsigned int num)
{
	p -= s_startPage;
	for (unsigned int page = p >> 12; page < (p >> 12) + num; page++)
	{
		ASSERT(p < s_totalPages);
		ASSERT(g_usedPage[page] == true);
		g_usedPage[page] = false;
	}
}

void BlankUsedPages(void)
{
	for (unsigned int count = 0; count < s_totalPages; count++)
    	g_usedPage[count] = false;
}

void ReservePages(unsigned int start, unsigned int num)
{
	start -= s_startPage;
	for (unsigned int count = start; count < start + num; count++)
	{
		ASSERT(count < s_totalPages);
		ASSERT(g_usedPage[count] == false);
		g_usedPage[count] = true;
	}
}

}



