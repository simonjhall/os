/*
 * UsbHub.h
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#ifndef USBHUB_H_
#define USBHUB_H_

#include "Usb.h"

#include "Hub.h"
#include "UsbDevice.h"

namespace USB
{

class UsbHub: public USB::Hub, public USB::UsbDevice
{
public:
	UsbHub(UsbDevice &rOther);
	virtual ~UsbHub();
};

} /* namespace USB */
#endif /* USBHUB_H_ */
