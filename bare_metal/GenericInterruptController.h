/*
 * GenericInterruptController.h
 *
 *  Created on: 23 Jun 2013
 *      Author: simon
 */

#ifndef GENERICINTERRUPTCONTROLLER_H_
#define GENERICINTERRUPTCONTROLLER_H_

#include "InterruptController.h"

namespace CortexA9MPCore
{

class GenericInterruptController : public InterruptController
{
public:
	GenericInterruptController(volatile unsigned int *pProcBase, volatile unsigned int *pDistBase);
	virtual	~GenericInterruptController();

	virtual bool SoftwareInterrupt(unsigned int i, IntDestType type, unsigned destMask = 0);
protected:
	virtual bool Enable(InterruptType, unsigned int, bool e, IntDestType type, unsigned int targetMask);
	virtual void Clear(unsigned int);
	virtual int GetFiredId(void);

private:
	void EnableForwarding(bool);
	void EnableReceiving(bool);

	//http://infocenter.arm.com/help/topic/com.arm.doc.ddi0407i/DDI0407I_cortex_a9_mpcore_r4p1_trm.pdf
	//http://infocenter.arm.com/help/topic/com.arm.doc.ihi0048a/IHI0048A_gic_architecture_spec_v1_0.pdf
	//distributor registers
	static const unsigned int sm_icddcr = 0;					//distributor control register
	static const unsigned int sm_icdictr = 1;					//int controller type register
	static const unsigned int sm_icdiidr = 2;					//dist implementer ident register
	static const unsigned int sm_icdisrBase = 0x80 >> 2;		//int security registers
	static const unsigned int sm_icdiserBase = 0x100 >> 2;		//int set-enable registers
	static const unsigned int sm_icdicerBase = 0x180 >> 2;		//int clear-enable registers
	static const unsigned int sm_icdisprBase = 0x200 >> 2;		//int set-pending registers
	static const unsigned int sm_icdicprBase = 0x280 >> 2;		//int clear-pending registers
	static const unsigned int sm_icdabrBase = 0x300 >> 2;		//active bit registers
	static const unsigned int sm_icdiprBase = 0x400 >> 2;		//int priority registers
	static const unsigned int sm_icdiptrBase = 0x800 >> 2;		//int processor targets registers
	static const unsigned int sm_icdicfrBase = 0xc00 >> 2;		//int configuration registers
	static const unsigned int sm_icppisr = 0xd00 >> 2;			//ppi status register - A9 only
	static const unsigned int sm_icspisrBase = 0xd04 >> 2;		//spi status register - A9 only
	static const unsigned int sm_icdsgir = 0xf00 >> 2;			//sgi register

	//interface registers
	static const unsigned int sm_iccicr = 0;					//cpu interface control register
	static const unsigned int sm_iccpmr = 1;					//interrupt priority mask register
	static const unsigned int sm_iccbpr = 2;					//binary point register
	static const unsigned int sm_icciar = 3;					//interrupt ack register
	static const unsigned int sm_icceoir = 0x10 >> 2;			//end of interrupt register
	static const unsigned int sm_iccrpr = 0x14 >> 2;			//running prio register
	static const unsigned int sm_icchpir = 0x18 >> 2;			//highest pending int register
	static const unsigned int sm_iccabpr = 0x1c >> 2;			//aliased binary point register

	unsigned int m_maxInterrupt;
	volatile unsigned int *m_pProcBase, *m_pDistBase;
};

} /* namespace CortexA9MPCore */
#endif /* GENERICINTERRUPTCONTROLLER_H_ */
