/*
 * Process.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "Process.h"
#include "common.h"

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
}
