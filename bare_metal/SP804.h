/*
 * SP804.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef SP804_H_
#define SP804_H_

#include "PeriodicTimer.h"

namespace VersatilePb
{

class SP804 : public PeriodicTimer
{
public:
	SP804(volatile unsigned *pBase, unsigned int whichTimer);
	virtual ~SP804();

	virtual void Enable(bool e);
	virtual bool IsEnabled(void);
	virtual bool SetFrequencyInMicroseconds(unsigned int micros);

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	virtual volatile unsigned int *GetFiqClearAddress(void)
	{
		return &m_pBase[sm_timerIntClr];
	}

	virtual unsigned int GetFiqClearValue(void)
	{
		return 1;
	}

	unsigned int GetCurrentValue(void);

private:
	unsigned int m_whichTimer;

	//http://infocenter.arm.com/help/topic/com.arm.doc.ddi0271d/DDI0271.pdf

	static const unsigned int sm_timerLoad = 0;
	static const unsigned int sm_timerValue = 1;
	static const unsigned int sm_timerControl = 2;
	static const unsigned int sm_timerIntClr = 3;
	static const unsigned int sm_timerRis = 4;
	static const unsigned int sm_timerMis = 5;
	static const unsigned int sm_timerBgLoad = 6;
};

} /* namespace VersatilePb */
#endif /* SP804_H_ */
