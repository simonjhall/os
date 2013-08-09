/*
 * UsbDevice.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef USBDEVICE_H_
#define USBDEVICE_H_

#include "Usb.h"
#include <list>

namespace USB
{

class UsbDevice
{
public:
	UsbDevice(Hcd &rHostController);
	UsbDevice(UsbDevice &rOther);
	virtual ~UsbDevice();

	virtual bool Valid(void);
	virtual bool BasicInit(int address);
	virtual bool Identify(unsigned short &rVendor, unsigned short &rProduct, unsigned short &rVersion);

protected:
	virtual bool Cloned(void);
	virtual bool SetAddress(int address);
	virtual bool ReadDescriptors(void);
	virtual bool SetConfiguration(unsigned int c);

	DeviceDescriptor m_deviceDescriptor;
	std::list<Configuration> m_configurations;

	Configuration *m_pCurrentConfiguration;
	int m_address;
	Hcd &m_rHostController;
};

} /* namespace USB */
#endif /* USBDEVICE_H_ */
