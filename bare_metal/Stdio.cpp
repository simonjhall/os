/*
 * Stdio.cpp
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#include "Stdio.h"
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

ssize_t Stdio::ReadFrom(void *pBuf, size_t count, off_t offset)
{
	if (m_mode != kStdin)
		return -1;

//	size_t original_count = count;

	unsigned char *pOut = (unsigned char *)pBuf;
	unsigned int read_bytes = 0;
	bool seen_newline = false;

	while(count && seen_newline == false)
	{
		unsigned char c;
		while (!m_rUart.ReadByte(c));
		{
			if (c == '\r')
			{
				c = '\n';
				seen_newline = true;
			}

			*pOut++ = c;
			count--;
			read_bytes++;
		}
//		else
//			break;
	}
//	if (original_count == 0)
//		return read_bytes;
//	else if (read_bytes == 0)
//		return -EAGAIN;
//	else
		return read_bytes;
}

ssize_t Stdio::WriteTo(const void *pBuf, size_t count, off_t offset)
{
	if (m_mode == kStdin)
		return -1;

	unsigned char *pIn = (unsigned char *)pBuf;
	unsigned int written_bytes = 0;

	while(count)
	{
		unsigned char c = *pIn++;
		//potentially do the \r thing here
		if (m_rUart.WriteByte(c))
		{
			count--;
			written_bytes++;
		}
		else
			break;
	}
	return written_bytes;
}

bool Stdio::Seekable(off_t)
{
	return false;
}

bool Stdio::Reparent(Directory *pParent)
{
	if (pParent)
	{
		m_pParent = pParent;
		m_orphan = false;
	}
	else
	{
		m_pParent = 0;
		m_orphan = true;
	}
	return true;
}

bool Stdio::Fstat(struct stat64 &rBuf)
{
	memset(&rBuf, 0, sizeof(rBuf));
	rBuf.st_dev = (dev_t)&m_rFileSystem;
	rBuf.st_ino = (ino_t)this;
	rBuf.st_size = 0;
	rBuf.st_mode = S_IFCHR;
	rBuf.st_rdev = (dev_t)1;
	rBuf.st_blksize = 1;
	rBuf.st_blocks = 0;
	return true;
}
