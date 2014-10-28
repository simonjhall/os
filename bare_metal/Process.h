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
#include "WrappedFile.h"
#include "elf.h"
#include "common.h"

#include <list>
#include <elf.h>
#include <link.h>
#include <type_traits>

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

struct RfeData
{
	union {
		void(*m_pPc)(void);
		void *func;
	};
	Cpsr m_cpsr;
	unsigned int *m_pSp;
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

	//details of a data abort
	unsigned int m_dfar, m_dfsr;
};

class Nameable
{
public:
	void SetName(const char *);
	inline const char *GetName(void) { return m_name; };

	static const int sm_nameLength = 15;
	char m_name[sm_nameLength];
};

class Process;
class Scheduler;
class EventQueue;
class Semaphore;

class Thread : public Nameable
{
public:
	enum State
	{
		kRunnable,
		kRunning,
		kBlocked,
		kBlockedOnEq,
		kDead,
	};

	Thread(unsigned int entryPoint, Process *pParent, bool priv,
			unsigned int stackSizePages, int priority, unsigned int userStack = 0);

	~Thread();

	State GetState(void);
	bool SetState(State s);

	inline void *GetStackLow(void) { return m_stackLow; }
	inline void SetStackLow(void *p) { m_stackLow = p; }

	template <class T>
	inline void SetArg(int arg, const T &rValue)
	{
		static_assert(sizeof(T) <= 4, "invalid argument size");
		ASSERT(arg < 4);

		m_pausedState.m_regs[arg] = (unsigned int)rValue;
	}


	bool Unblock(void);
	inline EventQueue *GetEqBlocker(void) { return m_pBlockedOn; }
	inline void SetEqBlocker(EventQueue *p)
	{
		ASSERT(!m_pBlockedOn || !p);
		m_pBlockedOn = p;
	}

	void HaveSavedState(ExceptionState);
	bool Run(void);
	bool RunAsHandler(Thread &rBlocked);

	friend void Handler(unsigned int arg0, unsigned int arg1);
	friend int SupervisorCall(Thread &rBlocked, Process *pParent, Thread::State &rNewState);

protected:
	//process attached to
	Process *m_pParent;
	//whether it's a system thread or not
	bool m_isPriv;
	TranslationTable::SmallPageActual *m_pPrivStack;
	void *m_stackLow;									//last valid byte of stack
	//register etc state
	ExceptionState m_pausedState;
	//thread state
	State m_state;
	//thread priority
	int m_priority;

	int m_threadSwitches;
	int m_serviceSwitches;

	EventQueue *m_pBlockedOn;
};

class EventQueue
{
public:
	inline ~EventQueue(void)
	{
		ASSERT(!m_threads.size());
	}

	inline bool AddThread(Thread &rThread)
	{
		ASSERT(!Exists(rThread));

		m_threads.push_back(&rThread);
		return true;
	}

	inline void RemoveThread(Thread &rThread)
	{
		ASSERT(Exists(rThread));
		m_threads.remove(&rThread);
	}

	inline void WakeOne(void)
	{
		if (!m_threads.size())
			return;

		Thread *p = m_threads.front();

		ASSERT(p->GetEqBlocker() == this);
		ASSERT(p->GetState() == Thread::kBlockedOnEq);

		if (!p->SetState(Thread::kBlocked))
			ASSERT(!"could not use an eq to unblock a thread\n");

		m_threads.pop_front();
	}

	inline void WakeAll(void)
	{
		while (m_threads.size())
			WakeOne();
	}

	inline bool Exists(Thread &rThread)
	{
		for (auto it = m_threads.begin(); it != m_threads.end(); it++)
			if (*it == &rThread)
				return true;

		return false;
	}

protected:
	std::list<Thread *> m_threads;
};

class Semaphore : public Nameable
{
public:
	inline Semaphore(int initial)
	: Nameable(),
	  m_value(initial)
	{
		ASSERT(initial >= 0);
	}

	inline void Signal(void)
	{
		ASSERT(IsIrqEnabled());
		InvokeSyscall(0xf0000002, (unsigned int)this);
	}

	inline void SignalAtomic(void)
	{
		ASSERT(!IsIrqEnabled());
		m_value++;

		if (m_value == 0)
			m_eq.WakeOne();
	}

	inline void Wait(void)
	{
		ASSERT(IsIrqEnabled());
		InvokeSyscall(0xf0000001, (unsigned int)this);
	}

	unsigned int WaitInternal(Thread &rBlocked, Thread::State &rNewState);
	void SignalInternal();

protected:
	int m_value;
	EventQueue m_eq;
};

class Mutex : protected Semaphore
{
public:
	inline Mutex(void) : Semaphore(1) {}

	inline void Lock(void)
	{
		Wait();
	}

	inline void Unlock(void)
	{
		Signal();
	}

	using Semaphore::SetName;
	using Semaphore::GetName;
};

class Process
{
public:
	enum State
	{
		kInitialising,
		kRunnable,
		kStopped,
		kDead,
	};

	Process(const char *pRootFilename, const char *pInitialDirectory,
			const char *, BaseFS &rVfs, File &rLoader);
	~Process();

	void SetDefaultStdio(void);
	void SetStdio(File &rStdio, File &rStdout, File &rStderr);

	void AddArgument(const char *);
	void SetEnvironment(const char *);

	void MakeRunnable(void);

	void MapProcess(void);

	void Schedule(Scheduler &rSched);
	void Deschedule(Scheduler &rSched);

	void *GetBrk(void);
	void SetBrk(void *);

	void *GetHighZero(void);
	void SetHighZero(void *);

	friend int SupervisorCall(Thread &rBlocked, Process *pParent, Thread::State &rNewState);
protected:
	//list of all the attached threads in the process
	std::list<Thread *> m_threads;
	Thread *m_pMainThread;
	//program run state
	State m_state;

	//lo table
	TranslationTable::L1Table *m_pPhysTable;
	TranslationTable::L1Table *m_pVirtTable;

	//brk position
	void *m_pBrk;
	//anon mmap
	void *m_pHighZero;

	//all file handles
	ProcessFS m_pfs;
	//and loaded through this
	BaseFS &m_rVfs;

	//the file handle for the executable
	File *m_pExeFile;
	//ld-linux
	File &m_rLoader;

	//aux vector
	static const int sm_auxSize = 7;
	ElfW(auxv_t) m_auxVec[sm_auxSize];

private:
	static char *LoadElf(Elf &elf, unsigned int voffset, bool &has_tls, unsigned int &tls_memsize, unsigned int &tls_filesize, unsigned int &tls_vaddr);

	std::list<char *> m_arguments;
	char *m_pEnvironment;
	unsigned int m_entryPoint;
};

extern "C" void Resume(ExceptionState *);
extern "C" void Irq(void);


#endif /* PROCESS_H_ */
