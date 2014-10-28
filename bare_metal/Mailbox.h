/*
 * Mailbox.h
 *
 *  Created on: 25 Sep 2014
 *      Author: simon
 */

#ifndef MAILBOX_H_
#define MAILBOX_H_

#include "InterruptSource.h"
#ifdef __ARM_ARCH_7A__
#include "Process.h"
#endif

namespace OMAP4460
{

class Mailbox
{
public:
	Mailbox(volatile unsigned int *pBase, bool disableInterrupts);
	~Mailbox();

	bool IsFifoFull(unsigned int queue);
	unsigned int NumUnreadMessages(unsigned int queue);

	unsigned int ReadMessage(unsigned int queue);
	void WriteMessage(unsigned int queue, unsigned int message);

	enum InterruptOn
	{
		kNotFull = 2,
		kNewMessage = 1,
		kBoth = 3,
	};

	enum User
	{
		kCortexA9 = 0,
		kDsp = 1,
		kCortexM3 = 2,
	};

	class Queue : public InterruptSource
	{
	public:
		Queue(User u =
#ifdef __ARM_ARCH_7A__
		  kCortexA9
#endif
#ifdef __ARM_ARCH_7M__
		  kCortexM3
#endif
		  );
		virtual ~Queue(void);

		void Init(Mailbox *pParent, unsigned int q);
		bool IsFifoFull(void);
		unsigned int NumUnreadMessages(void);

		unsigned int ReadMessage(void);
		void WriteMessage(unsigned int message);

		inline void SetInterruptType(InterruptOn on) { m_intType = on; }
		inline InterruptOn GetInterruptType(void) { return m_intType; }

		virtual void EnableInterrupt(bool e);
		virtual unsigned int GetInterruptNumber(void);
		virtual bool HasInterruptFired(void);
		virtual void ClearInterrupt(void);

		virtual bool HandleInterrupt(void);
		bool HandleInterruptDisable(void);

#ifdef __ARM_ARCH_7A__
		inline Semaphore &GetSemaphore(void) { return m_sem; }
#endif

	private:
		Mailbox *m_pParent;
		unsigned int m_queue;
		InterruptOn m_intType;
		bool m_interruptEnabled;
		bool m_tempDisabledIrq;
		User m_user;

#ifdef __ARM_ARCH_7A__
		Semaphore m_sem;
#endif
	};

	Queue &GetQueue(unsigned int);

protected:
	void EnableInterrupt(bool e, InterruptOn, unsigned int q, User u);
	unsigned int GetInterruptNumber(void);
	bool HasInterruptFired(unsigned int q, User u);
	void ClearInterrupt(unsigned int q, User u);

private:
	void Reset(void);

	volatile unsigned int *m_pBase;
	Queue m_queues[8];
	bool m_disableInterrupts;
};

} /* namespace OMAP4460 */
#endif /* MAILBOX_H_ */
