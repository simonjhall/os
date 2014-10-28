/*
 * UsbHub.cpp
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#include <string.h>

#include "common.h"
#include "print_uart.h"
#include "virt_memory.h"

#include "UsbHub.h"
#include "Configuration.h"
#include "Interface.h"
#include "Hcd.h"

namespace USB
{

UsbHub::UsbHub(UsbDevice &rOther)
: Hub(),
  UsbDevice(rOther)
{
	Printer &p = Printer::Get();
	memset(&m_hubDescriptor, 0, sizeof(m_hubDescriptor));

	if (!Valid())
		return;			//leaving the original object intact

	if (!CanConstruct())
	{
		p << "can't construct as a hub\n";
		m_address = -1;
		return;
	}
	else
		p << "could be a hub\n";

	if (!rOther.Cloned())
		ASSERT(0);
}

UsbHub::~UsbHub()
{
}

Port* UsbHub::GetPort(unsigned int p)
{
	if (p < GetNumPorts())
		return &m_ports[p];
	else
		return 0;
}

unsigned int UsbHub::GetNumPorts(void)
{
	return m_hubDescriptor.m_numPorts;
}

bool UsbHub::FullInit(void)
{
	Printer &p = Printer::Get();
	p << "hub full init\n";

	p << "selecting default configuration, from " << m_configurations.size() << "...";
	if (!SetConfiguration(0))
	{
		p << "failed to choose default configuration\n";
		return false;
	}
	p << "ok\n";

	char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	Descriptor *dd = (Descriptor *)buffer;

	//get the device descriptor
	SetupPacket packet(sm_reqTypeDeviceToHost | sm_reqTypeClass,
			sm_reqGetDescriptor, 0x29 << 8, 0, 64);

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, buffer, 64, packet))
	{
		p << "failed to get hub class descriptor\n";
		return false;
	}

	if (dd->m_descriptorType != 0x29)
	{
		p << "class descriptor type value not expected\n";
		return false;
	}

	p << "enumerating ports...";

	for (unsigned char count = 0; count < dd->m_numPorts; count++)
	{
//		p << count << "...";
		UsbHubPort *port = new UsbHubPort(*this, count);
//		p << port << "...";
		m_ports.push_back(*port);
	}

	p << "ok\n";

	memcpy(&m_hubDescriptor, dd, sizeof(m_hubDescriptor));

	return true;
}

void UsbHub::DumpDescriptors(Printer& p)
{
	if (!Valid())
	{
		p << "\tdevice not valid at this stage\n";
		return;
	}

	UsbDevice::DumpDescriptors(p);

	p << "hub class descriptor dump for " << this << "\n";
	p << "\tnum ports " << m_hubDescriptor.m_numPorts << "\n";
	p << "\tcharacteristics " << m_hubDescriptor.m_hubCharsLo << " " << m_hubDescriptor.m_hubCharsHi << "\n";
	p << "\tpower on to power good " << m_hubDescriptor.m_pwrOn2PwrGood * 2 << " ms\n";
	p << "\thub controller current " << m_hubDescriptor.m_hubContrCurrent << " mA\n";

//	for (int count = 0; count < 64; count++)
//		p << count << ": " << m_hubDescriptor.m_devRemovableAndPortPwrCtrlMask[count] << "\n";
}

bool UsbHub::CanConstruct(void)
{
	if (m_deviceDescriptor.m_deviceClass != (unsigned char)kClassHub)
		return false;

	if (m_deviceDescriptor.m_deviceSubClass != 0)
		return false;

	if (m_deviceDescriptor.m_deviceProtocol > 2)
		return false;

	if (m_deviceDescriptor.m_maxPacketSize0 != 64)
		return false;

	if (m_deviceDescriptor.m_numConfigurations != 1)
		return false;

	if (m_configurations.front().m_descriptor.m_numInterfaces != 1)
		return false;

	if (m_configurations.front().m_interfaces.front().m_descriptor.m_numEndpoints != 1)
		return false;

	if (m_configurations.front().m_interfaces.front().m_descriptor.m_interfaceClass != (unsigned char)kClassHub)
		return false;

	//in
	if ((m_configurations.front().m_interfaces.front().m_endPoints.front().m_descriptor.m_endpointAddress & 0x80) == 0)
		return false;

	if (m_configurations.front().m_interfaces.front().m_endPoints.front().m_descriptor.m_attributes != EndPoint::kInterrupt)
		return false;

	return true;
}

bool UsbHub::GetPortStatus(unsigned short &rPortStatus, unsigned short &rPortChange, unsigned int portNo)
{
	Printer &p = Printer::Get();
	p << "get port status, port " << portNo << "\n";

	unsigned short buffer[32];
	memset(buffer, 0, 64);

	SetupPacket packet(sm_reqTypeDeviceToHost | sm_reqTypeClass | sm_reqTypeOther,
			sm_reqGetStatus, 0, portNo, 4);
	/*p << "request type " << (sm_reqTypeHostToDevice | sm_reqTypeClass | sm_reqTypeOther)
			<< " request " << sm_reqGetStatus
			<< " index " << portNo << "\n";*/

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, buffer, 64, packet))
	{
		p << "failed to get usb hub port status\n";
		return false;
	}

	rPortStatus = buffer[0];
	rPortChange = buffer[1];

	p << "port status " << rPortStatus << " port change " << rPortChange << "\n";

	return true;
}

