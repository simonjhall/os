/*
 * InterruptSource.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef INTERRUPTSOURCE_H_
#define INTERRUPTSOURCE_H_

#include <functional>

class InterruptSource
{
public:
	InterruptSource()
	: m_pHandler(0)
	{
	}

	virtual ~InterruptSource()
	{
	}

	virtual void EnableInterrupt(bool e) = 0;
	virtual unsigned int GetInterruptNumber(void) = 0;
	virtual bool HasInterruptFired(void) = 0;
	virtual void ClearInterrupt(void) = 0;


	virtual void OnInterrupt(void (*pFunc)(InterruptSource &))
	{
		m_pHandler = pFunc;
	}

	virtual void HandleInterrupt(void)
	{
		if (m_pHandler)
			(*m_pHandler)(*this);
	}

	virtual volatile unsigned int *GetFiqClearAddress(void)
	{
		return 0;
	}

	virtual unsigned int GetFiqClearValue(void)
	{
		return 0;
	}

protected:
	void (*m_pHandler)(InterruptSource &);
};


#endif /* INTERRUPTSOURCE_H_ */
