/*
 * GpTimer.cpp
 *
 *  Created on: 24 Jun 2013
 *      Author: simon
 */

#include "GpTimer.h"
#include "common.h"

namespace OMAP4460
{

GpTimer::GpTimer(volatile unsigned int* pBase,
		volatile unsigned int* pClockBase, unsigned int whichTimer)
: PeriodicTimer(pBase),
  m_pClockBase(pClockBase),
  m_whichTimer(whichTimer)
{
	ASSERT(whichTimer == 1 || whichTimer == 2 || whichTimer == 10);

	unsigned int current = m_pBase[sm_tclr] & 0xffff8000;
	//disable the timer [0]=0
	//set autoreload mode [1]=1
	current |= 2;
	//disable prescaling[4:2]=0, [5]=0
	//disable compare[6]=0
	//disable pwm[7]=0
	//disable capture[9:8]=0

	m_pBase[sm_tclr] = current;

	//enable the interrupt
	EnableInterrupt(true);
}

GpTimer::~GpTimer()
{
	Enable(false);
	// TODO Auto-generated destructor stub
}

void GpTimer::Enable(bool e)
{
	if (e)
		m_pBase[sm_ttgr] = 1;		//make it take effect immediately
	m_pBase[sm_tclr] = (m_pBase[sm_tclr] & ~1) | (unsigned int)e;
}

bool GpTimer::IsEnabled(void)
{
	return (bool)(m_pBase[sm_tclr] & 1);
}

bool GpTimer::SetFrequencyInMicroseconds(unsigned int micros)
{
	//base clock is 38.4 MHz
	//so one microsecond is a load value of 38.4
	unsigned int load_value = 0xffffffff - (unsigned int)(micros * 38.4f);
	if (load_value != 0xffffffff)
	{
		m_pBase[sm_tldr] = load_value;
		return true;
	}
	else
		return false;
}

void GpTimer::EnableInterrupt(bool e)
{
	m_pBase[sm_tier] = (unsigned int)e << 1;		//on overflow
}

unsigned int GpTimer::GetInterruptNumber(void)
{
	return (m_whichTimer - 1) + 37 + 32;
}

bool GpTimer::HasInterruptFired(void)
{
	return m_pBase[sm_tisr] ? true : false;
}

void GpTimer::ClearInterrupt(void)
{
	m_pBase[sm_tisr] = 7;
}

unsigned int GpTimer::GetCurrentValue(void)
{
	return m_pBase[sm_tcrr];
}

} /* namespace OMAP4460 */
