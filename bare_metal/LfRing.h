/*
 * LfRing.h
 *
 *  Created on: 16 Oct 2014
 *      Author: simon
 */

#ifndef LFRING_H_
#define LFRING_H_

#include "Process.h"

template <class T, int Num, int Mask>
class LfRing
{
public:
	inline LfRing(void)
	: m_head(0),
	  m_tail(0),
	  m_sem(0)
	{
	}

	inline ~LfRing(void)
	{
	}

	inline bool Push(const T &rVal)
	{
		//if moving up the head pointer equals
		//tail pointer then fail (wasting a slot)
		if (((m_head + 1) & Mask) == m_tail)
			return false;

		m_ring[m_head] = rVal;
		m_head = (m_head + 1) & Mask;

		m_sem.Signal();

		return true;
	}

	inline void Pop(T &rOut)
	{
		m_sem.Wait();

		rOut = m_ring[m_tail];
		m_tail = (m_tail + 1) & Mask;
	}

protected:
	int m_head;				//first place to insert
	int m_tail;				//oldest valid element
	T m_ring[Num];
	Semaphore m_sem;
};


#endif /* LFRING_H_ */
