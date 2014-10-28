/*
 * UsbDevice.cpp
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#include <string.h>
#include <stdlib.h>

#include "UsbDevice.h"
#include "Hcd.h"

#include "Interface.h"
#include "Configuration.h"
#include "EndPoint.h"

#include "Hub.h"

#include "common.h"
#include "print_uart.h"
#include "virt_memory.h"

namespace USB
{

UsbDevice::UsbDevice(Hcd &rHostController, Port &rPort)
: m_endPointZero(64),
  m_pCurrentConfiguration(0),
  m_address(-1),
  m_rHostController(rHostController),
  m_rPort(rPort),
  m_speed(kInvalidSpeed)
{
}

UsbDevice::UsbDevice(UsbDevice& rOther)
: m_endPointZero(rOther.m_endPointZero),
  m_deviceDescriptor(/*rOther.m_deviceDescriptor*/),
  m_configurations(/*rOther.m_configurations*/),
  m_pCurrentConfiguration(0),
  m_address(rOther.m_address),
  m_rHostController(rOther.m_rHostController),
  m_rPort(rOther.m_rPort),
  m_speed(rOther.m_speed)
{
	if (!rOther.Valid())
	{
		m_address = -1;
		return;
	}

	if (!ReadDescriptors())
		m_address = -1;
}

UsbDevice::~UsbDevice()
{
	return;
	if (Valid())
	{
		m_rHostController.ReleaseBusAddress(m_address);
		//device is still programmed to respond on this address though
		//let's reset the port and get it back to zero
		m_rPort.Reset();
	}
}

bool UsbDevice::BasicInit(int address)
{
	Printer &p = Printer::Get();
	if (!m_rPort.Reset())
	{
		p << "port reset failed\n";
		return false;
	}

	//set the speed AFTER the reset
	m_speed = m_rPort.GetPortSpeed();

	if (!ReadDescriptors())
	{
		p << "read descriptors failed\n";
		return false;
	}

	m_endPointZero.m_descriptor.m_maxPacketSize = m_deviceDescriptor.m_maxPacketSize0;

	if (!SetAddress(address))
	{
		p << "set address failed\n";
		return false;
	}

	return true;
}

bool UsbDevice::ProductIdentify(unsigned short & rVendor, unsigned short & rProduct,
		unsigned short & rVersion)
{
	if (!Valid())
		return false;

	rVendor = m_deviceDescriptor.m_idVendor;
	rProduct = m_deviceDescriptor.m_idProduct;
	rVersion = m_deviceDescriptor.m_devVersion;

	return true;
}

bool UsbDevice::Cloned(void)
{
	if (!Valid())
		return false;

	m_address = -1;
	m_configurations.clear();
	return true;
}

bool UsbDevice::SetAddress(int address)
{
	if (address > 127)
		return false;

	SetupPacket packet(sm_reqTypeHostToDevice | sm_reqTypeStandard | sm_reqTypeDevice,
			sm_reqSetAddress, address, 0, 0);

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, 0, 0, packet))
		return false;

	m_address = address;
	return true;
}

