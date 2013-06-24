/*
 * InterruptSource.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef INTERRUPTSOURCE_H_
#define INTERRUPTSOURCE_H_

class InterruptSource
{
public:
	InterruptSource()
	{
	};
	virtual ~InterruptSource()
	{
	};

	virtual void EnableInterrupt(bool e) = 0;
	virtual unsigned int GetInterruptNumber(void) = 0;
	virtual bool HasInterruptFired(void) = 0;
	virtual void ClearInterrupt(void) = 0;

	virtual volatile unsigned int *GetFiqClearAddress(void)
	{
		return 0;
	}

	virtual unsigned int GetFiqClearValue(void)
	{
		return 0;
	}
};


#endif /* INTERRUPTSOURCE_H_ */
