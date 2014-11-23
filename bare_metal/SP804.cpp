/*
 * SP804.cpp
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#include "SP804.h"
#include "common.h"

namespace VersatilePb
{

SP804::SP804(volatile unsigned *pBase, unsigned int whichTimer)
: PeriodicTimer(pBase),
  m_whichTimer(whichTimer)
{
	ASSERT(pBase);
	ASSERT(whichTimer < 2);

	//get the current reserved bits
	unsigned int current = m_pBase[sm_timerControl] & 0xffffff10;

	//disable the timer [7]=0
	//set to periodic mode [6]=1
	current |= (1 << 6);
	//enable the interrupt [5]=1
	current |= (1 << 5);
	//leave the clock div 1
	//set to 32-bit mode [1]=1
	current |= (1 << 1);
	//set wrapping [0]=0

	m_pBase[sm_timerControl] = current;
}

SP804::~SP804()
{
	Enable(false);
}

void SP804::Enable(bool e)
{
	//get current value
	unsigned int control = m_pBase[sm_timerControl];
	//clear the enable bit
	control = control & ~(1 << 7);
	//set the new bit
	control = control | ((unsigned int)e << 7);

	m_pBase[sm_timerControl] = control;
}

bool SP804::IsEnabled(void)
{
	//get current value
	unsigned int control = m_pBase[sm_timerControl];
	//return the enable bit
	return (bool)((control >> 7) & 1);
}

bool SP804::SetFrequencyInMicroseconds(unsigned int micros)
{
	m_pBase[sm_timerLoad] = micros;
	return true;
}

unsigned int SP804::GetInterruptNumber(void)
{
	if (m_whichTimer == 0)
		return 4;
	else
		return 5;
}

void SP804::EnableInterrupt(bool e)
{
	unsigned int current = m_pBase[sm_timerControl];
	//clear the current interrupt flag
	current = current & ~(1 << 5);
	//enable the interrupt [5]=1
	current |= ((unsigned int)e << 5);

	m_pBase[sm_timerControl] = current;
}

bool SP804::HasInterruptFired(void)
{
	return (bool)(m_pBase[sm_timerMis] & 1);
}

void SP804::ClearInterrupt(void)
{
	m_pBase[sm_timerIntClr] = 1;
}

unsigned int SP804::GetCurrentValue(void)
{
	return m_pBase[sm_timerValue];
}

unsigned int SP804::GetMicrosUntilFire(void)
{
	return GetCurrentValue();
}

} /* namespace VersatilePb */
