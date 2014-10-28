/*
 * Protocol.h
 *
 *  Created on: 17 Oct 2014
 *      Author: simon
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

namespace Net
{
class InternetAddrIntf;
class MaxPacket;

enum PacketError
{
	kErrorOk, kErrorChecksum, kInternalError,
	kDestNetworkUnreachable,
	kDestHostUnreachable,
	kDestProtocolUnreachable,
	kDestPortUnreachable,
	kFragRequired,
	kSourceRoutefailed,
	kDestNetworkUnk,
	kDestHostUnk,
	kTtlExpired,
	kFragReassExceed,
	kBadLength,
};

class Protocol
{
public:
	virtual ~Protocol() {}

	virtual int DecodesProtocol(void) = 0;
	virtual PacketError DecodeAndDispatch(const InternetAddrIntf &rAddr, MaxPacket &, unsigned int dataSize) = 0;
};

}

#endif /* PROTOCOL_H_ */
