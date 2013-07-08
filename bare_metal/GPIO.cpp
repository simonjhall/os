/*
 * GPIO.cpp
 *
 *  Created on: 25 Jun 2013
 *      Author: simon
 */

#include "GPIO.h"
#include "common.h"

namespace OMAP4460
{

GPIO::GPIO(volatile unsigned int *pBase, unsigned int whichOne)
{
	m_pSysConfig = &pBase[0x10 >> 2];
	m_pIrqStatusRaw0 = &pBase[0x24 >> 2];
	m_pIrqStatus0 = &pBase[0x2c >> 2];
	m_pIrqStatusSet0 = &pBase[0x34 >> 2];
	m_pIrqStatusClr0 = &pBase[0x3c >> 2];
	m_pIrqWaken0 = &pBase[0x44 >> 2];
	m_pSysStatus = &pBase[0x114 >> 2];
	m_pCtrl = &pBase[0x130 >> 2];
	m_pOe = &pBase[0x134 >> 2];
	m_pDataIn = &pBase[0x138 >> 2];
	m_pDataOut = &pBase[0x13c >> 2];
	m_pLevelDetect0 = &pBase[0x140 >> 2];
	m_pLevelDetect1 = &pBase[0x144 >> 2];
	m_pRisingDetect = &pBase[0x148 >> 2];
	m_pFallingDetect = &pBase[0x14c >> 2];
	m_pDebounceEnable = &pBase[0x150 >> 2];
	m_pDebouncingTime = &pBase[0x154 >> 2];
	m_pClearDataOut = &pBase[0x190 >> 2];
	m_pSetDataOut = &pBase[0x194 >> 2];

	m_whichOne = whichOne;

	//software reset[1]=1
	*m_pSysConfig |= 2;

	//wait until reset completed
	while ((*m_pSysStatus & 1) == 0);

	//set no-idle
	*m_pSysStatus |= (1 << 3);

	//set all detection to false
	*m_pLevelDetect0 = 0;
	*m_pLevelDetect1 = 0;
	*m_pRisingDetect = 0;
	*m_pFallingDetect = 0;

	//enable debouncing
	*m_pDebounceEnable = 1;
	*m_pDebouncingTime = 0xff;	//max

	//set all as inputs
	*m_pOe = 0xffffffff;

	//disable all interrupts
	*m_pIrqStatusClr0 = 0xffffffff;
	//disable all interrupt events
	*m_pIrqStatus0 = 0;

	for (unsigned int count = 0; count < 32; count++)
		m_pins[count].Init(this, count);
}

GPIO::~GPIO()
{
	//disable all interrupts
	*m_pIrqStatusClr0 = 0xffffffff;
}

unsigned int GPIO::Read(void)
{
	return *m_pDataIn;
}

void GPIO::Write(unsigned int w)
{
	*m_pDataOut = w;
}

void GPIO::SetAs(Mode m, unsigned int pin)
{
	unsigned int current = *m_pOe;
	current = (current & ~(1 << pin)) | ((unsigned int)m << pin);
	*m_pOe = current;
}

void GPIO::EnableInterrupt(bool e, InterruptOn on, unsigned int p)
{
	if (e)
		*m_pIrqStatusSet0 |= (1 << p);
	else
		*m_pIrqStatusClr0 |= (1 << p);

	//disable on both first
	*m_pRisingDetect = *m_pRisingDetect & ~(1 << p);
	*m_pFallingDetect = *m_pFallingDetect & ~(1 << p);

	//now set what's required
	switch (on)
	{
	case kRising:
		*m_pRisingDetect = *m_pRisingDetect | (1 << p);
		break;
	case kFalling:
		*m_pFallingDetect = *m_pFallingDetect | (1 << p);
		break;
	case kBoth:
		*m_pRisingDetect = *m_pRisingDetect | (1 << p);
		*m_pFallingDetect = *m_pFallingDetect | (1 << p);
		break;
	default:
		ASSERT(0);
	}
}

bool GPIO::HasInterruptFired(unsigned int p)
{
	return (*m_pIrqStatus0 & (1 << p)) ? true : false;
}

void GPIO::ClearInterrupt(unsigned int p)
{
	//http://www.cs.fsu.edu/~baker/devices/lxr/http/source/linux/arch/arm/plat-omap/gpio.c?v=2.6.25#L760

	//disable the interrupt
	*m_pIrqStatusClr0 = 1 << p;
	//clear the status
	*m_pIrqStatus0 = *m_pIrqStatus0 | (1 << p);
	//re-enable the interrupt
	*m_pIrqStatusSet0 = 1 << p;
}

GPIO::Pin& GPIO::GetPin(unsigned int p)
{
	ASSERT(p < 32);
	return m_pins[p];
}

unsigned int GPIO::GetInterruptNumber(void)
{
	return (m_whichOne - 1) + 29 + 32;
}

} /* namespace OMAP4460 */

OMAP4460::GPIO::Pin::Pin(void)
: InterruptSource(),
  m_interruptEnabled (false)
{
}

OMAP4460::GPIO::Pin::~Pin(void)
{
}

void OMAP4460::GPIO::Pin::Init(GPIO *pParent, unsigned int p)
{
	m_pParent = pParent;
	m_pin = p;
}

void OMAP4460::GPIO::Pin::SetAs(Mode m)
{
	m_pParent->SetAs(m, m_pin);
}

void OMAP4460::GPIO::Pin::SetInterruptType(InterruptOn interruptOn)
{
	if (m_interruptEnabled)
	{
		EnableInterrupt(false);
		m_intType = interruptOn;
		EnableInterrupt(true);
	}
	else
		m_intType = interruptOn;		//just change it
}

void OMAP4460::GPIO::Pin::EnableInterrupt(bool e)
{
	m_interruptEnabled = e;
	m_pParent->EnableInterrupt(e, m_intType, m_pin);
}

unsigned int OMAP4460::GPIO::Pin::GetInterruptNumber(void)
{
	return m_pParent->GetInterruptNumber();
}

bool OMAP4460::GPIO::Pin::HasInterruptFired(void)
{
	return m_pParent->HasInterruptFired(m_pin);
}

bool OMAP4460::GPIO::Pin::Read(void)
{
	return (m_pParent->Read() & (1 << m_pin)) ? true : false;
}

void OMAP4460::GPIO::Pin::Write(bool e)
{
	m_pParent->Write((m_pParent->Read() & ~(1 << m_pin)) | ((unsigned int)e << m_pin));
}

OMAP4460::GPIO::InterruptOn OMAP4460::GPIO::Pin::GetInterruptType(void)
{
	return m_intType;
}

void OMAP4460::GPIO::Pin::ClearInterrupt(void)
{
	m_pParent->ClearInterrupt(m_pin);
}
