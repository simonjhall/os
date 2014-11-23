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

	//reset it
	m_pBase[sm_tiocp_cfg_1ms] = 2;
	while (m_pBase[sm_tiocp_cfg_1ms] & 2);

	//disable idling and clock activity
	m_pBase[sm_tiocp_cfg_1ms] = (1 << 3) | (3 << 8);

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
	unsigned int load_value;
	if (m_whichTimer == 1)			//32.768 kHz
	{
		//a load value of one = 32.768 us
		//1 us = 0.030517578 us
		load_value = 0xffffffff - (unsigned int)(micros / 32.768f);
	}
	else
		//base clock is 38.4 MHz
		//so one microsecond is a load value of 38.4
		load_value = 0xffffffff - (unsigned int)(micros * 38.4f);

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

unsigned int GpTimer::GetMicrosUntilFire(void)
{
	unsigned int current = 0xffffffff - GetCurrentValue();

	if (m_whichTimer == 1)
		//1/32.768 = 1 us
		return (unsigned int)(current * 32.768f);
	else
		//38.4 = 1 us
		return (unsigned int)(current / 38.4f);
}

} /* namespace OMAP4460 */
