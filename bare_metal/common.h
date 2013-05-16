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

#define ASSERT(x) { if (!(x)) {PrinterUart p;p.PrintString("assert ");p.PrintString(__FILE__);p.PrintString(" ");p.Print(__LINE__);assert_func();} }

namespace TranslationTable
{
	//properties
	enum MemRegionType
	{
		kStronglyOrdered = 0,
		kShareableDevice = 1,
		kOuterInnerWtNoWa = 2,
		kOuterInnerWbNoWa = 3,

		kOuterInnerNc = 4,
		kOuterInnerWbWa = 7,

		kNonShareableDevice = 8,
	};

	enum ExecuteNever
	{
		kExec = 0,
		kNoExec = 1,
	};

	enum AccessPerm
	{
		kNaNa = 0,
		kRwNa = 1,
		kRwRo = 2,
		kRwRw = 3,
		kRoNa = 5,
		kRoRo = 7,
	};

	struct Fault;
	struct PageTable;
	struct Section;
	struct SmallPage;

	struct TableEntryL1;
	struct TableEntryL2;


	//first level (fault is also good for second level)

	struct Fault
	{
		inline void Init(void)
		{
			m_ignore = 0;
			m_zero = 0;
		}
		inline void Init(unsigned int top)
		{
			ASSERT(top < 0x3fffffff);

			m_zero = 0;
			m_ignore = top;
		}

		void Print(Printer &p)
		{
			p.PrintString("FAULT\r\n");
		}

		unsigned int m_zero:2;
		unsigned int m_ignore:30;
	};

	struct PageTable
	{
		inline void Init(TableEntryL2 *pBase, unsigned int domain)
		{
			m_zeroOne = 1;
			m_pxn = 0;
			m_ns = 0;
			m_sbz = 0;

			ASSERT(domain < 15);
			m_domain = domain;

			ASSERT(((unsigned int)pBase & 1023) == 0);
			m_pageTableBase = (unsigned int)pBase >> 10;
		}

		void Print(Printer &p)
		{
			p.PrintString("PTE     phys ");
			p.Print(m_pageTableBase << 10);
			p.PrintString("\r\n");
		}

		TableEntryL2 *GetPhysPageTable(void)
		{
			return (TableEntryL2 *)(m_pageTableBase << 10);
		}

		unsigned int m_zeroOne:2;
		unsigned int m_pxn:1;
		unsigned int m_ns:1;
		unsigned int m_sbz:1;
		unsigned int m_domain:4;
		unsigned int m_ignore1:1;
		unsigned int m_pageTableBase:22;
	};

	struct Section
	{
		inline void Init(void *pBase, AccessPerm perm, ExecuteNever xn, MemRegionType type, unsigned int domain)
		{
			m_pxn = 0;
			m_one = 1;
			m_b = (unsigned int)type & 1;
			m_c = ((unsigned int)type >> 1) & 1;
			m_xn = (unsigned int)xn;
			m_domain = domain;
			m_ap = (unsigned int)perm & 3;
			m_tex = (unsigned int)type >> 2;
			m_ap2 = (unsigned int)perm >> 2;
			m_s = 0;
			m_ng = 0;
			m_zero = 0;
			m_ns = 0;

			ASSERT(((unsigned int)pBase & 1048575) == 0);
			m_sectionBase = (unsigned int)pBase >> 20;
		}

		void Print(Printer &p)
		{
			p.PrintString("Section phys ");
			p.Print(m_sectionBase << 20);
			p.PrintString(" AP ");
			p.Print(m_ap | (m_ap2 << 2));
			p.PrintString(" TEX ");
			p.Print(m_b | (m_c << 1) | (m_tex << 2));
			p.PrintString(" XN ");
			p.Print(m_xn);
			p.PrintString("\r\n");
		}

		unsigned int m_pxn:1;
		unsigned int m_one:1;
		unsigned int m_b:1;
		unsigned int m_c:1;
		unsigned int m_xn:1;
		unsigned int m_domain:4;
		unsigned int m_ignore1:1;
		unsigned int m_ap:2;
		unsigned int m_tex:3;
		unsigned int m_ap2:1;
		unsigned int m_s:1;
		unsigned int m_ng:1;
		unsigned int m_zero:1;
		unsigned int m_ns:1;
		unsigned int m_sectionBase:12;
	};

