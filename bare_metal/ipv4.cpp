/*
 * ipv4.cpp
 *
 *  Created on: 17 Oct 2014
 *      Author: simon
 */

#include "ipv4.h"
#include "Scheduler.h"
#include "ICMP.h"

namespace Net
{

void InternetIntf::RegisterProtocol(Protocol& rProt)
{
	m_protocols[rProt.DecodesProtocol()] = &rProt;
}

void InternetIntf::DemuxThreadFuncEntry(InternetIntf &rInetIntf)
{
	rInetIntf.DemuxThreadFunc();
}

IPv4::IPv4(LfRingType &rRecvRing, LfRingType &rSendRing)
: InternetIntf(),
  m_rRecvRing(rRecvRing),
  m_rSendRing(rSendRing)
{
	ASSERT(&rRecvRing);
	ASSERT(&rSendRing);

	m_sendMutex.SetName("IPv4 send");
}

IPv4::~IPv4(void)
{
}

void IPv4::AddAddr(unsigned char a, unsigned char b, unsigned char c,
		unsigned char d)
{
	m_localAddrs.push_back((a << 24) | (b << 16) | (c << 8) | d);
}

bool IPv4::EnqueuePacket(const InternetAddrIntf &rDest, int protocol, MaxPacket &rPayload)
{
	IPv4Addr *pDest = (IPv4Addr *)&rDest;
	//little endian
	union {
		IPv4Header m_header;
		unsigned int m_words[5];
	};

	m_header.m_destIpAddr = pDest->m_addr;
	m_header.m_dscp = 0;
	m_header.m_ecn = 0;

	//todo change to support fragmentation
	m_header.m_flags = 1 << IPv4Header::kDontFragment;
	m_header.m_fragmentOffset = 0;
	m_header.m_identification = 0;		//can be anything for atomic datagrams http://tools.ietf.org/html/rfc6864

	static const int kHeaderSize = 20;

	m_header.m_ihl = kHeaderSize >> 2;
	m_header.m_protocol = protocol;
	m_header.m_sourceIpAddr = m_localAddrs.front();		//todo change
	m_header.m_totalLength = kHeaderSize + rPayload.m_size - rPayload.m_startOffset;
	m_header.m_ttl = 255;
	m_header.m_version = 4;
	m_header.m_headerChecksum = 0;

	for (int count = 0; count < 5; count++)
		m_words[count] = __builtin_bswap32(m_words[count]);

	//compute and insert the checksum
	unsigned short checksum = ip_checksum(&m_header, kHeaderSize);
	unsigned short *pDataBE = (unsigned short *)&m_words;
	pDataBE[5] = __builtin_bswap16(checksum);

	//put it in the packet
	m_sendMutex.Lock();
	m_sendTempPacket.m_startOffset = 0;
	m_sendTempPacket.m_size = kHeaderSize + rPayload.m_size - rPayload.m_startOffset;

	memcpy(&m_sendTempPacket.x[0], &m_header, kHeaderSize);
	memcpy(&m_sendTempPacket.x[kHeaderSize],
			&rPayload.x[rPayload.m_startOffset],
			rPayload.m_size - rPayload.m_startOffset);

	//send it
	bool ok = m_rSendRing.Push(m_sendTempPacket);

	m_sendMutex.Unlock();
	return ok;
}

void IPv4::DemuxThreadFunc(void)
{
	Thread *pThisThread = Scheduler::GetMasterScheduler().WhatIsRunning();
	ASSERT(pThisThread);

	union PacketUnion
	{
		MaxPacket m_bytes;
		unsigned int m_words[5];
		IPv4Header m_header;
	} *pInBE = new PacketUnion;
	ASSERT(pInBE);

	union {
		unsigned int m_words[5];
		IPv4Header m_header;
	} in;

	while (1)
	{
		m_rRecvRing.Pop(pInBE->m_bytes);
		ASSERT(pInBE->m_bytes.m_startOffset == 0);

		for (int count = 0; count < 5; count++)
			in.m_words[count] = __builtin_bswap32(pInBE->m_words[count]);

		PacketError err;

		//check ipv4
		if (in.m_header.m_version != 4)
			continue;

		//check header size
		int header_size = in.m_header.m_ihl * 4;
		if (header_size < 20 || header_size > 60)
			continue;

		//check all the sizes sum up
		if (in.m_header.m_totalLength != pInBE->m_bytes.m_size)
			continue;

		//check checksum
		pInBE->m_bytes.x[10] = 0;
		pInBE->m_bytes.x[11] = 0;
		unsigned short checksum = ip_checksum(&pInBE->m_header, header_size);
		if (checksum != in.m_header.m_headerChecksum)
			continue;

		//check the destination; we have no routing
		bool found = false;
		for (auto it = m_localAddrs.begin(); it != m_localAddrs.end(); it++)
			if (*it == in.m_header.m_destIpAddr)
			{
				found = true;
				break;
			}

		if (!found)
			continue;

		//find a decoder for the protocol
		auto it = m_protocols.find(in.m_header.m_protocol);
		if (it == m_protocols.end())
			continue;

		Protocol *pProt = it->second;
		ASSERT(pProt);
		pInBE->m_bytes.m_startOffset = header_size;

		err = pProt->DecodeAndDispatch(IPv4Addr(in.m_header.m_sourceIpAddr),
			pInBE->m_bytes, in.m_header.m_totalLength - header_size);

		if (err == kErrorOk)
			continue;			//no problem

		it = m_protocols.find(1);
		if (it == m_protocols.end())
			continue;			//uh-oh, no icmp

		switch (err)
		{
			case kDestPortUnreachable:
				((ICMP *)it->second)->EnqueueErrorPacket(*this, IPv4Addr(in.m_header.m_sourceIpAddr),
						err, &pInBE->m_bytes.x[0], pInBE->m_bytes.m_size);
				break;
			default:
				break;
		}
	}
}

}
