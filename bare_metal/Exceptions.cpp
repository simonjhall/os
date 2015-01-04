/*
 * Exceptions.cpp
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#ifdef __ARM_ARCH_7A__

#include "common.h"
#include "Process.h"
#include "Scheduler.h"
#include "virt_memory.h"

extern ExceptionState __UndState;
extern ExceptionState __SvcState;
extern ExceptionState __PrfState;
extern ExceptionState __DatState;
extern ExceptionState __IrqState;

extern "C" unsigned int ReadDFAR(void);
extern "C" unsigned int ReadDFSR(void);
extern "C" unsigned int ReadTPIDRURO(void);
extern "C" void WriteTPIDRURO(unsigned int);

enum DfarFaultType
{
	kAlignmentFault		= 0b00001,
	kIcacheMaintFault	= 0b00100,
	kSyncExternTTL1		= 0b01100,
	kSyncExternTTL2		= 0b01110,
	kSynParityTTL1		= 0b11100,
	kSynParityTTL2		= 0b11110,
	kTransFaultL1		= 0b00101,
	kTransFaultL2		= 0b00111,
	kAccessFaultL1		= 0b00011,
	kAccessFaultL2		= 0b00110,
	kDomFaultL1			= 0b01001,
	kDomFaultL2			= 0b01011,
	kPermFaultL1		= 0b01101,
	kPermFaultL2		= 0b01111,
	kDebugEvent			= 0b00010,
	kSyncExtAbort		= 0b01000,
	kTlbConfAbort		= 0b10000,
	kSyncParityMem		= 0b11001,
	kAsyncExtAbort		= 0b10110,
	kAsyncParMem		= 0b11000,
};

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

//bool g_inHandler = false;

extern "C" void NewHandler(ExceptionState *pState, VectorTable::ExceptionType m)
{
	static_assert(sizeof(ExceptionState) == 86 * 4, "wrong exception structure size");
//	ASSERT(!g_inHandler);
//	g_inHandler = true;

	Printer &p = Printer::Get();
	bool thumb = pState->m_spsr.m_t;

	unsigned int cpuId = GetCpuId();

//	DumpAllStates(p);

	Thread *pThread = Scheduler::GetMasterScheduler().WhatIsRunning(cpuId);
	ASSERT(pState->m_spsr.m_mode == kSystem || pState->m_spsr.m_mode == kUser);

	bool uninterruptable = Scheduler::GetMasterScheduler().IsUninterruptableRunning(cpuId);

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
			else if (pState->m_regs[7] == 0xf0000002)	//semaphore signal
			{
				pThread->SetState(Thread::kRunnable);
				Semaphore *pSem = (Semaphore *)pState->m_regs[0];
				pSem->SignalInternal();
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
		{
			//reset originalPc to point to the faulting instruction
			pState->m_returnAddress -= 8;

			//return to the failed instruction
			pState->m_newPc = pState->m_returnAddress;

			unsigned int dfar = ReadDFAR();
			unsigned int dfsr = ReadDFSR();

			DfarFaultType faultType = (DfarFaultType)((dfsr & 0xf) | (((dfsr >> 10) & 1) << 4));

			p << "data abort at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
			p << "cpsr " << pState->m_spsrAsWord << "\n";
			for (int count = 0; count < 15; count++)
				p << "\t" << count << " " << pState->m_regs[count] << "\n";
			p << "dfsr " << dfsr << " dfar " << dfar << "\n";
			p << "dfsr: ext " << ((dfsr & (1 << 12)) ? "1" : "0")
					<< " wnr " << ((dfsr & (1 << 11)) ? "W" : "R")
					<< " fs " << faultType
					<< " dom " << ((dfsr >> 4) & 0xf) << "\n";

			pState->m_dfar = dfar;
			pState->m_dfsr = dfsr;

			ASSERT(!uninterruptable);
			ASSERT(pThread);

			pThread->SetState(Thread::kBlocked);
			Scheduler::GetMasterScheduler().OnThreadBlock(*pThread);
			break;
		}

		case VectorTable::kIrq:
			//reset originalPc to point to the point of return
			pState->m_returnAddress -= 4;

			//return to the unfinished instruction
			pState->m_newPc = pState->m_returnAddress;

			if (0)
			{
				p << "irq at " << pState->m_returnAddress << " returning to " << pState->m_newPc << "\n";
				p << "cpsr " << pState->m_spsrAsWord << "\n";
				for (int count = 0; count < 15; count++)
					p << "\t" << count << " " << pState->m_regs[count] << "\n";
			}

			//run all the interrupt handlers, and clear recorded interrupts
			//we can't enqueue them all in a list and run them elsewhere as interrupts will be turned on
			//and we'll come right back here
			Irq();

			if (uninterruptable || !pThread)
			{
//				g_inHandler = false;
				Resume(pState);
			}

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
	pThread = Scheduler::GetMasterScheduler().PickNext(&pBlocked, cpuId);
	ASSERT(pThread);

	//crap test to check that we're not in the exception handerl
//	g_inHandler = false;

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
//	Printer &p = Printer::Get();

	Thread *pBlocked = (Thread *)arg1;

	while (1)
	{
		ASSERT(pBlocked);
//		p << "in handler for thread " << pBlocked << "\n";

		Thread::State newState = Thread::kRunnable;

		switch (pBlocked->m_pausedState.m_mode)
		{
		case VectorTable::kSupervisorCall:
			pBlocked->m_pausedState.m_regs[0] = SupervisorCall(*pBlocked, pBlocked->m_pParent, newState);

			break;
		case VectorTable::kDataAbort:
		{
			DfarFaultType faultType = (DfarFaultType)((pBlocked->m_pausedState.m_dfsr & 0xf)
					| (((pBlocked->m_pausedState.m_dfsr >> 10) & 1) << 4));

			switch (faultType)
			{
			case kTransFaultL1:
			case kTransFaultL2:
			{
				unsigned int max_growth = 20 * 4096;

				//do we just need to grow the stack down a few pages?
				if (pBlocked->m_pausedState.m_dfar >= (unsigned int)pBlocked->GetStackLow() - max_growth
						&& pBlocked->m_pausedState.m_dfar < (unsigned int)pBlocked->GetStackLow())
				{
					//compute the amount of pages we need
					unsigned int bytes = (((unsigned int)pBlocked->GetStackLow() - pBlocked->m_pausedState.m_dfar) + 4095) & ~4095;
					unsigned int pages = bytes >> 12;

					if (!VirtMem::AllocAndMapVirtContig((void *)((unsigned int)pBlocked->GetStackLow() - bytes), pages, false,
							TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, TranslationTable::kShareable, 0))
						ASSERT(0);
					pBlocked->SetStackLow((void *)((unsigned int)pBlocked->GetStackLow() - bytes));

					break;
				}
			}
			default:
				//completely invalid, the process needs to go
				newState = Thread::kDead;
				break;
			}
			break;
		}
		default:
			ASSERT(0);
			/* no break */
		}

		switch (newState)
		{
			case Thread::kRunnable:
				if (pBlocked->Unblock())	//may be dead
					Scheduler::GetMasterScheduler().OnThreadUnblock(*pBlocked);
				else
					ASSERT(0);
				break;

			case Thread::kBlockedOnEq:
				pBlocked->SetState(Thread::kBlockedOnEq);
				break;

			default:
				ASSERT(0);
				/* no break */
		};

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

#endif
