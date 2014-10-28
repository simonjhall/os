/*
 * Stdio.h
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#ifndef STDIO_H_
#define STDIO_H_

#include "BaseFS.h"
//#include "print_uart.h"
//#include "PL011.h"
#include "UART.h"

class Stdio : public File
{
public:
	enum Mode
	{
		kStdin = 0,
		kStdout = 1,
		kStderr = 2,
	};
	Stdio(Mode m, GenericByteTxRx &rUart, BaseFS &fileSystem)
	: File(m == kStdin ? "stdin" : (m == kStdout ? "stdout" : "stderr"), 0, fileSystem),
	  m_rUart(rUart),
	  m_mode(m)
	{
	};

	virtual ssize_t ReadFrom(void *pBuf, size_t count, off_t offset);
	virtual ssize_t WriteTo(const void *pBuf, size_t count, off_t offset);
	virtual bool Seekable(off_t);

	virtual bool Fstat(struct stat64 &rBuf);

	//no locking
	virtual bool LockRead(void) { return true; }
	virtual bool LockWrite(void) { return true; }
	virtual void Unlock(void) { }

	virtual bool Reparent(Directory *pParent);

protected:
	GenericByteTxRx &m_rUart;
	Mode m_mode;
};

#endif /* STDIO_H_ */
