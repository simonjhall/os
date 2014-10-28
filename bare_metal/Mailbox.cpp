/*
 * Mailbox.cpp
 *
 *  Created on: 25 Sep 2014
 *      Author: simon
 */

#include "common.h"
#include "Mailbox.h"

namespace OMAP4460
{

Mailbox::Mailbox(volatile unsigned int *pBase, bool disableInterrupts)
: m_pBase(pBase),
  m_disableInterrupts(disableInterrupts)
{
	for (unsigned int count = 0; count < 8; count++)
		m_queues[count].Init(this, count);

	//disable all interrupts
	if (m_disableInterrupts)
		for (int queue = 0; queue < 8; queue++)
			for (int user = 0; user < 3; user++)
				EnableInterrupt(false, kBoth, queue, (User)user);
}

Mailbox::~Mailbox()
{
	//disable all interrupts
	if (m_disableInterrupts)
		for (int queue = 0; queue < 8; queue++)
			for (int user = 0; user < 3; user++)
				EnableInterrupt(false, kBoth, queue, (User)user);
}

void Mailbox::EnableInterrupt(bool e, InterruptOn on, unsigned int q, User u)
{
	//disable first, note no read-modify-write as writing a 1 will disable
	m_pBase[(0x10c + (16 * u)) >> 2] = (kBoth << (q * 2));

	//and then enable
	if (e)
		m_pBase[(0x108 + (16 * u)) >> 2] |= (on << (q * 2));
}

unsigned int Mailbox::GetInterruptNumber(void)
{
#ifdef __ARM_ARCH_7A__
	return 26 + 32;
#endif
#ifdef __ARM_ARCH_7M__
	return 34;
#endif
}

bool Mailbox::HasInterruptFired(unsigned int q, User u)
{
	if (m_pBase[(0x104 + (16 * u)) >> 2] & (kBoth << (q * 2)))
		return true;
	else
		return false;
}

void Mailbox::ClearInterrupt(unsigned int q, User u)
{
	//nb no read-modify-write as that will clear another interrupt
	m_pBase[(0x104 + (16 * u)) >> 2] = (kBoth << (q * 2));
}

bool Mailbox::IsFifoFull(unsigned int queue)
{
	ASSERT(queue >= 0 && queue < 8);

	if (m_pBase[(0x80 + (4 * queue)) >> 2] & 1)
		return true;
	else
		return false;
}

unsigned int Mailbox::NumUnreadMessages(unsigned int queue)
{
	ASSERT(queue >= 0 && queue < 8);
	return m_pBase[(0xc0 + (4 * queue)) >> 2] & 7;
}

unsigned int Mailbox::ReadMessage(unsigned int queue)
{
	ASSERT(NumUnreadMessages(queue));
	ASSERT(queue >= 0 && queue < 8);
	return m_pBase[(0x40 + (4 * queue)) >> 2];
}

void Mailbox::WriteMessage(unsigned int queue, unsigned int message)
{
	while (IsFifoFull(queue));

	ASSERT(queue >= 0 && queue < 8);
	m_pBase[(0x40 + (4 * queue)) >> 2] = message;

	asm("dsb");
}

Mailbox::Queue& Mailbox::GetQueue(unsigned int q)
{
	ASSERT(q >= 0 && q < 8);
	return m_queues[q];
}

void Mailbox::Reset(void)
{
	m_pBase[0x10 >> 2] |= 1;

	//wait for the reset to complete
	while(m_pBase[0x10 >> 2] & 1);
}

Mailbox::Queue::Queue(User u)
: InterruptSource(),
  m_pParent(0),
  m_queue(0),
  m_intType(kBoth),
  m_interruptEnabled(false),
  m_tempDisabledIrq(false),
  m_user(u)
#ifdef __ARM_ARCH_7A__
, m_sem(0)
#endif
{
}

Mailbox::Queue::~Queue(void)
{
}

void Mailbox::Queue::Init(Mailbox* pParent, unsigned int q)
{
	m_pParent = pParent;
	m_queue = q;
}

bool Mailbox::Queue::IsFifoFull(void)
{
	ASSERT(m_pParent);
	return m_pParent->IsFifoFull(m_queue);
}

unsigned int Mailbox::Queue::NumUnreadMessages(void)
{
	ASSERT(m_pParent);
	return m_pParent->NumUnreadMessages(m_queue);
}

unsigned int Mailbox::Queue::ReadMessage(void)
{
	ASSERT(m_pParent);
	auto m = m_pParent->ReadMessage(m_queue);

	if (m_tempDisabledIrq)
	{
		m_tempDisabledIrq = false;
		EnableInterrupt(true);
	}

	return m;
}

void Mailbox::Queue::WriteMessage(unsigned int message)
{
	ASSERT(m_pParent);
	m_pParent->WriteMessage(m_queue, message);

	if (m_tempDisabledIrq)
	{
		m_tempDisabledIrq = false;
		EnableInterrupt(true);
	}
}

void Mailbox::Queue::EnableInterrupt(bool e)
{
	ASSERT(m_pParent);
	return m_pParent->EnableInterrupt(e, m_intType, m_queue, m_user);
}

unsigned int Mailbox::Queue::GetInterruptNumber(void)
{
	ASSERT(m_pParent);
	return m_pParent->GetInterruptNumber();
}

bool Mailbox::Queue::HasInterruptFired(void)
{
	ASSERT(m_pParent);
	return m_pParent->HasInterruptFired(m_queue, m_user);
}

void Mailbox::Queue::ClearInterrupt(void)
{
	ASSERT(m_pParent);
	return m_pParent->ClearInterrupt(m_queue, m_user);
}

bool Mailbox::Queue::HandleInterrupt(void)
{
	if (InterruptSource::HandleInterrupt())
		return true;

	HandleInterruptDisable();
	return true;
}

bool Mailbox::Queue::HandleInterruptDisable(void)
{
	if (m_intType == kNewMessage || m_intType == kBoth)
		if (NumUnreadMessages() == 0)
			return false;				//it's from before

	if (m_intType == kNotFull || m_intType == kBoth)
		if (NumUnreadMessages() == 4)
			return false;

	ASSERT(!m_tempDisabledIrq);

	EnableInterrupt(false);
	m_tempDisabledIrq = true;

	ClearInterrupt();
	return true;
}

} /* namespace OMAP4460 */
