/*
 * virt_memory.h
 *
 *  Created on: 17 May 2013
 *      Author: simon
 */

#ifndef VIRT_MEMORY_H_
#define VIRT_MEMORY_H_

#include "common.h"
#include "translation_table.h"
#include "fixed_size_allocator.h"

namespace VirtMem
{

//insertion and deletion of page tables
bool AddPageTable(void *pVirtual, unsigned int domain, TranslationTable::TableEntryL2 **ppNewTableVirt, bool hi);
bool RemovePageTable(void *pVirtual, bool hi);

//mapping and unmapping memory
bool MapPhysToVirt(void *pPhys, void *pVirt, unsigned int length, bool hi,
		TranslationTable::AccessPerm perm, TranslationTable::ExecuteNever xn, TranslationTable::MemRegionType type, unsigned int domain);
bool UnmapVirt(void *pVirt, unsigned int length);

bool AllocAndMapVirtContig(void *pBase, unsigned int numPages, bool hi,
		TranslationTable::AccessPerm perm, TranslationTable::ExecuteNever xn, TranslationTable::MemRegionType type, unsigned int domain);

//master kernel L1 table
bool AllocL1Table(unsigned int entryPoint);
TranslationTable::TableEntryL1 *GetL1TableVirt(bool hi);
bool SetL1TableLo(TranslationTable::TableEntryL1 *pVirt);

//the L1 and L2 table allocators
bool InitL1L2Allocators(void);

//flushing TLB
extern "C" void FlushTlb(void);

//changing ttbr0
extern "C" void InsertPageTableLoAndFlush(TranslationTable::TableEntryL1 *pPhys);

//debug
void DumpVirtToPhys(void *pStart, void *pEnd, bool withL2, bool noFault, bool hi);

template <class T>
bool PhysToVirt(T *pPhysical, T **ppVirtual, unsigned int startMb = 0, unsigned int numMb = 4096, int depth = 0);

//translation of virtual to physical
template <class T>
bool VirtToPhys(T *pVirtual, T **ppPhysical)
{
	bool hi = pVirtual >= (T *)0x80000000;

	unsigned int megabyte = (unsigned int)pVirtual >> 20;
	TranslationTable::TableEntryL1 *pMainTableVirt = GetL1TableVirt(hi);

	if (pMainTableVirt[megabyte].IsFault())
		return false;
	else if (pMainTableVirt[megabyte].IsSection())
	{
		//offset within the megabyte
		unsigned int offset = (unsigned int)pVirtual & 1048575;
		*ppPhysical = (T *)((pMainTableVirt[megabyte].section.m_sectionBase << 20) + offset);
		return true;
	}
	else if (pMainTableVirt[megabyte].IsPageTable())
	{
		TranslationTable::TableEntryL2 *pPtePhys = pMainTableVirt[megabyte].pageTable.GetPhysPageTable();
		TranslationTable::TableEntryL2 *pPteVirt;

//		extern FixedSizeAllocator<TranslationTable::L1Table, 1048576 / sizeof(TranslationTable::L1Table)> g_masterTables;
		extern FixedSizeAllocator<TranslationTable::L2Table, 1048576 / sizeof(TranslationTable::L2Table)> g_subTables;

		if (PhysToVirt(pPtePhys, &pPteVirt, g_subTables.GetVirtBaseInMb(), g_subTables.GetLengthMb()))
		{
			//page within the megabyte
			unsigned int page = ((unsigned int)pVirtual >> 12) & 255;

			if (pPteVirt[page].IsFault())
				return false;
			else if (pPteVirt[page].IsSmallPage())
			{
				//offset within the page
				unsigned int offset = (unsigned int)pVirtual & 4095;
				*ppPhysical = (T *)((pPteVirt[page].smallPage.m_pageBase << 12) + offset);
				return true;
			}
			else
			{
				ASSERT(0);
				return false;
			}
		}
		else
			return false;
	}
	else
	{
		ASSERT(0);
		return false;
	}
}

//find a virtual mapping for a physical address
template <class T>
bool PhysToVirt(T *pPhysical, T **ppVirtual, unsigned int startMb = 0, unsigned int numMb = 4096, int depth = 0)
{
	if (depth == 10)
		return false;

//	unsigned int phys_whole = (unsigned int)pPhysical;

	unsigned int phys_mb = (unsigned int)pPhysical & ~1048575;
	unsigned int phys_sub_mb = (unsigned int)pPhysical & 1048575;

	unsigned int phys_4k = (unsigned int)pPhysical & ~4095;
	unsigned int phys_sub_4k = (unsigned int)pPhysical & 4095;

	for (unsigned int outer = startMb; outer < startMb + numMb; outer++)
	{
		TranslationTable::TableEntryL1 *pMainTableVirt = GetL1TableVirt(outer >= 2048);

		if (pMainTableVirt[outer].IsSection())
		{
			if ((unsigned int)(pMainTableVirt[outer].section.m_sectionBase << 20) == phys_mb)
			{
				*ppVirtual = (T *)((outer << 20) + phys_sub_mb);
				return true;
			}
		}
		else if (pMainTableVirt[outer].IsPageTable())
		{
			//get the virtual address of the page table
			TranslationTable::TableEntryL2 *pTableVirt;
			TranslationTable::TableEntryL2 *pTablePhys = (TranslationTable::TableEntryL2 *)(pMainTableVirt[outer].pageTable.m_pageTableBase << 10);

			//this would be bad if it failed
			//but not catastrophic
			if (!PhysToVirt(pTablePhys, &pTableVirt, startMb, numMb, depth + 1))
				continue;

			for (unsigned int inner = 0; inner < 256; inner++)
			{
				if ((unsigned int)(pTableVirt[inner].smallPage.m_pageBase << 12) == phys_4k)
				{
					*ppVirtual = (T *)((pTableVirt[inner].smallPage.m_pageBase << 12) + phys_sub_4k);
					return true;
				}
			}
		}
		//not interested in fault
	}

	return false;
}


template <class T>
void DumpMem(T *pVirtual, unsigned int len)
{
	PrinterUart<PL011> p;
	for (unsigned int count = 0; count < len; count++)
	{
		p.PrintHex((unsigned int)pVirtual);
		p.PrintString(": ");
		p.PrintHex(*pVirtual);
		p.PrintString("\n");
		pVirtual++;
	}
}

}



#endif /* VIRT_MEMORY_H_ */
