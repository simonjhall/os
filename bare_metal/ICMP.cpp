/*
 * ICMP.cpp
 *
 *  Created on: 17 Oct 2014
 *      Author: simon
 */

#include "ICMP.h"
#include "ipv4.h"

namespace Net
{

ICMP::ICMP()
: Protocol()
{
	m_sendMutex.SetName("ICMP send");
}

ICMP::~ICMP()
{
	// TODO Auto-generated destructor stub
}

PacketError ICMP::DecodeAndDispatch(const InternetAddrIntf &rAddr, MaxPacket &rDataBE, unsigned int dataSize)
{
	union {
		IcmpHeader m_header;
		unsigned int m_words[2];
	} in;

	unsigned int *pDataBegin = (unsigned int *)&rDataBE.x[rDataBE.m_startOffset];

	in.m_words[0] = __builtin_bswap32(pDataBegin[0]);
	in.m_words[1] = __builtin_bswap32(pDataBegin[1]);

	//clear the checksum (big endian)
	*pDataBegin = *pDataBegin & 0xffff;

	unsigned short checksum = ip_checksum(pDataBegin, dataSize);
	if (checksum != in.m_header.m_checksum)
		return kErrorChecksum;

	m_tempPacket.m_pSourceAddr = rAddr.Clone();
	if (!m_tempPacket.m_pSourceAddr)
	{
		ASSERT(!"cannot clone addr\n");
		return kInternalError;
	}

	m_tempPacket.m_bytes = rDataBE;
	m_tempPacket.m_dataSize = dataSize;

	switch (in.m_header.m_type)
	{
		//ping echo reply
		case 0:
			m_pingReplies[(in.m_words[1] & 0xffff0000) >> 16]->Push(m_tempPacket);
			break;
		//ping echo request
		case 8:
			m_pingRequest.Push(m_tempPacket);
			break;
		default:
			break;
	}

	return kErrorOk;
}

bool ICMP::AddPingReplyListener(unsigned short process, LfRingType& rRecvRing)
{
	if (m_pingReplies.find(process) == m_pingReplies.end())
		return false;

	m_pingReplies[process] = &rRecvRing;
	return true;
}

bool ICMP::EnqueuePacket(InternetIntf &rIntf, const InternetAddrIntf &rDest, IcmpPacket &rPacket)
{
	unsigned char *pDataBegin = (unsigned char *)&rPacket.m_bytes.x[rPacket.m_bytes.m_startOffset];

	//recompute the checksum
	unsigned short *pChecksum = (unsigned short *)&pDataBegin[2];
	*pChecksum = 0;
	unsigned short checksum = Net::ip_checksum(pDataBegin, rPacket.m_dataSize);
	*pChecksum = __builtin_bswap16(checksum);

	return rIntf.EnqueuePacket(rDest, 1, rPacket.m_bytes);
}

bool ICMP::EnqueueErrorPacket(InternetIntf &rIntf, const InternetAddrIntf &rDest, PacketError err, void *pOrig, unsigned int size)
{
	union {
		IcmpHeader m_header;
		unsigned int m_word;
	};

	switch (err)
	{
		case kDestPortUnreachable:
		{
			m_header.m_code = 3;
			m_header.m_type = 3;

			//get the ip header length
			unsigned int header_size = *(unsigned char *)pOrig & 0xf;
			header_size *= 4;

			if (size < header_size + 8)
				return false;

			//reverse it
			m_word = __builtin_bswap32(m_word);

			m_sendMutex.Lock();

			memcpy(&m_tempPacket.m_bytes.x[0], &m_word, 4);

			//the second word is unused
			m_word = 0;
			memcpy(&m_tempPacket.m_bytes.x[4], &m_word, 4);

			memcpy(&m_tempPacket.m_bytes.x[8], pOrig, header_size + 8);

			m_tempPacket.m_bytes.m_size = 8 + header_size + 8;
			m_tempPacket.m_bytes.m_startOffset = 0;
			m_tempPacket.m_dataSize = 8 + header_size + 8;
			m_tempPacket.m_pSourceAddr = 0;

			bool ok = EnqueuePacket(rIntf, rDest, m_tempPacket);

			m_sendMutex.Unlock();
			return ok;
		}
		default:
			return false;
	}
}

}
