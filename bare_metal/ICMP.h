/*
 * ICMP.h
 *
 *  Created on: 17 Oct 2014
 *      Author: simon
 */

#ifndef ICMP_H_
#define ICMP_H_

#include <map>

#include "Protocol.h"
#include "LfRing.h"
#include "ipv4.h"

class Thread;

namespace Net
{

class ICMP : public Protocol
{
public:
#pragma pack(push)
#pragma pack(1)
//valid, in little endian format
	struct IcmpHeader
	{
		unsigned short m_checksum;
		unsigned char m_code;
		unsigned char m_type;
	};
#pragma pack(pop)

	struct IcmpPacket
	{
		struct MaxPacket m_bytes;
		unsigned int m_dataSize;
		InternetAddrIntf *m_pSourceAddr;
	};

	typedef LfRing<IcmpPacket, 16, 15> LfRingType;

	ICMP();
	virtual ~ICMP();

	inline virtual int DecodesProtocol(void) { return 1; };
	virtual PacketError DecodeAndDispatch(const InternetAddrIntf &, MaxPacket &, unsigned int dataSize);

	bool AddPingReplyListener(unsigned short process, LfRingType &rRecvRing);
	inline LfRingType &GetPingRequestListener(void) { return m_pingRequest; };

	//BE data
	bool EnqueuePacket(InternetIntf &rIntf, const InternetAddrIntf &rDest, IcmpPacket &rPacket);
	bool EnqueueErrorPacket(InternetIntf &rIntf, const InternetAddrIntf &rDest, PacketError err, void *pOrig, unsigned int size);

private:
	std::map<unsigned short, LfRingType *> m_pingReplies;
	LfRingType m_pingRequest;

	IcmpPacket m_tempPacket;
	Mutex m_sendMutex;
};

}
#endif /* ICMP_H_ */
