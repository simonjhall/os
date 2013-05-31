/*
 * PL181.cpp
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#include "PL181.h"

PL181::PL181(volatile void *pBaseAddress)
: SD(),
  m_pBaseAddress ((unsigned int *)pBaseAddress)
{
}

PL181::~PL181()
{
}

void PL181::Command(SdCommand c, bool wait)
{
	unsigned int command = (unsigned int)c | ((unsigned int)wait << 6) | (1 << 10);
	m_pBaseAddress[3] = command;
}

void PL181::CommandArgument(SdCommand c, unsigned int a, bool wait)
{
	m_pBaseAddress[2] = a;

	unsigned int command = (unsigned int)c | ((unsigned int)wait << 6) | (1 << 10);
	m_pBaseAddress[3] = command;
}

unsigned int PL181::Response(unsigned int word)
{
	return m_pBaseAddress[5 + word];
}

unsigned int PL181::Status(void)
{
	return m_pBaseAddress[13];
}
