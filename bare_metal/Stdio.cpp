/*
 * Stdio.cpp
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#include "Stdio.h"

ssize_t Stdio::ReadFrom(void *pBuf, size_t count, off_t offset)
{
	if (m_mode != kStdin)
		return -1;

	unsigned char *pOut = (unsigned char *)pBuf;

	while(count)
	{
		unsigned char c;
		while (m_rUart.ReadByte(c) == false);

		*pOut++ = c;
		count--;
	}
	return count;
}

ssize_t Stdio::WriteTo(const void *pBuf, size_t count, off_t offset)
{
	if (m_mode == kStdin)
		return -1;

	unsigned char *pIn = (unsigned char *)pBuf;

	while(count)
	{
		unsigned char c = *pIn++;
		//potentially do the \r thing here
		while (m_rUart.WriteByte(c) == false);

		count--;
	}
	return count;
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
