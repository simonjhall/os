/*
 * GPIO.h
 *
 *  Created on: 25 Jun 2013
 *      Author: simon
 */

#ifndef GPIO_H_
#define GPIO_H_

#include "InterruptSource.h"

namespace OMAP4460
{

class GPIO
{
public:
	enum Mode
	{
		kOutput = 0,
		kInput = 1,
	};

	enum InterruptOn
	{
		kFalling,
		kRising,
		kBoth,
	};

	class Pin : public InterruptSource
	{
	public:
		Pin(void);
		virtual ~Pin(void);

		void Init(GPIO *pParent, unsigned int p);
		void SetAs(Mode m);
		bool Read(void);
		void Write(bool e);

		void SetInterruptType(InterruptOn);
		InterruptOn GetInterruptType(void);

		virtual void EnableInterrupt(bool e);
		virtual unsigned int GetInterruptNumber(void);
		virtual bool HasInterruptFired(void);
		virtual void ClearInterrupt(void);

	private:
		GPIO *m_pParent;
		unsigned int m_pin;
		InterruptOn m_intType;
		bool m_interruptEnabled;
	};

	/////////////////

	GPIO(volatile unsigned int *pBase, unsigned int whichOne);
	virtual ~GPIO();

	virtual unsigned int Read(void);
	virtual void Write(unsigned int);

	virtual void SetAs(Mode m, unsigned int pin);

	virtual Pin &GetPin(unsigned int);

protected:
	void EnableInterrupt(bool e, InterruptOn on, unsigned int p);
	bool HasInterruptFired(unsigned int p);
	void ClearInterrupt(unsigned int p);
	unsigned int GetInterruptNumber(void);

private:
	volatile unsigned int *m_pSysConfig;
	volatile unsigned int *m_pSysStatus;
	volatile unsigned int *m_pLevelDetect0;
	volatile unsigned int *m_pLevelDetect1;
	volatile unsigned int *m_pRisingDetect;
	volatile unsigned int *m_pFallingDetect;
	volatile unsigned int *m_pCtrl;
	volatile unsigned int *m_pOe;
	volatile unsigned int *m_pDebouncingTime;
	volatile unsigned int *m_pDebounceEnable;
	volatile unsigned int *m_pIrqStatusSet0;
	volatile unsigned int *m_pIrqWaken0;
	volatile unsigned int *m_pDataIn;
	volatile unsigned int *m_pDataOut;
	volatile unsigned int *m_pSetDataOut;
	volatile unsigned int *m_pClearDataOut;
	volatile unsigned int *m_pIrqStatusClr0;
	volatile unsigned int *m_pIrqStatus0;
	volatile unsigned int *m_pIrqStatusRaw0;

	unsigned int m_whichOne;

	Pin m_pins[32];
};

} /* namespace OMAP4460 */
#endif /* GPIO_H_ */
