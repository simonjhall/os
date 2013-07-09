/*
 * Exceptions.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#include "common.h"
#include "Process.h"
#include "Scheduler.h"

extern "C" void NewHandler(ExceptionState *pState, VectorTable::ExceptionType m)
{
	PrinterUart<PL011> p;
	bool thumb = pState->m_spsr.m_t;

	Thread *pThread = Scheduler::GetMasterScheduler().WhatIsRunning();
	ASSERT(pThread);

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

			pThread->SetState(Thread::kBlocked);
			break;

		case VectorTable::kSupervisorCall:
			//the next instruction...skip the svc instruction
			pState->m_newPc = pState->m_returnAddress;

			//reset originalPc to point to the svc instruction
			if (thumb)
				pState->m_returnAddress -= 2;
			else
				pState->m_returnAddress -= 4;

			p << "system call at at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";

			pThread->SetState(Thread::kBlocked);
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

			pThread->SetState(Thread::kBlocked);
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

			pThread->SetState(Thread::kBlocked);
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
	pThread = Scheduler::GetMasterScheduler().PickNext();
	ASSERT(pThread);

	//and run it
	pThread->Run();
}



