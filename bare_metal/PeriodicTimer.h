/*
 * PeriodicTimer.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef PERIODICTIMER_H_
#define PERIODICTIMER_H_

#include "InterruptSource.h"

class PeriodicTimer : public InterruptSource
{
public:
	PeriodicTimer(volatile unsigned int *pBase)
	: InterruptSource(),
	  m_pBase(pBase)
	{
	}

	virtual ~PeriodicTimer()
	{
	}

	virtual void Enable(bool e) = 0;
	virtual bool IsEnabled(void) = 0;
	virtual bool SetFrequencyInMicroseconds(unsigned int micros) = 0;
	virtual unsigned int GetMicrosUntilFire(void) = 0;

protected:
	volatile unsigned int *m_pBase;
};


#endif /* PERIODICTIMER_H_ */
