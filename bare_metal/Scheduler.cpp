/*
 * Scheduler.cpp
 *
 *  Created on: 8 Jul 2013
 *      Author: simon
 */

#include "Scheduler.h"

Scheduler * Scheduler::s_pMasterScheduler = 0;

Scheduler::Scheduler()
: m_pIdleThread(0),
  m_pHandlerThread(0),
  m_pRunningThread(0)
{
	// TODO Auto-generated constructor stub

}

void Scheduler::AddThread(Thread &rThread)
{
	m_allNormalThreads.push_back(&rThread);
	OnThreadAdd(rThread, kNormal);
}

void Scheduler::RemoveThread(Thread &rThread, SpecialType t)
{
	switch (t)
	{
	case kNormal:
		m_allNormalThreads.remove(&rThread);
		break;
	case kIdleThread:
		m_pIdleThread = 0;
		break;
	case kHandlerThread:
		m_pHandlerThread = 0;
		break;
	}

	OnThreadRemove(rThread, t);
}

Scheduler::~Scheduler()
{
	// TODO Auto-generated destructor stub
}

Thread* Scheduler::WhatIsRunning(void)
{
	return m_pRunningThread;
}

void Scheduler::OnThreadAdd(Thread&, SpecialType)
{
}

void Scheduler::OnThreadRemove(Thread&, SpecialType)
{
}

bool Scheduler::AddSpecialThread(Thread &rThread, SpecialType specialType)
{
	switch (specialType)
	{
	default:
		ASSERT(0);
	case kNormal:
		return false;
	case kIdleThread:
		if (m_pIdleThread)
			RemoveThread(*m_pIdleThread, kIdleThread);

		m_pIdleThread = &rThread;
		break;
	case kHandlerThread:
		if (m_pHandlerThread)
			RemoveThread(*m_pHandlerThread, kHandlerThread);

		m_pHandlerThread = &rThread;
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

void Scheduler::SetRunning(Thread *pThread)
{
	m_pRunningThread = pThread;
}

RoundRobin::RoundRobin()
: Scheduler()
{
}

RoundRobin::~RoundRobin()
{
}

void RoundRobin::OnThreadAdd(Thread &rThread, SpecialType t)
{
	if (t == kNormal)
		m_threadQueue.push_back(&rThread);
}

Thread* RoundRobin::PickNext(Thread **ppBlocked)
{
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
			pNext = m_pIdleThread;
			ASSERT(pNext);			//no idle thread, all other thread blocked

			if (!pNext->SetState(Thread::kRunning))
				ASSERT(0);			//idle thread blocked or dead??
		}

		if (blocked_success)
		{
			*ppBlocked = pNext;

			//find the handler thread
			pNext = m_pHandlerThread;
			ASSERT(pNext);			//no handler thread, can't unblock

			if (!pNext->SetState(Thread::kRunning))
				ASSERT(0);			//handler thread blocked or dead??
		}

		//tell the old one is it not running
		if (WhatIsRunning() && WhatIsRunning() != pNext)	//but it may have done that itself already (kBlocked)
			WhatIsRunning()->SetState(Thread::kRunnable);

		//set next to be running
		SetRunning(pNext);

		return pNext;
	}
	else if (m_pIdleThread)		//not really going to come back from here though...
	{
		static bool message = false;

		if (!message)
		{
			PrinterUart<PL011> p;
			p << "no threads to run, scheduling idle thread\n";
			message = true;
		}

		Thread *pNext = m_pIdleThread;
		if (!pNext->SetState(Thread::kRunning))
			ASSERT(0);			//idle thread blocked or dead??

		if (WhatIsRunning() && WhatIsRunning() != pNext)	//but it may have done that itself already (kBlocked)
			WhatIsRunning()->SetState(Thread::kRunnable);

		SetRunning(pNext);
		return pNext;
	}
	else
	{
		SetRunning(0);
		return 0;
	}
}

void RoundRobin::OnThreadBlock(Thread &rThread)
{
	ASSERT(rThread.GetState() == Thread::kBlocked);
	m_blockedQueue.push_back(&rThread);
}

void RoundRobin::OnThreadUnblock(Thread &rThread)
{
	ASSERT(rThread.GetState() != Thread::kBlocked);
	m_blockedQueue.remove(&rThread);
}

void RoundRobin::OnThreadRemove(Thread &rThread, SpecialType t)
{
	if (t == kNormal)
		m_threadQueue.remove(&rThread);
}
