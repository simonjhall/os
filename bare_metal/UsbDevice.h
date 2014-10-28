/*
 * UsbDevice.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef USBDEVICE_H_
#define USBDEVICE_H_

#include "Usb.h"
#include "EndPoint.h"
#include "print.h"

#include <list>

namespace USB
{

class UsbDevice
{
public:
	UsbDevice(Hcd &rHostController, Port &rPort);
	virtual ~UsbDevice();
	virtual bool Cloned(void);

	virtual bool Valid(void);
	virtual bool BasicInit(int address);
	virtual bool FullInit(void);

	virtual bool ProductIdentify(unsigned short &rVendor, unsigned short &rProduct, unsigned short &rVersion);
	virtual Speed GetSpeed(void);
	virtual int GetAddress(void);

	virtual void DumpDescriptors(Printer &p);

	UsbDevice(UsbDevice &rOther);
protected:

	virtual bool SetAddress(int address);
	virtual bool ReadDescriptors(void);
	virtual bool SetConfiguration(unsigned int c);
	virtual bool CanConstruct(void);

	EndPoint m_endPointZero;
	DeviceDescriptor m_deviceDescriptor __attribute__ ((aligned (8)));
	std::list<Configuration> m_configurations;

	Configuration *m_pCurrentConfiguration;

	int m_address;
	Hcd &m_rHostController;
	Port &m_rPort;
	Speed m_speed;


	//request type
	static const unsigned int sm_reqTypeHostToDevice = 0 << 7;
	static const unsigned int sm_reqTypeDeviceToHost = 1 << 7;

	static const unsigned int sm_reqTypeStandard = 0 << 5;
	static const unsigned int sm_reqTypeClass = 1 << 5;
	static const unsigned int sm_reqTypeVendor = 2 << 5;

	static const unsigned int sm_reqTypeDevice = 0;
	static const unsigned int sm_reqTypeInterface = 1;
	static const unsigned int sm_reqTypeEndpoint = 2;
	static const unsigned int sm_reqTypeOther = 3;

	//request
	static const unsigned int sm_reqClearFeature = 1;
	static const unsigned int sm_reqGetConfiguration = 8;
	static const unsigned int sm_reqGetDescriptor = 6;
	static const unsigned int sm_reqGetInterface = 10;
	static const unsigned int sm_reqGetStatus = 0;
	static const unsigned int sm_reqSetAddress = 5;
	static const unsigned int sm_reqSetConfiguration = 9;
	static const unsigned int sm_reqSetDescriptor = 7;
	static const unsigned int sm_reqSetFeature = 3;
	static const unsigned int sm_reqSetInterface = 11;
	static const unsigned int sm_reqSynchFrame = 12;
};

} /* namespace USB */
#endif /* USBDEVICE_H_ */
