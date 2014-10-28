/*
 * UdpPrinter.cpp
 *
 *  Created on: 25 Oct 2014
 *      Author: Simon
 */

#include "UdpPrinter.h"

UdpPrinter::UdpPrinter(Net::InternetIntf &rIntf, Net::UDP &rUdp,
		const Net::InternetAddrIntf &rAddr, unsigned short destPort)
: Printer(),
  m_rIntf(rIntf),
  m_rUdp(rUdp),
  m_rAddr(rAddr),
  m_destPort(destPort)
{
}

UdpPrinter::~UdpPrinter()
{
}

void UdpPrinter::PrintChar(char c)
{
	m_rUdp.SendDatagram(m_rIntf, m_rAddr, 0, m_destPort, &c, 1);
}

void UdpPrinter::PrintString(const char *pString, bool with_len, size_t len)
{
	if (!with_len)
		len = strlen(pString) + 1;

	while (len)
	{
		size_t to_send = len > 512 ? 512 : len;
		m_rUdp.SendDatagram(m_rIntf, m_rAddr, 0, m_destPort, pString, to_send);
		pString += to_send;
		len -= to_send;
	}
}
