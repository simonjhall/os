/*
 * GpTimer.h
 *
 *  Created on: 24 Jun 2013
 *      Author: simon
 */

#ifndef GPTIMER_H_
#define GPTIMER_H_

#include "PeriodicTimer.h"

namespace OMAP4460
{

class GpTimer: public PeriodicTimer
{
public:
	GpTimer(volatile unsigned int *pBase, volatile unsigned int *pClockBase, unsigned int whichTimer);
	virtual ~GpTimer();

	virtual void Enable(bool e);
	virtual bool IsEnabled(void);
	virtual bool SetFrequencyInMicroseconds(unsigned int micros);
	virtual unsigned int GetMicrosUntilFire(void);

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	unsigned int GetCurrentValue(void);

private:
	//1, 2, 10
	static const unsigned int sm_tiocp_cfg_1ms = 0x10 >> 2;
	static const unsigned int sm_tisr = 0x18 >> 2;
	static const unsigned int sm_tier = 0x1c >> 2;
	static const unsigned int sm_tclr = 0x24 >> 2;
	static const unsigned int sm_tcrr = 0x28 >> 2;
	static const unsigned int sm_tldr = 0x2c >> 2;
	static const unsigned int sm_ttgr = 0x30 >> 2;

	volatile unsigned int *m_pClockBase;
	unsigned int m_whichTimer;
};

} /* namespace OMAP4460 */
#endif /* GPTIMER_H_ */