	struct TableEntryL1
	{
		union
		{
			Fault fault;
			PageTable pageTable;
			Section section;
		};

		bool IsFault(void)
		{
			if (fault.m_zero == 0)
				return true;
			else
				return false;
		}

		bool IsSection(void)
		{
			if (section.m_zero == 0 && section.m_one == 1)
				return true;
			else
				return false;
		}

		bool IsPageTable(void)
		{
			if (pageTable.m_zeroOne == 1)
				return true;
			else
				return false;
		}

		bool Print(Printer &p)
		{
			if (IsPageTable())
			{
				pageTable.Print(p);
				return true;
			}
			else if (IsSection())
			{
				section.Print(p);
				return false;
			}
			else if (IsFault())
			{
				fault.Print(p);
				return false;
			}
			else
			{
				ASSERT(0);
				return false;
			}
		}
	};

	//second level
	struct SmallPage
	{
		inline void Init(void *pBase, AccessPerm perm, ExecuteNever xn, MemRegionType type)
		{
		  m_xn = (unsigned int)xn;
		  m_one = 1;
		  m_b = (unsigned int)type & 1;
		  m_c = ((unsigned int)type > 1) & 1;
		  m_ap = (unsigned int)perm & 3;
		  m_tex = (unsigned int)type >> 2;
		  m_ap2 = (unsigned int)perm >> 2;
		  m_s = 0;
		  m_ng = 0;

		  ASSERT(((unsigned int)pBase & 4095) == 0);
		  m_pageBase = (unsigned int)pBase >> 12;
		};

		void Print(Printer &p)
		{
			p.PrintString("4k page phys ");
			p.Print(m_pageBase << 12);
			p.PrintString(" AP ");
			p.Print(m_ap | (m_ap2 << 2));
			p.PrintString(" TEX ");
			p.Print(m_b | (m_c << 1) | (m_tex << 2));
			p.PrintString(" XN ");
			p.Print(m_xn);
			p.PrintString("\r\n");
		}

		unsigned int m_xn:1;
		unsigned int m_one:1;
		unsigned int m_b:1;
		unsigned int m_c:1;
		unsigned int m_ap:2;
		unsigned int m_tex:3;
		unsigned int m_ap2:1;
		unsigned int m_s:1;
		unsigned int m_ng:1;
		unsigned int m_pageBase:20;
	};

	struct TableEntryL2
	{
		union
		{
			Fault fault;
			SmallPage smallPage;
		};

		bool IsFault(void)
		{
			if (fault.m_zero == 0)
				return true;
			else
				return false;
		}

		bool IsSmallPage(void)
		{
			return !IsFault();			//do a better version
		}

		void Print(Printer &p)
		{
			if (IsSmallPage())
				smallPage.Print(p);
			else if (IsFault())
				fault.Print(p);
			else
				ASSERT(0);
		}
	};
}

namespace PhysPages
{
#ifdef PBES
	static const unsigned int s_loadAddr = 0x82000000;
	static const unsigned int s_startAddr = 0x80000000;
	static const unsigned int s_startPage = s_startAddr >> 12;
#else
	static const unsigned int s_loadAddr = 0x00000000;
	static const unsigned int s_startAddr = 0x00000000;
	static const unsigned int s_startPage = s_startAddr >> 12;
#endif

	static const unsigned int s_totalPages = 128 * 256;

	void BlankUsedPages(void);
	void ReservePages(unsigned int start, unsigned int num);

	bool AllocL1Table(void);
	TranslationTable::TableEntryL1 *GetL1Table(void);

	void *FindPage(void);
	void ReleasePage(unsigned int p);

	void *FindMultiplePages(unsigned int num, unsigned int alignOrder);
	void ReleaseMultiplePages(unsigned int p, unsigned int num);
}

bool AddPageTable(void *pVirtual, unsigned int domain, TranslationTable::TableEntryL2 **ppNewTable);
bool RemovePageTable(void *pVirtual);

bool UnmapVirt(void *pVirt, unsigned int length);
bool MapPhysToVirt(void *pPhys, void *pVirt, unsigned int length,
		TranslationTable::AccessPerm perm, TranslationTable::ExecuteNever xn, TranslationTable::MemRegionType type, unsigned int domain);

void EnableMmu(bool on);

void *GetHighBrk(void);
void SetHighBrk(void *);

#endif /* COMMON_H_ */
