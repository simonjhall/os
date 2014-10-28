/*
 * EhciRootHub.cpp
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#include "EhciRootHub.h"
#include "Ehci.h"
#include "print_uart.h"

namespace USB
{

EhciRootHub::EhciRootHub(Ehci& rHostController)
: m_rHostController(rHostController)
{
	for (unsigned int count = 0; count < GetNumPorts(); count++)
		m_ports.push_back(EhciPort(*this, count));
}

EhciRootHub::~EhciRootHub()
{
}

Port* EhciRootHub::GetPort(unsigned int p)
{
	if (p < GetNumPorts())
		return &m_ports[p];
	else
		return 0;
}

unsigned int EhciRootHub::GetNumPorts(void)
{
	return m_rHostController.m_pCaps->m_hcsParams & 0xf;
}

bool EhciRootHub::IsRootHub(void)
{
	return true;
}

} /* namespace USB */

USB::EhciRootHub::EhciPort::EhciPort(EhciRootHub& rHub, unsigned int portNo)
: m_rHub(rHub),
  m_portNumber(portNo)
{
}

USB::EhciRootHub::EhciPort::~EhciPort()
{
}

bool USB::EhciRootHub::EhciPort::PowerOn(bool o)
{
	m_rHub.m_rHostController.PortPower(m_portNumber, o);
	return IsPoweredOn();
}

bool USB::EhciRootHub::EhciPort::Reset(void)
{
	Printer &p = Printer::Get();
	p << "ehci root hub port reset (" << m_portNumber << ")\n";
	m_rHub.m_rHostController.PortReset(m_portNumber);
	return true;
}

bool USB::EhciRootHub::EhciPort::IsPoweredOn(void)
{
	return m_rHub.m_rHostController.IsPortPowered(m_portNumber);
}

bool USB::EhciRootHub::EhciPort::IsDeviceAttached(void)
{
	return m_rHub.m_rHostController.IsDeviceAttached(m_portNumber);
}

USB::Speed USB::EhciRootHub::EhciPort::GetPortSpeed(void)
{
//	PrinterUart<PL011> p;
//	p << "get port speed\n";
	//ehci only does high speed
	if (!IsDeviceAttached())
	{
//		p << __FILE__ << " " << __LINE__ << "\n";
		return kInvalidSpeed;
	}

	//can't tell until it's powered
	if (!IsPoweredOn())
	{
//		p << __FILE__ << " " << __LINE__ << "\n";
		return kInvalidSpeed;
	}

	//quick test
	if (((m_rHub.m_rHostController.m_pOps->m_portsSc[m_portNumber] >> 10) & 3) == 1)
	{
//		p << __FILE__ << " " << __LINE__ << "\n";
		return kLowSpeed;
	}

	//not sure about full speed
//	p << __FILE__ << " " << __LINE__ << "\n";
	return kHighSpeed;
}
