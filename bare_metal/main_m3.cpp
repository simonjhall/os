/*
 * main_m3.cpp
 *
 *  Created on: 20 Sep 2014
 *      Author: simon
 */

#include "UART.h"
#include "print.h"
#include "virt_memory.h"
#include "M3Control.h"
#include "IoSpace.h"
#include "Mailbox.h"
#include "slip.h"
#include "fixed_size_allocator.h"
#include "SemiHosting.h"

mspace s_uncachedPool;

template <class Type, class Len>
class MailboxRpc : public Slip::Rpc<Type, Len>
{
public:
	//0 = recv packet addr
	//1 = recv packet length
	//2 = send packet addr
	//3 = send packet length
	//4 = recv packet ack
	//5 = send packet ack
	MailboxRpc(bool disableInterrupts)
	: m_mailbox(IoSpace::GetDefaultIoSpace()->Get("Mailbox"), disableInterrupts),
	  m_rAddrSend(m_mailbox.GetQueue(0)),
	  m_rLenSend(m_mailbox.GetQueue(1)),
	  m_rAddrRecv(m_mailbox.GetQueue(2)),
	  m_rLenRecv(m_mailbox.GetQueue(3)),
	  m_rSendAck(m_mailbox.GetQueue(5)),
	  m_rRecvAck(m_mailbox.GetQueue(4))
	{
		m_packetAllocator.Init((Packet *)mspace_malloc(s_uncachedPool, 15000));
	}

	virtual ~MailboxRpc()
	{
	}

	virtual Type *Alloc(Len size)
	{
		Packet *p;
		if (!m_packetAllocator.Allocate(&p))
			return 0;

		return (Type *)p;
	}

	virtual void Free(Type *pAddr)
	{
		if (!m_packetAllocator.Free((Packet *)pAddr))
			ASSERT(0);
	}

	virtual void NotifyNew(Type *pPacket, Len length)
	{
		m_rAddrSend.WriteMessage((unsigned int)pPacket - 0xa0000000);
		m_rLenSend.WriteMessage(length);
	}

	virtual bool Transmit(Type **ppPacket, Len *pLength)
	{
		if (m_rLenRecv.NumUnreadMessages())
		{
			*ppPacket = (Type *)(m_rAddrRecv.ReadMessage() + 0xa0000000);
			*pLength = m_rLenRecv.ReadMessage();

			return true;
		}
		else
			return false;
	}

	virtual void SendAck(Type *pPacket)
	{
		m_rSendAck.WriteMessage((unsigned int)pPacket - 0xa0000000);
	}

	virtual bool RecvAck(Type **ppPacket)
	{
		if (m_rRecvAck.NumUnreadMessages())
		{
			*ppPacket = (Type *)(m_rRecvAck.ReadMessage() + 0xa0000000);
			return true;
		}
		else
			return false;
	}

	virtual bool ShouldQuit(void)
	{
		return false;
	}
private:
	OMAP4460::Mailbox m_mailbox;

	struct Packet
	{
		char buf[1500];
	};

	FixedSizeAllocator<Packet, 15000 / sizeof(Packet)> m_packetAllocator;

	OMAP4460::Mailbox::Queue &m_rAddrSend;
	OMAP4460::Mailbox::Queue &m_rLenSend;
	OMAP4460::Mailbox::Queue &m_rAddrRecv;
	OMAP4460::Mailbox::Queue &m_rLenRecv;
	OMAP4460::Mailbox::Queue &m_rSendAck;
	OMAP4460::Mailbox::Queue &m_rRecvAck;
};

inline void PrintString(const char *pString)
{
	OMAP4460::UART uart3((volatile unsigned int *)0x48020000);

	uart3 << pString;
}

extern "C" unsigned int FuncTest(void);
extern "C" void TimingTest(void);
extern "C" unsigned int TimingTestIssue(void);

Printer *Printer::sm_defaultPrinter = 0;

extern "C" void M3_Reset(void)
{
//	volatile bool pause = true;
//	while (pause);

	OMAP4460::UART uart3((volatile unsigned int *)0x48020000);
	Printer::sm_defaultPrinter = &uart3;

	uart3 << "hello from cortex-m3\n";
	uart3 << "built at " << __DATE__ << " time " << __TIME__ << "\n";

	auto *pVtor = (unsigned int *)0xe000ed08;
	auto *pShcsr = (unsigned int *)0xe000ed24;

	//change the vector table to be not zero
	*pVtor = 0x24000000;

	//listen for different faults
	*pShcsr = (1 << 18) | (1 << 17) | (1 << 16);

	//clear the zero page
	auto *pL1TablePhys = OMAP4460::M3::GetL1TablePhys(false);

	pL1TablePhys->fault.Init();
	OMAP4460::M3::FlushTlb(false);

	asm ("dsb");
	asm ("isb");

	//cached, default
	if (!InitMempool((void *)0x60000000, 256 * 1, true))		//1MB
		ASSERT(0);

	//uncached
	if (!InitMempool((void *)0xa0000000, 128 * 1, true, &s_uncachedPool))		//512KB
		ASSERT(0);

	OMAP4460::OmapIoSpace iospace;

	IoSpace::SetDefaultIoSpace(&iospace);
	IoSpace::GetDefaultIoSpace()->MapVirtAsPhys();

	MailboxRpc<unsigned char, unsigned int> rpc(false);
	Slip::FullDuplex(uart3, rpc);
//	Slip::HalfDuplex(uart3, rpc);

	while(1);
}