bool UsbDevice::ReadDescriptors(void)
{
	Printer &p = Printer::Get();

	char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	DeviceDescriptor *dd = (DeviceDescriptor *)buffer;
	ConfigurationDescriptor *cd = (ConfigurationDescriptor *)buffer;

	//get the device descriptor
	SetupPacket packet(sm_reqTypeDeviceToHost | sm_reqTypeStandard | sm_reqTypeDevice,
			sm_reqGetDescriptor, 1 << 8, 0, 64);

	int retry = 5;

	while (--retry)
	{
		if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, buffer, 64, packet))
		{
			p << "failed to get device descriptor\n";
			continue;
		}

		if (dd->m_descriptorType != 1)
		{
			p << "device descriptor type value not expected, " << retry << "\n";
			VirtMem::DumpMem((unsigned int *)dd, sizeof(*dd) >> 2);
			continue;
		}

		if (dd->m_length != sizeof(DeviceDescriptor))
		{
			p << "device descriptor length value not expected, " << retry << "\n";
			continue;
		}
		break;
	}
	if (!retry)
	{
		p << "bailing out of read descriptors\n";
		return false;
	}
	else if (retry != 4)
	{
		p << "got there in the end, " << retry << "\n";
		VirtMem::DumpMem((unsigned int *)dd, sizeof(*dd) >> 2);
	}

	memcpy(&m_deviceDescriptor, dd, sizeof(DeviceDescriptor));

	//get each configuration descriptor
	for (unsigned int con = 0; con < m_deviceDescriptor.m_numConfigurations; con++)
	{
		memset(buffer, 0, sizeof(buffer));

		ASSERT(con < 256);
		packet.m_value = (2 << 8) | con;

		retry = 5;

		while (--retry)
		{
			if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, buffer, 64, packet))
			{
				p << "failed to read configuration " << con << "\n";
				continue;
			}

			if (cd->m_descriptorType != 2)
			{
				p << "configuration " << con << " type mismatch\n";
				continue;
			}

			if (cd->m_length != sizeof(ConfigurationDescriptor))
			{
				p << "configuration " << con << " length mismatch\n";
				continue;
			}
			break;
		}
		if (!retry)
		{
			p << "bailing out of read configuration\n";
			return false;
		}

		Configuration c;
		memcpy(&c.m_descriptor, cd, sizeof(ConfigurationDescriptor));

		char *pWalking = (char *)cd + cd->m_length;

		for (unsigned int count = 0; count < c.m_descriptor.m_numInterfaces; count++)
		{
			InterfaceDescriptor *id = (InterfaceDescriptor *)pWalking;

			if (id->m_descriptorType != 4)
			{
				p << "interface " << con << "/" << count <<" type mismatch\n";
				return false;
			}

			if (id->m_length != sizeof(InterfaceDescriptor))
			{
				p << "interface " << con << "/" << count <<" length mismatch\n";
				return false;
			}

			pWalking += id->m_length;

			Interface in;
			memcpy(&in.m_descriptor, id, sizeof(InterfaceDescriptor));

			for (unsigned int count = 0; count < id->m_numEndpoints; count++)
			{
				EndpointDescriptor *ed = (EndpointDescriptor *)pWalking;

				if (ed->m_descriptorType != 5)
				{
					p << "endpoint type mismatch\n";
					return false;
				}

				if (ed->m_length != sizeof(EndpointDescriptor))
				{
					p << "endpoint length mismatch\n";
					return false;
				}

				pWalking += ed->m_length;

				EndPoint end(*ed);
				in.m_endPoints.push_back(end);
			}

			c.m_interfaces.push_back(in);
		}
		m_configurations.push_back(c);
	}

	return true;
}

bool UsbDevice::Valid(void)
{
	if (m_address > 0)
		return true;
	else
		return false;
}

Speed UsbDevice::GetSpeed(void)
{
	return m_speed;
}

int UsbDevice::GetAddress(void)
{
	if (Valid())
		return m_address;

	return 0;
}

void UsbDevice::DumpDescriptors(Printer& p)
{
	p << "device descriptor dump for " << this << ", device " << m_address << "\n";

	if (!Valid())
	{
		p << "\tdevice not valid at this stage\n";
		return;
	}

	p << "\tdevice descriptor\n";
	m_deviceDescriptor.Print(p, 2);
	p << "\n";

	for (auto cit = m_configurations.begin(); cit != m_configurations.end(); cit++)
	{
		p << "\t\tconfiguration\n";
		cit->m_descriptor.Print(p, 3);
		p << "\n";

		for (auto iit = cit->m_interfaces.begin(); iit != cit->m_interfaces.end(); iit++)
		{
			p << "\t\t\tinterface\n";
			iit->m_descriptor.Print(p, 4);
			p << "\n";

			for (auto eit = iit->m_endPoints.begin(); eit != iit->m_endPoints.end(); eit++)
			{
				p << "\t\t\t\tendpoint\n";
				eit->m_descriptor.Print(p, 5);
				p << "\n";
			}
		}
	}
}

bool UsbDevice::SetConfiguration(unsigned int c)
{
	if (c >= m_configurations.size())
		return false;

	m_pCurrentConfiguration = 0;

	unsigned int count = 0;
	for (auto it = m_configurations.begin(); it != m_configurations.end(); it++)
		if (count == c)
		{
			m_pCurrentConfiguration = &(*it);
			break;
		}

	ASSERT(m_pCurrentConfiguration);

	SetupPacket packet(sm_reqTypeHostToDevice | sm_reqTypeStandard | sm_reqTypeDevice,
			sm_reqSetConfiguration, m_pCurrentConfiguration->m_descriptor.m_configurationValue, 0, 0);

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, 0, 0, packet))
		return false;

	return true;
}

bool UsbDevice::FullInit(void)
{
	ASSERT(0);			//should not be called
	return false;
}

bool UsbDevice::CanConstruct(void)
{
	ASSERT(0);			//should not be called
	return false;
}

} /* namespace USB */
