/*
 * Process.h
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#ifndef PROCESS_H_
#define PROCESS_H_

#include "VectorTable.h"
#include "translation_table.h"
#include <list>

enum Mode
{
	kUser = 16,
	kFiq = 17,
	kIrq = 18,
	kSupervisor = 19,
	kAbort = 23,
	kUndefined = 27,
	kSystem = 31,
};

struct Cpsr
{
	unsigned int m_mode:5;
	unsigned int m_t:1;
	unsigned int m_f:1;
	unsigned int m_i:1;
	unsigned int m_a:1;
	unsigned int m_e:1;
	unsigned int m_it2:6;
	unsigned int m_ge:4;
	unsigned int m_raz:4;
	unsigned int m_j:1;
	unsigned int m_it:2;
	unsigned int m_q:1;
	unsigned int m_v:1;
	unsigned int m_c:1;
	unsigned int m_z:1;
	unsigned int m_n:1;
};

struct ExceptionState
{
	//where we were called from
	unsigned int m_returnAddress;
	//the state when we were invoked
	union {
		Cpsr m_spsr;
		unsigned int m_spsrAsWord;
	};

	//a capture of all the registers
	unsigned int m_regs[15];

	//the pc of where we should finish once the event has been processed
	unsigned int m_newPc;

	//the exception type that was invoked
	VectorTable::ExceptionType m_mode;
};

class Process;

class Thread
{
public:
	enum State
	{
		kRunnable,
		kRunning,
		kBlocked,
		kDead,
	};

	Thread(unsigned int entryPoint, Process *pParent, bool priv);

	State GetState(void);
	bool SetState(State s);

	void Unblock(void);

	void HaveSavedState(ExceptionState);
	bool Run(void);

protected:
	Process *m_pParent;
	bool m_isPriv;
	ExceptionState m_pausedState;
	State m_state;
};

class Process
{
public:
	Process(unsigned int entryPoint);
	void MapProcess(void);

	void *GetBrk(void);
	void SetBrk(void *);
protected:
	std::list<Thread> m_threads;

	TranslationTable::L1Table *pPhysTable;
	TranslationTable::L1Table *pVirtTable;

	void *m_pBrk;
};

extern "C" void Resume(ExceptionState *);
extern "C" void Irq(void);


#endif /* PROCESS_H_ */
