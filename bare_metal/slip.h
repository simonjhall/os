/*
 * slip.h
 *
 *  Created on: 25 Sep 2014
 *      Author: simon
 */

#ifndef SLIP_H_
#define SLIP_H_

#include "UART.h"
#include "LinkLayer.h"
#include "Mailbox.h"
#include "InterruptController.h"
#include "fixed_size_allocator.h"

namespace Slip
{

void send_packet(char *p, int len, GenericByteTxRx &);
int recv_packet(char *p, int len, GenericByteTxRx &);

template <class Type, class Len>
class Rpc
{
public:
	virtual Type *Alloc(Len size) = 0;
	virtual void Free(Type *) = 0;

	virtual void NotifyNew(Type *pPacket, Len length) = 0;
	virtual bool Transmit(Type **ppPacket, Len *pLength) = 0;
	virtual void SendAck(Type *pPacket) = 0;
	virtual bool RecvAck(Type **ppPacket) = 0;

	virtual bool ShouldQuit(void) = 0;
};

void HalfDuplex(GenericByteTxRx &, Rpc<unsigned char, unsigned int> &);
void FullDuplex(GenericByteTxRx &, Rpc<unsigned char, unsigned int> &);

//////////////////////

class SlipLinkAddr : public LinkAddrIntf
{
public:
	inline SlipLinkAddr(int a = -1)
	: LinkAddrIntf(),
	  m_addr(a)
	{
	}
protected:
	int m_addr;
};

class SlipLink : public LinkIntf
{
public:
	virtual ~SlipLink();

	inline virtual int GetMtu()
	{
		return kMtu;
	}

	//todo
	inline virtual LinkAddrIntf *GetAddr(void) { return &m_thisAddr; }
	inline virtual LinkAddrIntf *GetPtpAddr(void) { return &m_otherAddr; }

protected:
	SlipLink();
	static const int kMtu = 576;

	SlipLinkAddr m_thisAddr;
	SlipLinkAddr m_otherAddr;
};

class MailboxSlipLink : public SlipLink
{
public:
	MailboxSlipLink(InterruptController *pPic);
	virtual ~MailboxSlipLink();

	virtual bool SendTo(LinkAddrIntf &rDest, void *pData, unsigned int len);
	virtual bool Recv(LinkAddrIntf **ppFrom, void *pData, unsigned int &rLen, unsigned int maxLen);

protected:
	OMAP4460::Mailbox m_mailbox;

	OMAP4460::Mailbox::Queue &m_rAddrSend;
	OMAP4460::Mailbox::Queue &m_rLenSend;
	OMAP4460::Mailbox::Queue &m_rAddrRecv;
	OMAP4460::Mailbox::Queue &m_rLenRecv;
	OMAP4460::Mailbox::Queue &m_rSendAck;
	OMAP4460::Mailbox::Queue &m_rRecvAck;

	struct Packet
	{
		char buf[1500];
	};

	FixedSizeAllocator<Packet, 15000 / sizeof(Packet)> m_packetAllocator;
};

class SynchronousSlipLink : public SlipLink
{
public:
	SynchronousSlipLink(GenericByteTxRx &rTxRx);
	virtual ~SynchronousSlipLink();

	virtual bool SendTo(LinkAddrIntf &rDest, void *pData, unsigned int len);
	virtual bool Recv(LinkAddrIntf **ppFrom, void *pData, unsigned int &rLen, unsigned int maxLen);

protected:
	GenericByteTxRx &m_rTxRx;
};

}

#endif /* SLIP_H_ */
