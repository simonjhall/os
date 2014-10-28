/*
 * UDP.cpp
 *
 *  Created on: 19 Oct 2014
 *      Author: simon
 */

#include "UDP.h"

namespace Net
{

UDP::UDP()
: Protocol()
{
	m_sendMutex.SetName("UDP send");
}

UDP::~UDP()
{
	// TODO Auto-generated destructor stub
}

PacketError UDP::DecodeAndDispatch(const InternetAddrIntf &rAddr, MaxPacket &rDataBE, unsigned int dataSize)
{
	union {
		UdpHeader m_header;
		unsigned int m_words[2];
	} in;

	unsigned int *pDataBegin = (unsigned int *)&rDataBE.x[rDataBE.m_startOffset];

	in.m_words[0] = __builtin_bswap32(pDataBegin[0]);
	in.m_words[1] = __builtin_bswap32(pDataBegin[1]);

	//find if the destination port is valid
	auto pit = m_sockets.find(in.m_header.m_destPort);
	if (pit == m_sockets.end())
		return kDestPortUnreachable;

	m_recvTempPacket.m_pSourceAddr = rAddr.Clone();
	if (!m_recvTempPacket.m_pSourceAddr)
	{
		ASSERT(!"cannot clone addr\n");
		return kInternalError;
	}

	m_recvTempPacket.m_bytes = rDataBE;
	m_recvTempPacket.m_dataSize = dataSize - sizeof(in.m_header);
	m_recvTempPacket.m_sourcePort = in.m_header.m_srcPort;

	//move the data up to skip the header
	m_recvTempPacket.m_bytes.m_startOffset += sizeof(in.m_header);

	//stick it in the appropriate socket's recv queue
	pit->second->m_recvQueue.Push(m_recvTempPacket);

	return kErrorOk;
}

auto UDP::CreateSocket(unsigned short port) -> Socket *
{
	//shouldn't be another one there
	if (m_sockets.find(port) != m_sockets.end())
		return 0;

	return m_sockets[port];
}

bool UDP::DeleteSocket(unsigned short port)
{
	if (m_sockets.find(port) == m_sockets.end())
		return false;

	auto ok = m_sockets.erase(port);
	ASSERT(ok);

	return true;
}

PacketError UDP::SendDatagram(InternetIntf &rIntf, const InternetAddrIntf& rAddr, unsigned short sourcePort,
		unsigned short destPort, const void* pData, unsigned int len)
{
	union {
		UdpHeader m_header;
		unsigned int m_words[2];
	} in;

	in.m_header.m_destPort = destPort;
	in.m_header.m_srcPort = sourcePort;
	in.m_header.m_length = len + sizeof(UdpHeader);
	in.m_header.m_checksum = 0;

	if (len + sizeof(UdpHeader) > 65535)
		return kBadLength;

	in.m_words[0] = __builtin_bswap32(in.m_words[0]);
	in.m_words[1] = __builtin_bswap32(in.m_words[1]);

	m_sendMutex.Lock();

	//copy the data into the packet
	memcpy(m_sendTempPacket.m_bytes.x, in.m_words, sizeof(UdpHeader));
	memcpy(&m_sendTempPacket.m_bytes.x[sizeof(UdpHeader)], pData, len);

	m_sendTempPacket.m_bytes.m_startOffset = 0;
	m_sendTempPacket.m_bytes.m_size = len + sizeof(UdpHeader);

	bool ok = rIntf.EnqueuePacket(rAddr, 17, m_sendTempPacket.m_bytes);

	m_sendMutex.Unlock();

	return ok ? kErrorOk : kInternalError;
}

}
 /* namespace Net */
