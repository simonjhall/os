/*
 * InterruptController.cpp
 *
 *  Created on: 22 Jun 2013
 *      Author: simon
 */

#include "InterruptController.h"
#include "print_uart.h"

InterruptController::InterruptController(void)
{
	m_sources.clear();
}

InterruptController::~InterruptController(void)
{
}

bool InterruptController::RegisterInterrupt(InterruptSource& rSource,
		InterruptType type)
{
	//enable that interrupt #
	if (!Enable(type, rSource.GetInterruptNumber(), true))
		return false;

	//check it doesn't exist
	std::list<InterruptSource *>::iterator it;
	for (it = m_sources[rSource.GetInterruptNumber()].begin(); it != m_sources[rSource.GetInterruptNumber()].end(); it++)
		if (*it == &rSource)
			return true;

	//add it to the list
	m_sources[rSource.GetInterruptNumber()].push_back(&rSource);
	return true;
}

bool InterruptController::DeRegisterInterrupt(InterruptSource& rSource)
{
	//remove it from the list
	std::list<InterruptSource *>::iterator it;
	for (it = m_sources[rSource.GetInterruptNumber()].begin(); it != m_sources[rSource.GetInterruptNumber()].end(); it++)
		if (*it == &rSource)
		{
			m_sources[rSource.GetInterruptNumber()].erase(it);
			return true;
		}

	//if nothing in the list disable that entry
	if (m_sources[rSource.GetInterruptNumber()].size() == 0)
	{
		Enable(kIrq, rSource.GetInterruptNumber(), false);
		Enable(kFiq, rSource.GetInterruptNumber(), false);
	}

	return false;
}

InterruptSource *InterruptController::WhatHasFired(void)
{
	PrinterUart<PL011> p;

	int i = GetFiredId();

	p << "firing id is " << i << "\n";
	if (i == -1)
		return 0;

	//clear the interrupt in the controller
	Clear(i);

	//now find what caused it
	std::map<unsigned int, std::list<InterruptSource *> >::iterator mit = m_sources.find(i);

	if (mit != m_sources.end())
	{
		std::list<InterruptSource *>::iterator it;
		for (it = mit->second.begin(); it != mit->second.end(); it++)
			if ((*it)->HasInterruptFired())
			{
				(*it)->ClearInterrupt();
				return *it;
			}
	}

	//nothing found...software interrupt?
	return 0;
}

bool InterruptController::SoftwareInterrupt(unsigned int i)
{
	return false;
}
