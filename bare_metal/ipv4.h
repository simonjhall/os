/*
 * ipv4.h
 *
 *  Created on: 17 Oct 2014
 *      Author: simon
 */

#ifndef IPV4_H_
#define IPV4_H_

#include "LinkLayer.h"
#include "Protocol.h"
#include "LfRing.h"
#include <string.h>

#include <list>
#include <map>

namespace Net
{

//http://www.microhowto.info/howto/calculate_an_internet_protocol_checksum_in_c.h
inline uint16_t ip_checksum(void* vdata,size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    uint32_t acc=0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i=0;i+1<length;i+=2) {
        uint16_t word;
        memcpy(&word,data+i,2);
        acc+=__builtin_bswap16(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        uint16_t word=0;
        memcpy(&word,data+length-1,1);
        acc+=__builtin_bswap16(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return ~acc;
}

class InternetAddrIntf
{
public:
	inline virtual ~InternetAddrIntf(void) {};
	virtual InternetAddrIntf *Clone(void) const = 0;
};

class IPv4Addr : public InternetAddrIntf
{
public:
	inline IPv4Addr(unsigned int a)
	: m_addr(a)
	{
	}

	inline IPv4Addr(unsigned char a, unsigned char b, unsigned char c,
		unsigned char d)
	: m_addr((a << 24) | (b << 16) | (c << 8) | d)
	{
	}

	virtual InternetAddrIntf *Clone(void) const
	{
		return new IPv4Addr(m_addr);
	}

	unsigned int m_addr;
};

typedef struct MaxPacket
{
	unsigned char x[65536];
	unsigned int m_size;			//from zero byte to the last valid byte
	unsigned int m_startOffset;		//when the first useful byte is

	/*inline MaxPacket(void)
	: m_size(0)
	{
	}

	inline MaxPacket(const MaxPacket &rOther)
	{
		memcpy(&x, rOther.x, rOther.m_size);
		m_size = rOther.m_size;
	}*/

	inline MaxPacket &operator=(const MaxPacket &rOther)
	{
		memcpy(&x, rOther.x, rOther.m_size);
		m_size = rOther.m_size;
		m_startOffset = rOther.m_startOffset;

		return *this;
	}
} MaxPacket;

class InternetIntf
{
public:
	inline virtual ~InternetIntf(void) {};
	virtual void RegisterProtocol(Protocol &rProt);

	virtual void DemuxThreadFunc(void) = 0;
	static void DemuxThreadFuncEntry(InternetIntf &rInetIntf);

	//BE data
	virtual bool EnqueuePacket(const InternetAddrIntf &rDest, int protocol, MaxPacket &rPayload) = 0;

protected:
	std::map<int, Protocol *> m_protocols;
};

class IPv4 : public InternetIntf
{
public:
	typedef LfRing<MaxPacket, 16, 15> LfRingType;

#pragma pack(push)
#pragma pack(1)
//valid, in little endian format
	struct IPv4Header
	{
		unsigned short m_totalLength;

		unsigned m_ecn :2;
		unsigned m_dscp :6;

		unsigned m_ihl :4;
		unsigned m_version :4;

		unsigned m_fragmentOffset :13;
		unsigned m_flags :3;

		unsigned short m_identification;

		unsigned short m_headerChecksum;

		unsigned char m_protocol;

		unsigned char m_ttl;

		unsigned int m_sourceIpAddr;

		unsigned int m_destIpAddr;

		unsigned int m_options[10];

		enum Flags
		{
			kReserved = 0,
			kDontFragment = 1,
			kMoreFragments = 2,
		};
	};
#pragma pack(pop)


	IPv4(LfRingType &rRecvRing, LfRingType &rSendRing);
	virtual ~IPv4(void);

	void AddAddr(unsigned char a, unsigned char b, unsigned char c, unsigned char d);

	virtual void DemuxThreadFunc(void);

	virtual bool EnqueuePacket(const InternetAddrIntf &rDest, int protocol, MaxPacket &rPayload);


protected:
	LfRingType &m_rRecvRing;
	LfRingType &m_rSendRing;

	std::list<unsigned int> m_localAddrs;
	MaxPacket m_sendTempPacket;
	Mutex m_sendMutex;
};

}

#endif /* IPV4_H_ */
