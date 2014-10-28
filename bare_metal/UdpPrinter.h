/*
 * UdpPrinter.h
 *
 *  Created on: 25 Oct 2014
 *      Author: Simon
 */

#ifndef UDPPRINTER_H_
#define UDPPRINTER_H_

#include "print.h"
#include "ipv4.h"
#include "UDP.h"

class UdpPrinter: public Printer
{
public:
	UdpPrinter(Net::InternetIntf &rIntf, Net::UDP &rUdp, const Net::InternetAddrIntf &rAddr, unsigned short destPort);
	virtual ~UdpPrinter();

	virtual void PrintChar(char c);
	virtual void PrintString(const char *pString, bool with_len = false, size_t len = 0);

protected:
	Net::InternetIntf &m_rIntf;
	Net::UDP &m_rUdp;
	const Net::InternetAddrIntf &m_rAddr;
	unsigned short m_destPort;
};

#endif /* UDPPRINTER_H_ */
