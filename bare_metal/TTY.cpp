/*
 * TTY.cpp
 *
 *  Created on: 5 Jun 2013
 *      Author: simon
 */

#include "TTY.h"


TTY::~TTY()
{
	// TODO Auto-generated destructor stub
}

ssize_t TTY::ReadFrom(void *pBuf, size_t count, off_t offset)
{
	return m_rRead.ReadFrom(pBuf, count, offset);
}

ssize_t TTY::WriteTo(const void *pBuf, size_t count, off_t offset)
{
	return m_rWrite.WriteTo(pBuf, count, offset);
}

bool TTY::Seekable(off_t o)
{
	return m_rRead.Seekable(o) && m_rWrite.Seekable(o);
}

bool TTY::Reparent(Directory *pParent)
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