extern "C" void M3_NMI(void)
{
	PrintString("NMI");
	while(1);
}

extern "C" void M3_HardFault(void)
{
	PrintString("HardFault");
	while(1);
}

extern "C" void M3_MemManage(void)
{
	PrintString("MemManage");
	while(1);
}

extern "C" void M3_BusFault(unsigned int sp)
{
	OMAP4460::UART uart3((volatile unsigned int *)0x48020000);
	unsigned int *cfsr = (unsigned int *)0xe000ed28;
	unsigned int *bfar = (unsigned int *)0xe000ed38;
	uart3 << "BusFault; cfsr " << *cfsr << "; sp " << sp << "; bfar " << *bfar << "\n";

	for (int count = -10; count < 10; count++)
		uart3 << (sp + count * 4) << ": " << *(unsigned int *)(sp + count * 4) << "\n";

	unsigned int r0 = ((unsigned int *)sp)[0];
	unsigned int r1 = ((unsigned int *)sp)[1];
	unsigned int r2 = ((unsigned int *)sp)[2];
	unsigned int r3 = ((unsigned int *)sp)[3];
	unsigned int r12 = ((unsigned int *)sp)[4];
	unsigned int r14 = ((unsigned int *)sp)[5];
	unsigned int ret = ((unsigned int *)sp)[6];
	unsigned int xpsr = ((unsigned int *)sp)[7];

	uart3 << "r0  " << r0 << "\n";
	uart3 << "r1  " << r1 << "\n";
	uart3 << "r2  " << r2 << "\n";
	uart3 << "r3  " << r3 << "\n";
	uart3 << "r12 " << r12 << "\n";
	uart3 << "r14 " << r14 << "\n";
	uart3 << "pc  " << ret << "\n";
	uart3 << "psr " << xpsr << "\n";

	ret = ret & ~3;

	for (int count = -10; count < 10; count++)
		uart3 << (ret + count * 4) << ": " << *(unsigned int *)(ret + count * 4) << "\n";

	while(1);
}

extern "C" void M3_UsageFault(unsigned int sp)
{
	OMAP4460::UART uart3((volatile unsigned int *)0x48020000);
	unsigned int *cfsr = (unsigned int *)0xe000ed28;
	uart3 << "UsageFault; cfsr " << *cfsr << "; sp " << sp << "\n";

	for (int count = -10; count < 10; count++)
		uart3 << (sp + count * 4) << ": " << *(unsigned int *)(sp + count * 4) << "\n";

	unsigned int r0 = ((unsigned int *)sp)[0];
	unsigned int r1 = ((unsigned int *)sp)[1];
	unsigned int r2 = ((unsigned int *)sp)[2];
	unsigned int r3 = ((unsigned int *)sp)[3];
	unsigned int r12 = ((unsigned int *)sp)[4];
	unsigned int r14 = ((unsigned int *)sp)[5];
	unsigned int ret = ((unsigned int *)sp)[6];
	unsigned int xpsr = ((unsigned int *)sp)[7];

	uart3 << "r0  " << r0 << "\n";
	uart3 << "r1  " << r1 << "\n";
	uart3 << "r2  " << r2 << "\n";
	uart3 << "r3  " << r3 << "\n";
	uart3 << "r12 " << r12 << "\n";
	uart3 << "r14 " << r14 << "\n";
	uart3 << "pc  " << ret << "\n";
	uart3 << "psr " << xpsr << "\n";

	ret = ret & ~3;

	for (int count = -10; count < 10; count++)
		uart3 << (ret + count * 4) << ": " << *(unsigned int *)(ret + count * 4) << "\n";

	while(1);
}

extern "C" void M3_SVCall(void)
{
	PrintString("SVCall");
	while(1);
}

extern "C" void M3_DebugMonitor(void)
{
	PrintString("DebugMonitor");
	while(1);
}

extern "C" void M3_PendSV(void)
{
	PrintString("PendSV");
	while(1);
}

extern "C" void M3_SysTick(void)
{
	PrintString("SysTick");
	while(1);
}

extern "C" void M3_ExtIntN(void)
{
	PrintString("external interrupt");
	while(1);
}

