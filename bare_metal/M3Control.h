/*
 * M3Control.h
 *
 *  Created on: 17 Sep 2014
 *      Author: simon
 */

#ifndef M3CONTROL_H_
#define M3CONTROL_H_

#include "translation_table.h"
#include "InterruptSource.h"

namespace OMAP4460
{
namespace M3
{

void SetPowerState(bool e);
bool GetPowerState(void);

void ResetAll(void);
void ResetCpu(int id);

void ReleaseCacheMmu(void);
void EnableCpu(int id);

bool SetL1Table(TranslationTable::TableEntryL1 *pVirt);
TranslationTable::TableEntryL1 *GetL1TablePhys(bool viaIospace);

void EnableMmu(bool e);
void EnableBusError(bool e);
void FlushTlb(bool viaIospace);

class MmuInterrupt : public InterruptSource
{
public:
	MmuInterrupt(volatile unsigned int *pBase);
	virtual ~MmuInterrupt();

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	unsigned int ReadIrqStatus(void);

private:
	volatile unsigned int *m_pBase;
};

}
}

#endif /* M3CONTROL_H_ */
