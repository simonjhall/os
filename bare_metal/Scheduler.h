/*
 * Scheduler.h
 *
 *  Created on: 8 Jul 2013
 *      Author: simon
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "Process.h"
#include <list>

class Scheduler
{
public:
	virtual Thread *PickNext(void) = 0;
	virtual Thread *WhatIsRunning(void);

	virtual void AddThread(Thread &);

protected:
	Scheduler();
	virtual ~Scheduler();

	virtual void OnThreadAdd(Thread &);
	virtual void SetRunning(Thread *);

	std::list<Thread *> m_allThreads;
	Thread *m_pRunningThread;
};

class RoundRobin : public Scheduler
{
public:
	RoundRobin();
	virtual ~RoundRobin();

	virtual Thread *PickNext(void);

protected:
	virtual void OnThreadAdd(Thread &);

	std::list<Thread *> m_threadQueue;
};

#endif /* SCHEDULER_H_ */
