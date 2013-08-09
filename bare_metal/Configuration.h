/*
 * Configuration.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "Usb.h"
#include <list>

namespace USB
{

struct Configuration
{
	ConfigurationDescriptor m_descriptor;
	std::list<Interface> m_interfaces;
};

} /* namespace USB */
#endif /* CONFIGURATION_H_ */
