/*
 * InterruptController.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef INTERRUPTCONTROLLER_H_
#define INTERRUPTCONTROLLER_H_

#include <map>
#include <list>

#include "InterruptSource.h"

class InterruptController
{
public:
	enum InterruptType
	{
		kIrq,
		kFiq,
	};

	enum IntDestType
	{
		kSelectedCpus,
		kAllOtherCpus,
		kThisCpu,
		kAllCpus,
	};

	InterruptController(void);
	virtual ~InterruptController(void);

	virtual bool RegisterInterrupt(InterruptSource &rSource, InterruptType type = kIrq);
	virtual bool DeRegisterInterrupt(InterruptSource &rSource);

	//auto clearing
	virtual InterruptSource *WhatHasFired(void);

	virtual bool SoftwareInterrupt(unsigned int i, IntDestType type, unsigned destMask = 0);

protected:
	virtual bool Enable(InterruptType, unsigned int, bool e, IntDestType type, unsigned int targetMask) = 0;
	virtual void Clear(unsigned int) = 0;
	virtual int GetFiredId(void) = 0;

	std::map<unsigned int, std::list<InterruptSource *> > m_sources;
};


#endif /* INTERRUPTCONTROLLER_H_ */
