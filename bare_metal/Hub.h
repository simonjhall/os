/*
 * Hub.h
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#ifndef HUB_H_
#define HUB_H_

#include <list>
#include "Usb.h"

namespace USB
{

class Port
{
protected:
	Port();
	virtual ~Port();
public:
	virtual bool Reset(void) = 0;
	virtual bool PowerOn(bool o) = 0;
	virtual bool IsPoweredOn(void) = 0;
	virtual bool IsDeviceAttached(void) = 0;
	virtual Speed GetPortSpeed(void) = 0;
};

class Hub
{
public:

	virtual Port *GetPort(unsigned int) = 0;
	virtual unsigned int GetNumPorts(void) = 0;

protected:
	Hub();
	virtual ~Hub();
};

} /* namespace USB */
#endif /* HUB_H_ */
