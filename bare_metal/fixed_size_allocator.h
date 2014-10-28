/*
 * fixed_size_allocator.h
 *
 *  Created on: 17 May 2013
 *      Author: simon
 */

#ifndef FIXED_SIZE_ALLOCATOR_H_
#define FIXED_SIZE_ALLOCATOR_H_

#include "common.h"

template <class T, unsigned int max>
class FixedSizeAllocator
{
public:
	void Init(T *pBase)
	{
		m_pBase = pBase;
		m_used = 0;

		ASSERT(pBase);

		for (unsigned int count = 0; count < max; count++)
			m_slot[count] = false;
	}

	bool IsFull(void)
	{
		return m_used == max;
	}

	bool Allocate(T **pOut)
	{
		for (unsigned int count = 0; count < max; count++)
			if (m_slot[count] == false)
			{
				m_used++;
				m_slot[count] = true;
				*pOut = m_pBase + count;

				return true;
			}

		return false;
	}

	bool Free(T *pIn)
	{
		unsigned int slot = pIn - m_pBase;

		if (slot >= max)
			return false;

		if (m_slot[slot] == false)
			return false;

		m_slot[slot] = false;
		m_used--;

		return true;
	}

	unsigned int GetVirtBaseInMb(void)
	{
		return (unsigned int)m_pBase >> 20;
	}

	unsigned int GetLengthMb(void)
	{
		return (((max * sizeof(T)) + 1048575) & ~1048575) >> 20;
	}

private:
	T *m_pBase;

	bool m_slot[max];

	unsigned int m_used;
};


#endif /* FIXED_SIZE_ALLOCATOR_H_ */
