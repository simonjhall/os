/*
 * Process.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "Process.h"
#include "common.h"
#include "virt_memory.h"

namespace VirtMem
{
	extern FixedSizeAllocator<TranslationTable::SmallPageActual, 1048576 / sizeof(TranslationTable::SmallPageActual)> g_sysThreadStacks;
}

void *GetHighBrk(void)
{
//	return g_brk;
	ASSERT(0);
}

void SetHighBrk(void *p)
{
//	g_brk = p;
	ASSERT(0);
}

void* Process::GetBrk(void)
{
	return m_pBrk;
}

Process::Process(unsigned int entryPoint)
{
	m_threads.push_back(*new Thread(entryPoint, this, false));
}

void Process::MapProcess(void)
{

}

void Process::SetBrk(void *p)
{
	m_pBrk = p;
}

Thread::Thread(unsigned int entryPoint, Process* pParent, bool priv)
: m_pParent(pParent),
  m_isPriv(priv)
{
	m_pausedState.m_newPc = entryPoint;
	m_pausedState.m_spsr.m_t = entryPoint & 1;
	m_pausedState.m_spsr.m_f = 0;
	m_pausedState.m_spsr.m_i = 0;

	m_state = kRunnable;

	if (m_isPriv)
	{
		TranslationTable::SmallPageActual *sp;
		if (!VirtMem::g_sysThreadStacks.Allocate(&sp))
			ASSERT(0);

		m_pausedState.m_regs[13] = (unsigned int)sp;
		m_pausedState.m_spsr.m_mode = kSystem;
	}
}

Thread::State Thread::GetState(void)
{
	return m_state;
}

bool Thread::SetState(State target)
{
	switch (m_state)
	{
	case kRunning:
		if (target == kBlocked || target == kRunnable || target == kDead)
		{
			m_state = target;
			return true;
		}
		else
			return false;

	case kDead:				//not going anywhere
	case kBlocked:
		return false;		//needs an unblock

	case kRunnable:
		if (target == kRunning)
		{
			m_state = target;
			return true;
		}
		else
			return false;

	default:
		ASSERT(0);
		return false;
	}
}

void Thread::Unblock(void)
{
	m_state = kRunnable;
}

void Thread::HaveSavedState(ExceptionState exceptionState)
{
	m_pausedState = exceptionState;
}

bool Thread::Run(void)
{
	if (m_pParent)
	{
		ASSERT(!m_isPriv);
		m_pParent->MapProcess();
	}
	else
	{
		ASSERT(m_isPriv);
		//unmap the lo table
		VirtMem::SetL1TableLo(0);
	}

	Resume(&m_pausedState);

	return true;
}
