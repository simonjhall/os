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
	enum SpecialType
	{
		kNormal,
		kIdleThread,
		kHandlerThread,
	};

	virtual Thread *PickNext(Thread **ppBlocked, unsigned int cpu) = 0;
	virtual Thread *WhatIsRunning(unsigned int cpu);
	virtual bool IsUninterruptableRunning(unsigned int cpu);

	virtual void AddThread(Thread &);
	virtual bool AddSpecialThread(Thread &, SpecialType);

	virtual void RemoveThread(Thread &, SpecialType = kNormal);

	virtual void OnThreadBlock(Thread &);
	virtual void OnThreadUnblock(Thread &);

	static Scheduler &GetMasterScheduler(void)
	{
		return *s_pMasterScheduler;
	}

	static void SetMasterScheduler(Scheduler &rSched)
	{
		s_pMasterScheduler = &rSched;
	}

protected:
	Scheduler(unsigned int numCpus);
	virtual ~Scheduler();

	virtual void OnThreadAdd(Thread &, SpecialType);
	virtual void OnThreadRemove(Thread &, SpecialType);
	virtual void SetRunning(Thread *, unsigned int cpu);

	std::list<Thread *> m_allNormalThreads;
	Thread **m_pIdleThreads, **m_pHandlerThreads;
	Thread **m_pRunningThreads;

	unsigned int m_numCpus;
	RecursiveSpinLock m_lock;

	static Scheduler *s_pMasterScheduler;
};

class RoundRobin : public Scheduler
{
public:
	RoundRobin(unsigned int numCpus);
	virtual ~RoundRobin();

	virtual Thread *PickNext(Thread **ppBlocked, unsigned int cpu);
	virtual void OnThreadBlock(Thread &);
	virtual void OnThreadUnblock(Thread &);

protected:
	virtual void OnThreadAdd(Thread &, SpecialType);
	virtual void OnThreadRemove(Thread &, SpecialType);

	std::list<Thread *> m_threadQueue;
	std::list<Thread *> m_blockedQueue;
};

void Handler(unsigned int arg0, unsigned int arg1);
void IdleThread(void);

#endif /* SCHEDULER_H_ */
