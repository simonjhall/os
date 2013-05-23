/*
 * virt_memory.cpp
 *
 *  Created on: 17 May 2013
 *      Author: simon
 */

#include "virt_memory.h"

#include <functional>


extern unsigned int master_tlb;

namespace VirtMem
{

//master kernel L1 tables
static TranslationTable::TableEntryL1 *g_pPageTableL1Virt;
static TranslationTable::TableEntryL1 *g_pPageTableL1Phys;

//allocators for tables, in the virtual address space
FixedSizeAllocator<TranslationTable::L1Table, 1048576 / sizeof(TranslationTable::L1Table)> g_masterTables;
FixedSizeAllocator<TranslationTable::L2Table, 1048576 / sizeof(TranslationTable::L2Table)> g_subTables;
bool g_allocatorsInited = false;

bool AllocL1Table(unsigned int entryPoint)
{
	g_pPageTableL1Virt = (TranslationTable::TableEntryL1 *)&master_tlb;
	g_pPageTableL1Phys = (TranslationTable::TableEntryL1 *)entryPoint;

	if (g_pPageTableL1Virt == (TranslationTable::TableEntryL1 *)-1)
		return false;
	else
		return true;
}

TranslationTable::TableEntryL1 *GetL1TableVirt(void)
{
	return g_pPageTableL1Virt;
}

bool InitL1L2Allocators(void)
{
	//find a MB for each
	void *pL1Phys = PhysPages::FindMultiplePages(256, 8);
	void *pL2Phys = PhysPages::FindMultiplePages(256, 8);

	if (pL1Phys == (void *)-1 || pL2Phys == (void *)-1)
		return false;

	//map them virtually
	if (!MapPhysToVirt(pL1Phys, (void *)0xfe000000, 1048576, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0))
		return false;

	if (!MapPhysToVirt(pL2Phys, (void *)(0xfe000000 + 1048576), 1048576, TranslationTable::kRwNa, TranslationTable::kNoExec, TranslationTable::kOuterInnerWbWa, 0))
		return false;

	g_masterTables.Init((TranslationTable::L1Table *)0xfe000000);
	g_subTables.Init((TranslationTable::L2Table *)(0xfe000000 + 1048576));

	g_allocatorsInited = true;

	return true;
}

void FlushTlb(void)
{
	asm volatile ("mrc p15, 0, r0, c3, c0, 0\n");
	asm volatile ("mcr p15, 0, r0, c3, c0, 0\n");
}

bool MapPhysToVirt(void *pPhys, void *pVirt, unsigned int length,
		TranslationTable::AccessPerm perm, TranslationTable::ExecuteNever xn, TranslationTable::MemRegionType type, unsigned int domain)
{
	auto mapper = [pPhys, pVirt, length, perm, xn, type, domain](bool commit)
		{
			unsigned int virtStart = (unsigned int)pVirt;
			unsigned int physStart = (unsigned int)pPhys;
			unsigned int len = length;

			/*PrinterUart p;
			p.PrintString("starting virt ");
			p.Print(virtStart);
			p.PrintString(" phys ");
			p.Print(physStart);
			p.PrintString(" len ");
			p.Print(len);
			p.PrintString("\n");*/


			ASSERT((len & 0xfff) == 0);					//page multiple
			ASSERT((physStart & 0xfff) == 0);				//page aligned
			ASSERT((virtStart & 0xfff) == 0);				//page aligned

			while (len)
			{
				unsigned int to_map = len > 1048576 ? 1048576 : len;
				unsigned int virtEnd = virtStart + to_map;

				/*p.PrintString("   it virtStart ");
				p.Print(virtStart);
				p.PrintString(" virtEnd ");
				p.Print(virtEnd);
				p.PrintString(" physStart ");
				p.Print(physStart);
				p.PrintString(" len ");
				p.Print(len);
				p.PrintString(" to_map ");
				p.Print(to_map);
				p.PrintString("\n");*/

				//clamp to the end of the megabyte
				if ((virtEnd >> 20) != (virtStart >> 20))
				{
					ASSERT((virtEnd >> 20) - (virtStart >> 20) == 1);

					virtEnd = virtEnd & ~1048575;
					to_map = virtEnd - virtStart;
				}

				//get the l1 entry
				unsigned int megabyte = (unsigned int)virtStart >> 20;
				TranslationTable::TableEntryL1 *pMainTableVirt = GetL1TableVirt();

				//find out what's there
				bool current_fault, current_pagetable, current_section;

				current_fault = pMainTableVirt[megabyte].IsFault();
				current_pagetable = pMainTableVirt[megabyte].IsPageTable();
				current_section = pMainTableVirt[megabyte].IsSection();

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

						pMainTableVirt[megabyte].section.Init((void *)physStart, perm, xn, type, domain);
					}
				}
				else		//map part of it
				{
					//currently a fault, so replace any unused range with fault
					if (current_fault)
					{
						if (commit)
						{
							TranslationTable::TableEntryL2 *pNewVirt;
							bool ok = AddPageTable((void *)virtStart, domain, &pNewVirt);

							if (!ok)
								return false;

							//clear the lot
							for (unsigned int o = 0; o < 256; o++)
								pNewVirt[o].fault.Init();

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pNewVirt[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
							}
						}
					}
					//currently a section
					if (current_section)
					{
						if (pMainTableVirt[megabyte].section.m_domain != domain)
							return false;

						if (commit)
						{
							//take a copy
							TranslationTable::Section existing = pMainTableVirt[megabyte].section;

							//extract the goodies
							unsigned int existing_pa = existing.m_sectionBase << 20;
							TranslationTable::MemRegionType existing_type = (TranslationTable::MemRegionType)((existing.m_b) | (existing.m_c << 1) | (existing.m_tex << 2));
							TranslationTable::AccessPerm existing_perm = (TranslationTable::AccessPerm)(existing.m_ap | (existing.m_ap2 << 2));

							TranslationTable::TableEntryL2 *pNewVirt;
							bool ok = AddPageTable((void *)virtStart, domain, &pNewVirt);

							if (!ok)
								return false;

							//re-use the initial settings
							for (unsigned int o = 0; o < 256; o++)
								pNewVirt[o].smallPage.Init((void *)(existing_pa + o * 4096),
										existing_perm, (TranslationTable::ExecuteNever)existing.m_xn, existing_type);

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pNewVirt[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
							}
						}
					}
					//currently a page table
					if (current_pagetable)
					{
						if (pMainTableVirt[megabyte].pageTable.m_domain != domain)
							return false;

						if (commit)
						{
							//get the existing page table phys address
							TranslationTable::TableEntryL2 *pTablePhys = (TranslationTable::TableEntryL2 *)(pMainTableVirt[megabyte].pageTable.m_pageTableBase << 10);

							//find a virtual mapping
							TranslationTable::TableEntryL2 *pTableVirt;
							if (!PhysToVirt(pTablePhys, &pTableVirt, g_subTables.GetVirtBaseInMb(), g_subTables.GetLengthMb()))
								return false;

							//map the bit we're interested in
							unsigned int total_pages = to_map >> 12;
							unsigned int starting_page = (virtStart >> 12) & 255;

							for (unsigned int o = 0; o < total_pages; o++)
							{
								ASSERT(o + starting_page < 256);
								pTableVirt[o + starting_page].smallPage.Init((void *)(physStart + o * 4096), perm, xn, type);
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

		FlushTlb();
		return ok;
	}
	else
		return false;
}

bool UnmapVirt(void *pVirt, unsigned int length)
{
	return false;
}

bool AddPageTable(void *pVirtual, unsigned int domain, TranslationTable::TableEntryL2 **ppNewTableVirt)
{
	if (!g_allocatorsInited)
		return false;

	//get a new mapped page table
	TranslationTable::L2Table *pNewVirt, *pNewPhys;
	if (!g_subTables.Allocate(&pNewVirt))
		return false;

	//get its physical address
	if (!VirtToPhys(pNewVirt, &pNewPhys))
	{
		ASSERT(0);
		if (!g_subTables.Free(pNewVirt))
			ASSERT(0);
		return false;
	}

	//clear the table
	for (int count = 0; count < 256; count++)
		pNewVirt->e[count].fault.Init();

	//add the new entry into the table
	unsigned int megabyte = (unsigned int)pVirtual >> 20;
	TranslationTable::TableEntryL1 *pMainTable = GetL1TableVirt();
	pMainTable[megabyte].pageTable.Init((TranslationTable::TableEntryL2 *)pNewPhys, domain);

	*ppNewTableVirt = (TranslationTable::TableEntryL2 *)pNewVirt;

	return true;
}

bool RemovePageTable(void *pVirtual)
{
	ASSERT(g_allocatorsInited);

	//find the entry
	unsigned int megabyte = (unsigned int)pVirtual >> 20;
	TranslationTable::TableEntryL1 *pMainTable = GetL1TableVirt();

	//check there is something there
	if (!pMainTable[megabyte].IsPageTable())
		return false;

	//get the phys addr of the table
	unsigned int phys = pMainTable[megabyte].pageTable.m_pageTableBase << 10;

	//replace the entry with a fault
	pMainTable[megabyte].fault.Init();

	//get the table's virtual address
	TranslationTable::L2Table *pOldVirt;
	if (!PhysToVirt((TranslationTable::L2Table *)phys, &pOldVirt, g_subTables.GetVirtBaseInMb(), g_subTables.GetLengthMb()))
	{
		ASSERT(0);
		return false;
	}

	//release the memory
	if (!g_subTables.Free(pOldVirt))
		ASSERT(0);
	return true;
}

void DumpVirtToPhys(void *pStart, void *pEnd, bool withL2, bool noFault)
{
	TranslationTable::TableEntryL1 *pL1Virt = VirtMem::GetL1TableVirt();

	for (unsigned int count = (unsigned int)pStart >> 20; count < (unsigned int)pEnd >> 20; count++)
	{
		if (noFault && pL1Virt[count].IsFault())
			continue;

		PrinterUart<PL011> p;

		p.Print(count * 1048576);
		p.PrintString(": ");

		if (pL1Virt[count].Print(p) && withL2)
		{
			p.PrintString("\n");
			TranslationTable::TableEntryL2 *pL2Phys = pL1Virt[count].pageTable.GetPhysPageTable();
			TranslationTable::TableEntryL2 *pL2Virt;

			if (!VirtMem::PhysToVirt(pL2Phys, &pL2Virt))
				p.PrintString("\tNO PHYS->VIRT MAPPING FOR PAGE TABLE\n");
			else
			{
				for (unsigned int inner = 0; inner < 256; inner++)
				{
					if (noFault && pL2Virt[inner].IsFault())
						continue;

					p.PrintString("\t");
					p.Print(count * 1048576 + inner * 4096);
					p.PrintString(": ");
					pL2Virt[inner].Print(p);
				}

				p.PrintString("\n");
			}
		}
	}
}

}


