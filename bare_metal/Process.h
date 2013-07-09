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

	Thread(unsigned int entryPoint, Process *pParent, bool priv,
			unsigned int stackSizePages, int priority);

	State GetState(void);
	bool SetState(State s);

	void Unblock(void);

	void HaveSavedState(ExceptionState);
	bool Run(void);
	bool RunAsHandler(Thread &rBlocked);

protected:
	//process attached to
	Process *m_pParent;
	//whether it's a system thread or not
	bool m_isPriv;
	//register etc state
	ExceptionState m_pausedState;
	//thread state
	State m_state;
	//thread priority
	int m_priority;
};

class Process
{
public:
	enum State
	{
		kRunnable,
		kStopped,
		kDead,
	};

	Process(ProcessFS &, const char *, BaseFS &rVfs, File &rLoader);
	void MapProcess(void);

	void *GetBrk(void);
	void SetBrk(void *);
protected:
	//list of all the attached threads in the process
	std::list<Thread> m_threads;
	//program run state
	State m_state;

	//lo table
	TranslationTable::L1Table *m_pPhysTable;
	TranslationTable::L1Table *m_pVirtTable;

	//brk position
	void *m_pBrk;

	//all file handles
	ProcessFS &m_rPfs;
	//and loaded through this
	BaseFS &m_rVfs;

	//the file handle for the executable
	File *m_pExeFile;
	//ld-linux
	File &m_rLoader;

	//file handles
	int m_exeFd, m_ldFd;

private:
	static char *LoadElf(Elf &elf, unsigned int voffset, bool &has_tls, unsigned int &tls_memsize, unsigned int &tls_filesize, unsigned int &tls_vaddr);
};

extern "C" void Resume(ExceptionState *);
extern "C" void Irq(void);


#endif /* PROCESS_H_ */