bool UsbHub::SetPortFeature(Feature feat, unsigned int selector,
		unsigned int portNo)
{
	Printer &p = Printer::Get();
	p << "set feature " << feat << " port " << portNo << "\n";

	SetupPacket packet(sm_reqTypeHostToDevice | sm_reqTypeClass | sm_reqTypeOther,
			sm_reqSetFeature, feat, (selector << 8) | (portNo & 0xff), 0);
	/*p << "request type " << (sm_reqTypeHostToDevice | sm_reqTypeClass | sm_reqTypeOther)
			<< " request " << sm_reqSetFeature << " value " << feat
			<< " index " << ((selector << 8) | (portNo & 0xff)) << "\n";*/

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, 0, 0, packet))
	{
		p << "failed to set usb hub port feature\n";
		return false;
	}

	return true;
}

bool UsbHub::ClearPortFeature(Feature feat, unsigned int selector,
		unsigned int portNo)
{
	Printer &p = Printer::Get();
	p << "clear feature " << feat << " port " << portNo << "\n";

	SetupPacket packet(sm_reqTypeHostToDevice | sm_reqTypeClass | sm_reqTypeOther,
			sm_reqClearFeature, feat, (selector << 8) | (portNo & 0xff), 0);
	/*p << "request type " << (sm_reqTypeHostToDevice | sm_reqTypeClass | sm_reqTypeOther)
			<< " request " << sm_reqSetFeature << " value " << feat
			<< " index " << ((selector << 8) | (portNo & 0xff)) << "\n";*/

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, 0, 0, packet))
	{
		p << "failed to clear usb hub port feature\n";
		return false;
	}

	return true;
}

UsbHub::UsbHubPort::UsbHubPort(UsbHub& rHub, unsigned int portNo)
: m_rHub(rHub),
  m_portNo(portNo)
{
}

UsbHub::UsbHubPort::~UsbHubPort()
{
}

bool UsbHub::UsbHubPort::Reset(void)
{
	Printer &p = Printer::Get();

	int retries = 5;
	while (retries--)
	{
		if (m_rHub.SetPortFeature(kPortReset, 0, m_portNo + 1))
		{
			for (int count = 0; count < 200; count++)
				DelayMillisecond();

			if (!m_rHub.ClearPortFeature(kCPortReset, 0, m_portNo + 1))
			{
				p << "reset clear failure\n";
				continue;
			}

			for (int count = 0; count < 200; count++)
				DelayMillisecond();

			unsigned short status, change;
			if (m_rHub.GetPortStatus(status, change, m_portNo + 1))
			{
				if (status & 2)		//port enable
					return true;
				else
					p << "port reset, but not yet enabled\n";
			}
			else
				p << "get port status (to check enable) failed\n";
			continue;
		}

		p << "reset set failure\n";
	}

	if (!retries)
	{
		p << "reset timeout\n";
		return false;
	}

	ASSERT(0);
}

bool UsbHub::UsbHubPort::PowerOn(bool o)
{
	Printer &p = Printer::Get();

	if (o)
	{
		if (m_rHub.SetPortFeature(kPortPower, 0, m_portNo + 1))
		{
			for (int count = 0; count < m_rHub.m_hubDescriptor.m_pwrOn2PwrGood * 2; count++)
				DelayMillisecond();

			//check power turned on ok
			if (IsPoweredOn())
			{
				p << "powered on ok\n";
				return true;
			}
		}
	}
	else
	{
		if (m_rHub.ClearPortFeature(kPortPower, 0, m_portNo + 1))
		{
			for (int count = 0; count < m_rHub.m_hubDescriptor.m_pwrOn2PwrGood * 2; count++)
				DelayMillisecond();

			//check power turned off ok
			if (!IsPoweredOn())
			{
				p << "powered off ok\n";
				return true;
			}
		}
	}

	p << "power " << (o ? "on" : "false") << " failure\n";
	return false;
}

bool UsbHub::UsbHubPort::IsPoweredOn(void)
{
	Printer &p = Printer::Get();
	unsigned short status, change;

	if (!m_rHub.GetPortStatus(status, change, m_portNo + 1))
	{
		p << "failed to get port status\n";
		return false;
	}

	return (bool)((status >> 8) & 1);
}

bool UsbHub::UsbHubPort::IsDeviceAttached(void)
{
	Printer &p = Printer::Get();
	unsigned short status, change;

	if (!m_rHub.GetPortStatus(status, change, m_portNo + 1))
	{
		p << "failed to get port status\n";
		return false;
	}

	return (bool)(status & 1);
}

Speed UsbHub::UsbHubPort::GetPortSpeed(void)
{
	Printer &p = Printer::Get();
	unsigned short status, change;

	if (!m_rHub.GetPortStatus(status, change, m_portNo + 1))
	{
		p << "failed to get port status\n";
		return kInvalidSpeed;
	}

	p << "status is " << status << "\n";

	unsigned int speed = (status >> 9) & 3;
	p << "port speed is " << speed << "\n";

	switch (speed)
	{
		case 0:
			return kFullSpeed;
		case 1:
			return kLowSpeed;
		default:		//2
			return kHighSpeed;
	}
}

} /* namespace USB */
