/*
 * M3Control.cpp
 *
 *  Created on: 17 Sep 2014
 *      Author: simon
 */

#include "M3Control.h"
#include "IoSpace.h"
#include "virt_memory.h"
#include "common.h"

#include "print_uart.h"

namespace OMAP4460
{
namespace M3
{

void SetPowerState(bool e)
{
	//enable/disable the module

	//page p31
	volatile unsigned int *CM_MPU_M3_MPU_M3_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x920 >> 2];
//	ASSERT((unsigned int)CM_MPU_M3_MPU_M3_CLKCTRL == 0x4a008920);
//	p << "CM_MPU_M3_MPU_M3_CLKCTRL was " << *CM_MPU_M3_MPU_M3_CLKCTRL << "\n";

	*CM_MPU_M3_MPU_M3_CLKCTRL = (*CM_MPU_M3_MPU_M3_CLKCTRL & ~1) | (e ? 1 : 0);

	asm volatile ("dsb");

	//wake it up
	if (e)
	{
		//page 928
		volatile unsigned int *CM_MPU_M3_CLKSTCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x900 >> 2];
//		ASSERT((unsigned int)CM_MPU_M3_CLKSTCTRL == 0x4a008900);

//		p << "CM_MPU_M3_CLKSTCTRL was " << *CM_MPU_M3_CLKSTCTRL << "\n";
		*CM_MPU_M3_CLKSTCTRL = (*CM_MPU_M3_CLKSTCTRL & ~3) | 2;
//		p << "CM_MPU_M3_CLKSTCTRL is " << *CM_MPU_M3_CLKSTCTRL << "\n";

		asm volatile ("dsb");
	}

//	p << "CM_MPU_M3_MPU_M3_CLKCTRL is " << *CM_MPU_M3_MPU_M3_CLKCTRL << "\n";
}

bool GetPowerState(void)
{
	//CM_MPU_M3_MPU_M3_CLKCTRL, 931
	volatile unsigned int *CM_MPU_M3_MPU_M3_CLKCTRL = &IoSpace::GetDefaultIoSpace()->Get("CM2")[0x920 >> 2];
//	ASSERT((unsigned int)CM_MPU_M3_MPU_M3_CLKCTRL == 0x4a008920);

	if (*CM_MPU_M3_MPU_M3_CLKCTRL & 1)
		return true;
	else
		return false;
}

void ResetAll(void)
{
//	Printer &p = Printer::Get();

	//page 630
	volatile unsigned int *RM_MPU_M3_RSTCTRL = &IoSpace::GetDefaultIoSpace()->Get("PRM")[0x910 >> 2];
//	ASSERT((unsigned int)RM_MPU_M3_RSTCTRL == 0x4a306910);

//	p << "RM_MPU_M3_RSTCTRL was " << *RM_MPU_M3_RSTCTRL << "\n";
	*RM_MPU_M3_RSTCTRL = 7;
//	p << "RM_MPU_M3_RSTCTRL is " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
}

void ResetCpu(int id)
{
	//page 630
	volatile unsigned int *RM_MPU_M3_RSTCTRL = &IoSpace::GetDefaultIoSpace()->Get("PRM")[0x910 >> 2];
//	ASSERT((unsigned int)RM_MPU_M3_RSTCTRL == 0x4a306910);

	ASSERT(id == 1 || id == 2);

	*RM_MPU_M3_RSTCTRL = *RM_MPU_M3_RSTCTRL | id;

	asm volatile ("dsb");
}

void ReleaseCacheMmu(void)
{
//	Printer &p = Printer::Get();
	//page 630
	volatile unsigned int *RM_MPU_M3_RSTCTRL = &IoSpace::GetDefaultIoSpace()->Get("PRM")[0x910 >> 2];
//	ASSERT((unsigned int)RM_MPU_M3_RSTCTRL == 0x4a306910);

//	p << "RM_MPU_M3_RSTCTRL was " << *RM_MPU_M3_RSTCTRL << "\n";
	*RM_MPU_M3_RSTCTRL = *RM_MPU_M3_RSTCTRL & ~4;
//	p << "RM_MPU_M3_RSTCTRL is " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
}

