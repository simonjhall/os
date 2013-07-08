/*
 * Scheduler.cpp
 *
 *  Created on: 8 Jul 2013
 *      Author: simon
 */

#include "Scheduler.h"

Scheduler::Scheduler()
: m_pRunningThread(0)
{
	// TODO Auto-generated constructor stub

}

void Scheduler::AddThread(Thread &rThread)
{
	m_allThreads.push_back(&rThread);
	OnThreadAdd(rThread);
}

Scheduler::~Scheduler()
{
	// TODO Auto-generated destructor stub
}

RoundRobin::RoundRobin()
: Scheduler()
{
}

RoundRobin::~RoundRobin()
{
}

Thread* RoundRobin::PickNext(void)
{
	if (m_threadQueue.size())
	{
		bool success = false;
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

			if (iterated == m_threadQueue.size())
				break;
		}

		ASSERT(success);		//else all threads blocked

		//tell the old one is it not running
		if (WhatIsRunning())	//but it may have done that itself already (kBlocked)
			WhatIsRunning()->SetState(Thread::kRunnable);

		//set next to be running
		SetRunning(pNext);

		return pNext;
	}
	else
	{
		SetRunning(0);
		return 0;
	}
}

Thread* Scheduler::WhatIsRunning(void)
{
	return m_pRunningThread;
}

void Scheduler::OnThreadAdd(Thread&)
{
}

void RoundRobin::OnThreadAdd(Thread &rThread)
{
	m_threadQueue.push_back(&rThread);
}

void Scheduler::SetRunning(Thread *pThread)
{
	m_pRunningThread = pThread;
}
