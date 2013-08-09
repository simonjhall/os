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

#include "common.h"

namespace USB
{

UsbDevice::UsbDevice(Hcd &rHostController)
: m_address(-1),
  m_rHostController(rHostController)
{
}

UsbDevice::UsbDevice(UsbDevice& rOther)
: m_address(rOther.m_address),
  m_rHostController(rOther.m_rHostController)
{
	if (rOther.Cloned())
	{
		if (!ReadDescriptors())
			m_address = -1;
	}
	else
	{
		ASSERT(0);
		return false;			//two broken objects?
	}
}

UsbDevice::~UsbDevice()
{
	if (Valid())
		m_rHostController.ReleaseBusAddress(m_address);
}

bool UsbDevice::BasicInit(int address)
{
	if (!SetAddress(address))
		return false;
	else
		return ReadDescriptors();
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
	return true;
}

bool UsbDevice::SetAddress(int address)
{
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
}

} /* namespace USB */