void EnableCpu(int id)
{
//	Printer &p = Printer::Get();
	//page 630
	volatile unsigned int *RM_MPU_M3_RSTCTRL = &IoSpace::GetDefaultIoSpace()->Get("PRM")[0x910 >> 2];
//	ASSERT((unsigned int)RM_MPU_M3_RSTCTRL == 0x4a306910);

	ASSERT(id == 1 || id == 2);

//	p << "RM_MPU_M3_RSTCTRL was " << *RM_MPU_M3_RSTCTRL << "\n";
	*RM_MPU_M3_RSTCTRL = *RM_MPU_M3_RSTCTRL & ~id;
//	p << "RM_MPU_M3_RSTCTRL is " << *RM_MPU_M3_RSTCTRL << "\n";

	asm volatile ("dsb");
}

bool SetL1Table(TranslationTable::TableEntryL1 *pVirt)
{
	if (pVirt)
	{
		//get phys
		TranslationTable::TableEntryL1 *pPhys;
		//needs to be mapped
		if (!VirtMem::VirtToPhys(pVirt, &pPhys))
			return false;

		if ((unsigned int)pPhys & 16383)
			return false;

//		PrinterUart<PL011> p;

		//page 4431
		volatile unsigned int *MMU_TTB = &IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU")[0x4c >> 2];
//		p << "MMU_TTB was " << *MMU_TTB << "\n";

//		ASSERT((unsigned int)MMU_TTB == 0x5508204c);
		*MMU_TTB = (unsigned int)pPhys;

//		p << "MMU_TTB is " << *MMU_TTB << "\n";

		asm volatile ("dsb");
	}
	else
		return false;

	return true;
}

TranslationTable::TableEntryL1 *GetL1TablePhys(bool viaIospace)
{
	if (viaIospace)
	{
		volatile unsigned int *MMU_TTB = &IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU")[0x4c >> 2];
		return (TranslationTable::TableEntryL1 *)*MMU_TTB;
	}
	else
	{
		volatile unsigned int *MMU_TTB = (volatile unsigned int *)0x5508204c;
		return (TranslationTable::TableEntryL1 *)*MMU_TTB;
	}
}

void EnableMmu(bool e)
{
//	Printer &p = Printer::Get();
	//page 4430
	volatile unsigned int *MMU_CNTL = &IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU")[0x44 >> 2];
//	ASSERT((unsigned int)MMU_CNTL == 0x55082044);

//	p << "MMU_CNTL was " << *MMU_CNTL << "\n";

	if (e)
		*MMU_CNTL = 6;
	else
		*MMU_CNTL = 0;

//	p << "MMU_CNTL is " << *MMU_CNTL << "\n";

	asm volatile ("dsb");
}

void EnableBusError(bool e)
{
	volatile unsigned int *MMU_GP_REG = &IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU")[0x88 >> 2];
	if (e)
		*MMU_GP_REG = 1;
	else
		*MMU_GP_REG = 0;
}

void FlushTlb(bool viaIospace)
{
	if (viaIospace)
	{
		volatile unsigned int *MMU_GFLUSH = &IoSpace::GetDefaultIoSpace()->Get("CORTEXM3_L2MMU")[0x60 >> 2];
		*MMU_GFLUSH = 1;
	}
	else
	{
		volatile unsigned int *MMU_GFLUSH = (volatile unsigned int *)0x55082060;
		*MMU_GFLUSH = 1;
	}
}

MmuInterrupt::MmuInterrupt(volatile unsigned int* pBase)
: InterruptSource(),
  m_pBase(pBase)
{
	//clear existing interrupts
	ClearInterrupt();
}

MmuInterrupt::~MmuInterrupt()
{
}

void MmuInterrupt::EnableInterrupt(bool e)
{
	if (e)
		m_pBase[0x1c >> 2] = 31;
	else
		m_pBase[0x1c >> 2] = 0;
}

unsigned int MmuInterrupt::GetInterruptNumber(void)
{
	return 100 + 32;
}

bool MmuInterrupt::HasInterruptFired(void)
{
	if (ReadIrqStatus())
		return true;
	else
		return false;
}

unsigned int MmuInterrupt::ReadIrqStatus(void)
{
	return m_pBase[0x18 >> 2];
}

void MmuInterrupt::ClearInterrupt(void)
{
	m_pBase[0x18 >> 2] = 31;
}

}
}
