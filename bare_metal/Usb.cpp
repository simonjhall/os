/*
 * Usb.cpp
 *
 *  Created on: 11 Aug 2013
 *      Author: simon
 */

#include "Usb.h"

void USB::DeviceDescriptor::Print(Printer& p, int tab)
{
	for (int count = 0; count < tab; count++)
		p << "\t";

	p << "usb version " << m_usbVersion << " class "
		<< m_deviceClass << " sub " << m_deviceSubClass
		<< " prot " << m_deviceProtocol << " max packet " << m_maxPacketSize0
		<< " vendor " << m_idVendor << " product " << m_idProduct << "\n";
}

void USB::ConfigurationDescriptor::Print(Printer& p, int tab)
{
	for (int count = 0; count < tab; count++)
		p << "\t";

	p << "configuration " << m_configurationValue
			<< " attributes " << m_attributes
			<< " max power " << m_maxPower
			<< " interfaces " << m_numInterfaces << "\n";
}

void USB::InterfaceDescriptor::Print(Printer& p, int tab)
{
	for (int count = 0; count < tab; count++)
		p << "\t";

	p << "interface " << m_interfaceNumber
			<< " alternate " << m_alternateSetting
			<< " endpoints " << m_numEndpoints
			<< " class " << m_interfaceClass
			<< " sub " << m_interfaceSubclass
			<< " prot " << m_interfaceProtocol << "\n";
}

void USB::EndpointDescriptor::Print(Printer& p, int tab)
{
	for (int count = 0; count < tab; count++)
		p << "\t";

	p << "addr " << m_endpointAddress
			<< " attributes " << m_attributes
			<< " max packet " << m_maxPacketSize
			<< " interval " << m_interval << "\n";
}
