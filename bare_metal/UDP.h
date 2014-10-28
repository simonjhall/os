/*
 * UDP.h
 *
 *  Created on: 19 Oct 2014
 *      Author: simon
 */

#ifndef UDP_H_
#define UDP_H_

#include "Protocol.h"
#include "LfRing.h"
#include "ipv4.h"

namespace Net
{

class UDP : public Protocol
{
public:
#pragma pack(push)
#pragma pack(1)
//valid, in little endian format
	struct UdpHeader
	{
		unsigned short m_destPort;
		unsigned short m_srcPort;
		unsigned short m_checksum;
		unsigned short m_length;
	};
#pragma pack(pop)

	struct UdpPacket
	{
		struct MaxPacket m_bytes;
		unsigned int m_dataSize;
		InternetAddrIntf *m_pSourceAddr;
		unsigned short m_sourcePort;
	};

	typedef LfRing<UdpPacket, 16, 15> LfRingType;

	struct Socket
	{
		unsigned short m_port;
		LfRingType m_recvQueue;
	};

	UDP();
	virtual ~UDP();

	inline virtual int DecodesProtocol(void) { return 17; }
	virtual PacketError DecodeAndDispatch(const InternetAddrIntf &rAddr, MaxPacket &, unsigned int dataSize);

	Socket *CreateSocket(unsigned short port);
	bool DeleteSocket(unsigned short port);

	PacketError SendDatagram(InternetIntf &rIntf, const InternetAddrIntf &rAddr, unsigned short sourcePort, unsigned short destPort, const void *pData, unsigned int len);

protected:
	UdpPacket m_recvTempPacket;
	UdpPacket m_sendTempPacket;
	Mutex m_sendMutex;

	std::map<unsigned short, Socket *> m_sockets;
};

} /* namespace Net */
#endif /* UDP_H_ */
