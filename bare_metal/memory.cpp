/*
 * memory.cpp
 *
 *  Created on: 11 May 2013
 *      Author: simon
 */

#include "print_uart.h"
#include "common.h"

#include <functional>

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

static TranslationTable::TableEntryL1 *g_pPageTableL1;
bool AllocL1Table(void)
{
	g_pPageTableL1 = (TranslationTable::TableEntryL1 *)FindMultiplePages(4, 2);		//4096 entries of 4b each

	if (g_pPageTableL1 == (TranslationTable::TableEntryL1 *)-1)
		return false;
	else
		return true;
}

TranslationTable::TableEntryL1 *GetL1Table(void)
{
	return g_pPageTableL1;
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


void *g_brk;
void *GetHighBrk(void)
{
	return g_brk;
}

void SetHighBrk(void *p)
{
	g_brk = p;
}

bool MapPhysToVirt(void *pPhys, void *pVirt, unsigned int length,
		TranslationTable::AccessPerm perm, TranslationTable::ExecuteNever xn, TranslationTable::MemRegionType type, unsigned int domain)
{
	auto mapper = [pPhys, pVirt, length, perm, xn, type, domain](bool commit)
		{
			unsigned int virtStart = (unsigned int)pVirt;
			unsigned int physStart = (unsigned int)pPhys;
			unsigned int len = length;

			PrinterUart p;
			p.PrintString("starting ");
			p.Print(virtStart);
			p.PrintString(" ");
			p.Print(physStart);
			p.PrintString(" ");
			p.Print(len);
			p.PrintString("\r\n");


			ASSERT((len & 0xfff) == 0);					//page multiple
			ASSERT((physStart & 0xfff) == 0);				//page aligned
			ASSERT((virtStart & 0xfff) == 0);				//page aligned

			p.Print(__LINE__);
			p.PrintString("\r\n");

			while (len)
			{
				unsigned int to_map = len > 1048576 ? 1048576 : len;
				unsigned int virtEnd = virtStart + to_map;

				p.PrintString("   it ");
				p.Print(virtStart);
				p.PrintString(" ");
				p.Print(virtEnd);
				p.PrintString(" ");
				p.Print(physStart);
				p.PrintString(" ");
				p.Print(len);
				p.PrintString(" ");
				p.Print(to_map);
				p.PrintString("\r\n");

				//clamp to the end of the megabyte
				if ((virtEnd >> 20) != (virtStart >> 20))
				{
					ASSERT((virtEnd >> 20) - (virtStart >> 20) == 1);

					virtEnd = virtEnd & ~1048575;
					to_map = virtEnd - virtStart;
				}

				//get the l1 entry
				unsigned int megabyte = (unsigned int)virtStart >> 20;
				TranslationTable::TableEntryL1 *pMainTable = PhysPages::GetL1Table();

				//find out what's there
				bool current_fault, current_pagetable, current_section;

				current_fault = pMainTable[megabyte].IsFault();
				current_pagetable = pMainTable[megabyte].IsPageTable();
				current_section = pMainTable[megabyte].IsSection();

				//the l1 page table should be initialised
				ASSERT(current_fault || current_pagetable || current_section);
				ASSERT((int)current_fault + (int)current_pagetable + (int)current_section == 1);

				//map the whole thing
				if (len == 1048576)
				{
					//map the section
					if (commit)
					{
						//release a page table if mapped
						//fault and section do not matter
						if (current_pagetable)
							RemovePageTable((void *)virtStart);

						pMainTable[megabyte].section.Init((void *)physStart, perm, xn, type, domain);
					}
				}
				else		//map part of it
				{
					//currently a fault, so replace any unused range with fault
					if (current_fault)
					{
						if (commit)
						{
							TranslationTable::TableEntryL2 *pNew;
							bool ok = AddPageTable((void *)virtStart, domain, &pNew);

							if (!ok)
								return false;

							//clear the lot
							for (unsigned int o = 0; o < 256; o++)
								pNew[o].fault.Init();

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pNew[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
							}
						}
					}
					//currently a section
					if (current_section)
					{
						if (pMainTable[megabyte].section.m_domain != domain)
							return false;

						if (commit)
						{
							//take a copy
							TranslationTable::Section existing = pMainTable[megabyte].section;
							unsigned int existing_pa = existing.m_sectionBase << 20;
							TranslationTable::MemRegionType existing_type = (TranslationTable::MemRegionType)((existing.m_b) | (existing.m_c << 1) | (existing.m_tex << 2));
							TranslationTable::AccessPerm existing_perm = (TranslationTable::AccessPerm)(existing.m_ap | (existing.m_ap2 << 2));

							TranslationTable::TableEntryL2 *pNew;
							bool ok = AddPageTable((void *)virtStart, domain, &pNew);

							if (!ok)
								return false;

							//re-use the initial settings
							for (unsigned int o = 0; o < 256; o++)
								pNew[o].smallPage.Init((void *)(existing_pa + o * 4096),
										existing_perm, (TranslationTable::ExecuteNever)existing.m_xn, existing_type);

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pNew[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
							}
						}
					}
					//currently a page table
					if (current_pagetable)
					{
						if (pMainTable[megabyte].pageTable.m_domain != domain)
							return false;

						if (commit)
						{
							//get the existing page table phys address
							TranslationTable::TableEntryL2 *pTable = (TranslationTable::TableEntryL2 *)(pMainTable[megabyte].pageTable.m_pageTableBase << 10);

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pTable[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
							}
						}
					}
				}

				ASSERT(len - to_map < len);	//no negative

				len -= to_map;
				physStart += to_map;
				virtStart += to_map;
			}

			return true;
		};

	if (mapper(false))
	{
		bool ok = mapper(true);
		ASSERT(ok);

		return ok;
	}
	else
		return false;
}

bool UnmapVirt(void *pVirt, unsigned int length)
{
	return false;
}

bool AddPageTable(void *pVirtual, unsigned int domain, TranslationTable::TableEntryL2 **ppNewTable)
{
	//find 1KB of free phys space
	void *pPage = PhysPages::FindPage();

	if (pPage == (void *)-1)
		return false;

	//clear the table
	TranslationTable::TableEntryL2 *pTable = (TranslationTable::TableEntryL2 *)pPage;
	for (int count = 0; count < 256; count++)
		pTable[count].fault.Init();

	//add the new entry into the table
	unsigned int megabyte = (unsigned int)pVirtual >> 20;
	TranslationTable::TableEntryL1 *pMainTable = PhysPages::GetL1Table();
	pMainTable[megabyte].pageTable.Init(pTable, domain);

	*ppNewTable = pTable;

	return true;
}

bool RemovePageTable(void *pVirtual)
{
	//find the entry
	unsigned int megabyte = (unsigned int)pVirtual >> 20;
	TranslationTable::TableEntryL1 *pMainTable = PhysPages::GetL1Table();

	//check there is something there
	bool current = pMainTable[megabyte].IsPageTable();
	if (!current)
		return false;

	//get the phys addr of the table
	unsigned int phys = pMainTable[megabyte].pageTable.m_pageTableBase << 10;

	//replace the entry with a fault
	pMainTable[megabyte].fault.Init();

	//release the memory
	PhysPages::ReleasePage(phys >> 12);
	return true;
}

void EnableMmu(bool on)
{
	unsigned int current, onoff;

	asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (current) : : "cc");
	onoff = current & 1;

	ASSERT(onoff == !on);

    current = (current & ~1) | (unsigned int)on;
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (current) : "cc");
}

