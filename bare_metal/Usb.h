/*
 * Usb.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef USB_H_
#define USB_H_

namespace USB
{

class Hcd;
class UsbDevice;
struct EndPoint;
struct Interface;
struct Configuration;

#pragma pack(push)
#pragma pack(1)
struct SetupPacket
{
	SetupPacket(unsigned char requestType, unsigned char request,
			unsigned short value, unsigned short index, unsigned short length)
	: m_requestType(requestType),
	  m_request(request),
	  m_value(value),
	  m_index(index),
	  m_length(length)
	{
	}

	unsigned char m_requestType;
	unsigned char m_request;
	unsigned short m_value;
	unsigned short m_index;
	unsigned short m_length;

};

struct DeviceDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned short m_usbVersion;
	unsigned char m_deviceClass;
	unsigned char m_deviceSubClass;
	unsigned char m_deviceProtocol;
	unsigned char m_maxPacketSize0;
	unsigned short m_idVendor;
	unsigned short m_idProduct;
	unsigned short m_devVersion;
	unsigned char m_manufacturerIndex;
	unsigned char m_productIndex;
	unsigned char m_serialIndex;
	unsigned char m_numConfigurations;
};

struct ConfigurationDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned short m_totalLength;
	unsigned char m_numInterfaces;
	unsigned char m_configurationValue;
	unsigned char m_configurationIndex;
	unsigned char m_attributes;
	unsigned char m_maxPower;
};

struct InterfaceDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned char m_interfaceNumber;
	unsigned char m_alternateSetting;
	unsigned char m_numEndpoints;
	unsigned char m_interfaceClass;
	unsigned char m_interfaceSubclass;
	unsigned char m_interfaceProtocol;
	unsigned char m_interfaceIndex;
};

struct EndpointDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned char m_endpointAddress;
	unsigned char m_attributes;
	unsigned short m_maxPacketSize;
	unsigned char m_interval;
};

#pragma pack(pop)

}


#endif /* USB_H_ */
