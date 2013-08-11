/*
 * Hcd.cpp
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#include "Hcd.h"
#include "UsbDevice.h"

#include "common.h"

namespace USB
{

Hcd::Hcd()
{
	for (int count = 0; count < 128; count++)
		m_used[count] = false;

	m_used[0] = true;
}

Hcd::~Hcd()
{
}

int Hcd::AllocateBusAddress(void)
{
	for (int count = 0; count < 128; count++)
		if (m_used[count] == false)
		{
			m_used[count] = true;
			return count;
		}

	return -1;
}

void Hcd::ReleaseBusAddress(int addr)
{
	ASSERT(m_used[addr]);
	m_used[addr] = false;
}

} /* namespace USB */
