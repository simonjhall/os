/*
 * Exceptions.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "common.h"
#include "Process.h"
#include "Scheduler.h"

extern ExceptionState __UndState;
extern ExceptionState __SvcState;
extern ExceptionState __PrfState;
extern ExceptionState __DatState;
extern ExceptionState __IrqState;

void DumpAllStates(Printer &p)
{
	p << "undefined\n";
	p << __UndState.m_returnAddress << " " << __UndState.m_spsrAsWord << "\n";
	for (int count = 0; count < 15; count++)
		p << __UndState.m_regs[count] << "\n";
	p << __UndState.m_newPc << "\n";
	p << (int)__UndState.m_mode << "\n";

	p << "supervisor call\n";
	p << __SvcState.m_returnAddress << " " << __SvcState.m_spsrAsWord << "\n";
	for (int count = 0; count < 15; count++)
		p << __SvcState.m_regs[count] << "\n";
	p << __SvcState.m_newPc << "\n";
	p << (int)__SvcState.m_mode << "\n";

	p << "prefetch abort\n";
	p << __PrfState.m_returnAddress << " " << __PrfState.m_spsrAsWord << "\n";
	for (int count = 0; count < 15; count++)
		p << __PrfState.m_regs[count] << "\n";
	p << __PrfState.m_newPc << "\n";
	p << (int)__PrfState.m_mode << "\n";

	p << "data abort\n";
	p << __DatState.m_returnAddress << " " << __DatState.m_spsrAsWord << "\n";
	for (int count = 0; count < 15; count++)
		p << __DatState.m_regs[count] << "\n";
	p << __DatState.m_newPc << "\n";
	p << (int)__DatState.m_mode << "\n";

	p << "irq\n";
	p << __IrqState.m_returnAddress << " " << __IrqState.m_spsrAsWord << "\n";
	for (int count = 0; count < 15; count++)
		p << __IrqState.m_regs[count] << "\n";
	p << __IrqState.m_newPc << "\n";
	p << (int)__IrqState.m_mode << "\n";
}

extern "C" void NewHandler(ExceptionState *pState, VectorTable::ExceptionType m)
{
	PrinterUart<PL011> p;
	bool thumb = pState->m_spsr.m_t;

//	DumpAllStates(p);

	Thread *pThread = Scheduler::GetMasterScheduler().WhatIsRunning();

	bool uninterruptable = Scheduler::GetMasterScheduler().IsUninterruptableRunning();

	switch (m)
	{
		default:
		case VectorTable::kReset:
			p << "entered handler with unknown or reset state - " << (unsigned int)m << "\n";
			p << "pc? " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";

			ASSERT(0);
			pThread->SetState(Thread::kDead);
			break;

		case VectorTable::kUndefinedInstruction:
			//reset originalPc to point to the faulting instruction
			if (thumb)
				pState->m_returnAddress -= 2;
			else
				pState->m_returnAddress -= 4;

			//return to the failed instruction
			pState->m_newPc = pState->m_returnAddress;

			p << "undefined instruction at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";

			ASSERT(!uninterruptable);

			ASSERT(pThread);
			pThread->SetState(Thread::kBlocked);
			Scheduler::GetMasterScheduler().OnThreadBlock(*pThread);
			break;

		case VectorTable::kSupervisorCall:
			//the next instruction...skip the svc instruction
			pState->m_newPc = pState->m_returnAddress;

			//reset originalPc to point to the svc instruction
			if (thumb)
				pState->m_returnAddress -= 2;
			else
				pState->m_returnAddress -= 4;

//			p << "system call at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
//			p << "call is "; p.PrintDec(pState->m_regs[7], false); p << "\n";

			ASSERT(pThread);

			if (pState->m_regs[7] == 158)	//yield
			{
				pThread->SetState(Thread::kRunnable);
				pState->m_regs[0] = 0;			//no error
			}
			else if (pState->m_regs[7] == 1)	//exit
			{
				pThread->SetState(Thread::kRunnable);
				pState->m_regs[0] = 0;			//no error
			}
			else
			{
				ASSERT(!uninterruptable);
				pThread->SetState(Thread::kBlocked);
				Scheduler::GetMasterScheduler().OnThreadBlock(*pThread);
			}

			break;

		case VectorTable::kPrefetchAbort:
			//reset originalPc to point to the faulting instruction
			pState->m_returnAddress -= 4;

			//return to the failed instruction
			pState->m_newPc = pState->m_returnAddress;

			p << "prefetch abort at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";

			ASSERT(!uninterruptable);
			ASSERT(pThread);

			pThread->SetState(Thread::kBlocked);
			Scheduler::GetMasterScheduler().OnThreadBlock(*pThread);
			break;

		case VectorTable::kDataAbort:
			//reset originalPc to point to the faulting instruction
			pState->m_returnAddress -= 8;

			//return to the failed instruction
			pState->m_newPc = pState->m_returnAddress;

			p << "data abort at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";

			ASSERT(!uninterruptable);
			ASSERT(pThread);

			pThread->SetState(Thread::kBlocked);
			Scheduler::GetMasterScheduler().OnThreadBlock(*pThread);
			break;

		case VectorTable::kIrq:
			//reset originalPc to point to the point of return
			pState->m_returnAddress -= 4;

			//return to the unfinished instruction
			pState->m_newPc = pState->m_returnAddress;

			p << "irq at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";

			//run all the interrupt handlers, and clear recorded interrupts
			//we can't enqueue them all in a list and run them elsewhere as interrupts will be turned on
			//and we'll come right back here
			Irq();

			if (uninterruptable || !pThread)
				Resume(pState);

			ASSERT(pThread);

			pThread->SetState(Thread::kRunnable);
			break;
	}

	//should have been changed by now
	ASSERT(pThread->GetState() != Thread::kRunning);

	//everything else goes to the system handler
	pState->m_mode = m;

	//store the corrected state
	pThread->HaveSavedState(*pState);

	//find what we're to run next
	Thread *pBlocked;
	pThread = Scheduler::GetMasterScheduler().PickNext(&pBlocked);
	ASSERT(pThread);

	//and run it
	if (!pBlocked)
		pThread->Run();
	else
		pThread->RunAsHandler(*pBlocked);

	//shouldn't reach here
	ASSERT(0);
}

extern "C" Thread *HandlerYield(void);

void Handler(unsigned int arg0, unsigned int arg1)
{
	PrinterUart<PL011> p;

	Thread *pBlocked = (Thread *)arg1;

	while (1)
	{
		ASSERT(pBlocked);
//		p << "in handler for thread " << pBlocked << "\n";

		switch (pBlocked->m_pausedState.m_mode)
		{
		case VectorTable::kSupervisorCall:
			pBlocked->m_pausedState.m_regs[0] = SupervisorCall(*pBlocked, pBlocked->m_pParent);
			break;
		default:
			ASSERT(0);
		}

		if (pBlocked->Unblock())	//may be dead
			Scheduler::GetMasterScheduler().OnThreadUnblock(*pBlocked);

		pBlocked = HandlerYield();
	}
}

void IdleThread(void)
{
	while (1)
	{
		asm volatile ("wfe");
	}
}

