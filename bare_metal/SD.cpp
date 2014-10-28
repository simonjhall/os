/*
 * SD.cpp
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#include "SD.h"

SD::SD()
: BlockDevice(),
  m_cardState(kIdleState)
{
	// TODO Auto-generated constructor stub

}

SD::~SD()
{
	// TODO Auto-generated destructor stub
}

bool SD::Reset(void)
{
	return true;
}
