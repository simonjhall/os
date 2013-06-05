/*
 * Stdio.h
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#ifndef STDIO_H_
#define STDIO_H_

#include "BaseFS.h"
#include "print_uart.h"

class Stdio //: public BaseFS
{
public:
	class Stdin : public File
	{
	public:
		Stdin(PL011 &rUart, BaseFS &fileSystem)
		: File("stdin", 0, fileSystem),
		  m_rUart(rUart)
		{
		};

		virtual ssize_t ReadFrom(void *pBuf, size_t count, off_t offset);
		virtual ssize_t WriteTo(const void *pBuf, size_t count, off_t offset);
		virtual bool Seekable(off_t);

	protected:
		PL011 &m_rUart;
	};

	Stdio();
	virtual ~Stdio();
};

#endif /* STDIO_H_ */
