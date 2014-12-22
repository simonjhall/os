/*
 * PL190.cpp
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#include "PL190.h"
#include "common.h"

namespace VersatilePb
{

PL190::PL190(volatile unsigned int *pBase)
: InterruptController(),
  m_pBase(pBase)
{
	// TODO Auto-generated constructor stub

}

PL190::~PL190()
{
	// TODO Auto-generated destructor stub
}

bool PL190::Enable(InterruptType interruptType, unsigned int i, bool e, IntDestType type, unsigned int targetMask)
{
	ASSERT(type == kAllCpus);

	if (i >= 32)
		return false;

	unsigned int t;
	if (interruptType == kIrq)
		t = 0;
	else
		t = 1;

	//clear the type
	m_pBase[sm_intSelect] &= ~(1 << i);
	//set the type
	m_pBase[sm_intSelect] |= (t << i);

	//set its enablement (sp?)
	m_pBase[sm_intEnable] &= ~(1 << i);
	m_pBase[sm_intEnable] |= ((unsigned int)e << i);

	return true;
}

void PL190::Clear(unsigned int i)
{
	//does not need clearing - clearing the peripheral does the trick
}

int PL190::GetFiredId(void)
{
	unsigned int mask = m_pBase[sm_irqStatus] | m_pBase[sm_fiqStatus];

	for (int count = 0; count < 32; count++)
		if (mask & (1 << count))
			return count;

	return -1;
}

} /* namespace VersatilePb */
