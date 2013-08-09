/*
 * EndPoint.cpp
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#include "EndPoint.h"

namespace USB
{

} /* namespace USB */

USB::EndPoint::EndPoint(unsigned int maxSize)
{
	m_descriptor.m_attributes = 0;
	m_descriptor.m_descriptorType = 5;
	m_descriptor.m_endpointAddress = 0;
	m_descriptor.m_interval = 0;
	m_descriptor.m_length = sizeof(m_descriptor);
	m_descriptor.m_maxPacketSize = maxSize;
}

USB::EndPoint::EndPoint(EndpointDescriptor& rOther)
: m_descriptor(rOther)
{
}
