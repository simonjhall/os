/*
 * TTY.h
 *
 *  Created on: 5 Jun 2013
 *      Author: simon
 */

#ifndef TTY_H_
#define TTY_H_

#include "BaseFS.h"

class TTY: public File
{
public:
	TTY(File &rRead, File &rWrite, BaseFS &fileSystem)
	: File("tty", 0, fileSystem),
	  m_rRead(rRead),
	  m_rWrite(rWrite)
	{
	}

	virtual ~TTY();

	virtual ssize_t ReadFrom(void *pBuf, size_t count, off_t offset);
	virtual ssize_t WriteTo(const void *pBuf, size_t count, off_t offset);
	virtual bool Seekable(off_t);

	virtual bool Reparent(Directory *pParent);

protected:
	File &m_rRead;
	File &m_rWrite;
};

#endif /* TTY_H_ */
