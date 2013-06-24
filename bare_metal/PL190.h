/*
 * PL190.h
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#ifndef PL190_H_
#define PL190_H_

#include "InterruptController.h"

namespace VersatilePb
{

class PL190 : public InterruptController
{
public:
	PL190(volatile unsigned int *pBase);
	virtual ~PL190();

protected:
	virtual bool Enable(InterruptType, unsigned int, bool);
	virtual void Clear(unsigned int);
	virtual unsigned int GetFiredMask(void);

private:
	volatile unsigned int *m_pBase;

	//http://infocenter.arm.com/help/topic/com.arm.doc.ddi0181e/DDI0181.pdf

	static const unsigned int sm_irqStatus = 0;
	static const unsigned int sm_fiqStatus = 1;
	static const unsigned int sm_rawIntr = 2;
	static const unsigned int sm_intSelect = 3;
	static const unsigned int sm_intEnable = 4;
	static const unsigned int sm_intEnableClear = 5;
	static const unsigned int sm_softInt = 6;
	static const unsigned int sm_softIntClear = 7;
};

} /* namespace VersatilePb */
#endif /* PL190_H_ */
