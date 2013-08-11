/*
 * Interface.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "Usb.h"
#include <list>

namespace USB
{

struct Interface
{
	InterfaceDescriptor m_descriptor __attribute__ ((aligned (8)));
	std::list<EndPoint> m_endPoints;
};

} /* namespace USB */
#endif /* INTERFACE_H_ */
