/*
 * UsbDevice.cpp
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#include "UsbDevice.h"
#include "Hcd.h"

#include "Interface.h"
#include "Configuration.h"
#include "EndPoint.h"

#include "Hub.h"

#include "common.h"

namespace USB
{

UsbDevice::UsbDevice(Hcd &rHostController, Port &rPort)
: m_endPointZero(8),
  m_address(-1),
  m_rHostController(rHostController),
  m_rPort(rPort)
{
}

UsbDevice::UsbDevice(UsbDevice& rOther)
: m_endPointZero(rOther.m_endPointZero),
  m_deviceDescriptor(rOther.m_deviceDescriptor),
  m_configurations(rOther.m_configurations),
  m_address(rOther.m_address),
  m_rHostController(rOther.m_rHostController),
  m_rPort(rOther.m_rPort),
  m_speed(rOther.m_speed)
{
	if (rOther.Cloned())
	{
		if (!ReadDescriptors())
			m_address = -1;
	}
	else
	{
		ASSERT(0);			//two broken objects?
	}
}

UsbDevice::~UsbDevice()
{
	if (Valid())
		m_rHostController.ReleaseBusAddress(m_address);
}

bool UsbDevice::BasicInit(int address)
{
	if (!m_rPort.Reset())
		return false;

	if (!ReadDescriptors())
		return false;

	m_endPointZero.m_descriptor.m_maxPacketSize = m_deviceDescriptor.m_maxPacketSize0;

	if (!m_rPort.Reset())
		return false;

	if (!ReadDescriptors())
		return false;

	if (!SetAddress(address))
		return false;

	return true;
}

bool UsbDevice::Identify(unsigned short & rVendor, unsigned short & rProduct,
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
}

bool UsbDevice::Valid(void)
{
	if (m_address > 0)
		return true;
	else
		return false;
}

bool UsbDevice::SetConfiguration(unsigned int c)
{
	if (c >= m_configurations.size())
		return false;

	SetupPacket packet(sm_reqTypeHostToDevice | sm_reqTypeStandard | sm_reqTypeDevice,
			sm_reqSetConfiguration, c, 0, 0);

	if (!m_rHostController.SubmitControlMessage(m_endPointZero, *this, 0, 0, packet))
		return false;
}

} /* namespace USB */
