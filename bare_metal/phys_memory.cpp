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
struct Reservation
{
	unsigned int m_usageCount;
};

Reservation g_usedPage [s_totalPages];

//find one free physical page
void *FindPage(void)
{
	for (unsigned int count = 0; count < s_totalPages; count++)
		if (g_usedPage[count].m_usageCount == 0)
		{
			g_usedPage[count].m_usageCount++;
			return (void *)((count + s_startPage) * 4096);
		}
	return (void *)-1;
}

//find X free physical pages, (1<<Y) * 4096 byte aligned
void *FindMultiplePages(unsigned int num, unsigned int alignOrder)
{
	unsigned int pageStep = 1 << alignOrder;

	for (unsigned int count = 0; count < s_totalPages; count += pageStep)
		if (g_usedPage[count].m_usageCount == 0)
		{
			unsigned int found = 0;
			for (unsigned int inner = 0; inner < num; inner++)
			{
				if (g_usedPage[count + inner].m_usageCount == 0)
					found++;
				else
					break;
			}

			if (found == num)
				for (unsigned int inner = 0; inner < num; inner++)
				{
					if (g_usedPage[count + inner].m_usageCount == 0)
						g_usedPage[count + inner].m_usageCount++;
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
	ASSERT(g_usedPage[page].m_usageCount);

	g_usedPage[page].m_usageCount--;
}

void ReleaseMultiplePages(unsigned int p, unsigned int num)
{
	p -= s_startPage;
	for (unsigned int page = p >> 12; page < (p >> 12) + num; page++)
	{
		ASSERT(p < s_totalPages);
		ASSERT(g_usedPage[page].m_usageCount);

		g_usedPage[page].m_usageCount--;
	}
}

void BlankUsedPages(void)
{
	for (unsigned int count = 0; count < s_totalPages; count++)
    	g_usedPage[count].m_usageCount = 0;
}

void ReservePages(unsigned int start, unsigned int num)
{
	//in case we've got the wrong memory model
	ASSERT(start - s_startAddr <= start);
	start -= s_startPage;

	for (unsigned int count = start; count < start + num; count++)
	{
		ASSERT(count < s_totalPages);

		//to check for overflow
		ASSERT(g_usedPage[count].m_usageCount + 1 > g_usedPage[count].m_usageCount);
		g_usedPage[count].m_usageCount++;
	}
}

unsigned int PageUsedCount(void)
{
	unsigned int total = 0;
	for (unsigned int count = 0; count < s_totalPages; count++)
		if (g_usedPage[count].m_usageCount)
			total++;

	return total;
}

}



