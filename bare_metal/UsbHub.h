/*
 * UsbHub.h
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#ifndef USBHUB_H_
#define USBHUB_H_

#include <vector>

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

	virtual bool FullInit(void);

	virtual Port *GetPort(unsigned int);
	virtual unsigned int GetNumPorts(void);

	virtual void DumpDescriptors(Printer &p);

protected:
#pragma pack(push)
#pragma pack(1)
	struct Descriptor
	{
		unsigned char m_length;
		unsigned char m_descriptorType;
		unsigned char m_numPorts;
		unsigned char m_hubCharsLo, m_hubCharsHi;
		unsigned char m_pwrOn2PwrGood;
		unsigned char m_hubContrCurrent;
		unsigned char m_devRemovableAndPortPwrCtrlMask[64];
	};
#pragma pack(pop)

	enum Feature
	{
		kCHubLocalPower = 0,
		kCHubOverCurrent = 1,
		kPortConnection = 0,
		kPortEnable = 1,
		kPortSuspend = 2,
		kPortOverCurrent = 3,
		kPortReset = 4,
		kPortPower = 8,
		kPortLowSpeed = 9,
		kCPortConnection = 16,
		kCPortEnable = 17,
		kCPortSuspend = 18,
		kCPortOverCurrent = 19,
		kCPortReset = 20,
		kPortTest = 21,
		kPortIndicator = 22,
	};

	Descriptor m_hubDescriptor __attribute__ ((aligned (8)));

	virtual bool CanConstruct(void);
	bool GetPortStatus(unsigned short &rPortStatus, unsigned short &rPortChange, unsigned int portNo);
	bool ClearPortFeature(Feature feat, unsigned int selector, unsigned int portNo);
	bool SetPortFeature(Feature feat, unsigned int selector, unsigned int portNo);

	class UsbHubPort: public USB::Port
	{
	public:
		UsbHubPort(UsbHub &rHub, unsigned int portNo);		//zero based
		virtual ~UsbHubPort();

		virtual bool Reset(void);
		virtual bool PowerOn(bool o);
		virtual bool IsPoweredOn(void);
		virtual bool IsDeviceAttached(void);
		virtual Speed GetPortSpeed(void);

	protected:
		UsbHub &m_rHub;
		unsigned int m_portNo;
	};

	friend class UsbHubPort;

	std::vector<UsbHubPort> m_ports;
};

} /* namespace USB */
#endif /* USBHUB_H_ */
