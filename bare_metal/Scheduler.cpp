/*
 * Scheduler.cpp
 *
 *  Created on: 8 Jul 2013
 *      Author: simon
 */

#include "Scheduler.h"

Scheduler * Scheduler::s_pMasterScheduler = 0;

Scheduler::Scheduler(unsigned int numCpus)
: m_pIdleThreads(0),
  m_pHandlerThreads(0),
  m_pRunningThreads(0),
  m_numCpus(numCpus)
{
	m_pIdleThreads = new Thread *[numCpus];
	m_pHandlerThreads = new Thread *[numCpus];
	m_pRunningThreads = new Thread *[numCpus];

	ASSERT(m_pIdleThreads);
	ASSERT(m_pHandlerThreads);
	ASSERT(m_pRunningThreads);

	for (unsigned int count = 0; count < numCpus; count++)
	{
		m_pIdleThreads[count] = 0;
		m_pHandlerThreads[count] = 0;
		m_pRunningThreads[count] = 0;
	}
}

void Scheduler::AddThread(Thread &rThread)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	m_allNormalThreads.push_back(&rThread);
	OnThreadAdd(rThread, kNormal);
}

void Scheduler::RemoveThread(Thread &rThread, SpecialType t)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	switch (t)
	{
	case kNormal:
		m_allNormalThreads.remove(&rThread);
		break;
	case kIdleThread:
		for (unsigned int count = 0; count < m_numCpus; count++)
			if (m_pIdleThreads[count] == &rThread)
			{
				m_pIdleThreads[count] = 0;
				break;
			}

		break;
	case kHandlerThread:
		for (unsigned int count = 0; count < m_numCpus; count++)
			if (m_pHandlerThreads[count] == &rThread)
			{
				m_pHandlerThreads[count] = 0;
				break;
			}

		break;
	default:
		ASSERT(0);
	}

	OnThreadRemove(rThread, t);
}

Scheduler::~Scheduler()
{
	// TODO Auto-generated destructor stub
}

Thread* Scheduler::WhatIsRunning(unsigned int cpu)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(cpu < m_numCpus);
	return m_pRunningThreads[cpu];
}

void Scheduler::OnThreadAdd(Thread&, SpecialType)
{
}

void Scheduler::OnThreadRemove(Thread&, SpecialType)
{
}

bool Scheduler::AddSpecialThread(Thread &rThread, SpecialType specialType)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(__builtin_popcount(rThread.GetAffinityMask()) == 1);
	unsigned int cpuId = __builtin_ctz(rThread.GetAffinityMask());

	switch (specialType)
	{
	default:
		ASSERT(0);
		return false;
	case kNormal:
		return false;
	case kIdleThread:
		if (m_pIdleThreads[cpuId])
			RemoveThread(*m_pIdleThreads[cpuId], kIdleThread);

		m_pIdleThreads[cpuId] = &rThread;
		break;
	case kHandlerThread:
		if (m_pHandlerThreads[cpuId])
			RemoveThread(*m_pHandlerThreads[cpuId], kHandlerThread);

		m_pHandlerThreads[cpuId] = &rThread;
		break;
	}

	OnThreadAdd(rThread, specialType);
	return true;
}

void Scheduler::OnThreadBlock(Thread &rThread)
{
	ASSERT(rThread.GetState() == Thread::kBlocked);
}

void Scheduler::OnThreadUnblock(Thread&)
{
}

bool Scheduler::IsUninterruptableRunning(unsigned int cpu)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(cpu < m_numCpus);
	if (m_pHandlerThreads[cpu] && WhatIsRunning(cpu) == m_pHandlerThreads[cpu])
		return true;
	else
		return false;
}

void Scheduler::SetRunning(Thread *pThread, unsigned int cpu)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(cpu < m_numCpus);
	m_pRunningThreads[cpu] = pThread;
}

RoundRobin::RoundRobin(unsigned int numCpus)
: Scheduler(numCpus)
{
}

RoundRobin::~RoundRobin()
{
}

void RoundRobin::OnThreadAdd(Thread &rThread, SpecialType t)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	if (t == kNormal)
		m_threadQueue.push_back(&rThread);
}

Thread* RoundRobin::PickNext(Thread **ppBlocked, unsigned int cpu)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(ppBlocked);
	*ppBlocked = 0;

	if (m_threadQueue.size())
	{
		bool success = false;
		bool blocked_success = false;
		size_t iterated = 0;

		Thread *pNext = 0;

		while (1)
		{
			//front of the list
			pNext = m_threadQueue.front();
			m_threadQueue.pop_front();

			//add it again to the back
			m_threadQueue.push_back(pNext);

			//linker errors
//			m_threadQueue.splice(m_threadQueue.end(), m_threadQueue, m_threadQueue.begin());

			iterated++;

			//try and make it runnable
			if (pNext->SetState(Thread::kRunning))
			{
				success = true;
				break;
			}

			if (pNext->GetState() == Thread::kBlocked)
			{
				blocked_success = true;
				success = true;
				break;
			}

			if (iterated == m_threadQueue.size())
				break;
		}

		if (!success)
		{
			//find the idle thread
			pNext = m_pIdleThreads[cpu];
			ASSERT(pNext);			//no idle thread, all other thread blocked

			if (!pNext->SetState(Thread::kRunning))
				ASSERT(0);			//idle thread blocked or dead??
		}

		if (blocked_success)
		{
			*ppBlocked = pNext;

			//find the handler thread
			pNext = m_pHandlerThreads[cpu];
			ASSERT(pNext);			//no handler thread, can't unblock

			if (!pNext->SetState(Thread::kRunning))
				ASSERT(0);			//handler thread blocked or dead??
		}

		auto pRunning = WhatIsRunning(cpu);

		//tell the old one is it not running
		if (pRunning && pRunning != pNext)	//but it may have done that itself already (kBlocked)
			pRunning->SetState(Thread::kRunnable);

		//set next to be running
		SetRunning(pNext, cpu);

		return pNext;
	}
	else if (m_pIdleThreads[cpu])		//not really going to come back from here though...
	{
		static bool message = false;

		if (!message)
		{
			Printer &p = Printer::Get();
			p << "no threads to run, scheduling idle thread\n";
			message = true;
		}

		Thread *pNext = m_pIdleThreads[cpu];
		if (!pNext->SetState(Thread::kRunning))
			ASSERT(0);			//idle thread blocked or dead??

		auto pRunning = WhatIsRunning(cpu);

		if (pRunning && pRunning != pNext)	//but it may have done that itself already (kBlocked)
			pRunning->SetState(Thread::kRunnable);

		SetRunning(pNext, cpu);
		return pNext;
	}
	else
	{
		SetRunning(0, cpu);
		return 0;
	}
}

void RoundRobin::OnThreadBlock(Thread &rThread)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(rThread.GetState() == Thread::kBlocked);
	m_blockedQueue.push_back(&rThread);
}

void RoundRobin::OnThreadUnblock(Thread &rThread)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	ASSERT(rThread.GetState() != Thread::kBlocked);
	m_blockedQueue.remove(&rThread);
}

void RoundRobin::OnThreadRemove(Thread &rThread, SpecialType t)
{
	ScopedLock<InterruptDisable> irq;
	WrappedScopedLock<RecursiveSpinLock> lock(m_lock, (void *)GetCpuId());

	if (t == kNormal)
		m_threadQueue.remove(&rThread);
}
