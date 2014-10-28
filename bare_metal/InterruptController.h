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

	InterruptController(void);
	virtual ~InterruptController(void);

	virtual bool RegisterInterrupt(InterruptSource &rSource, InterruptType type = kIrq);
	virtual bool DeRegisterInterrupt(InterruptSource &rSource);

	//auto clearing
	virtual InterruptSource *WhatHasFired(void);

	virtual bool SoftwareInterrupt(unsigned int i);

protected:
	virtual bool Enable(InterruptType, unsigned int, bool) = 0;
	virtual void Clear(unsigned int) = 0;
	virtual int GetFiredId(void) = 0;

	std::map<unsigned int, std::list<InterruptSource *> > m_sources;
};


#endif /* INTERRUPTCONTROLLER_H_ */
