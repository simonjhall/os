/*
 * Ehci.h
 *
 *  Created on: 29 Jul 2013
 *      Author: simon
 */

#ifndef EHCI_H_
#define EHCI_H_

#include "fixed_size_allocator.h"

#include "Usb.h"
#include "EhciRootHub.h"
#include "Hcd.h"

namespace USB
{

enum DescriptorType
{
	kDevice = 1,
	kConfiguration = 2,
	kInterface = 4,
	kEndpoint = 5,
};


struct ITD;
struct QH;
struct SITD;
struct FSTN;

struct QTD;

#pragma pack(push)
#pragma pack(1)

struct FrameListElement
{
	enum Type
	{
		kIsochronousTransferDescriptor = 0,
		kQueueHead = 1,
		kSplitTransationIsochronousTransferDescriptor = 2,
		kFrameSpanTraversalNode = 3,
	};

	unsigned int m_word;

	FrameListElement(void);
	FrameListElement(void *, Type);
};


struct ITD
{
	FrameListElement m_fle;
	unsigned int m_words[15];
};

struct SITD
{
	FrameListElement m_fle;
	unsigned int m_words[6];
};

struct FSTN
{
	FrameListElement m_nple;
	FrameListElement m_bple;
};

struct QTD
{
	QTD(void);
	QTD(QTD *pVirtNext, bool nextTerm,
			QTD *pVirtAltNext, bool altNextTerm,
			bool dataToggle,
			unsigned int totalBytes,
			bool interruptOnComplete,
			unsigned int currentPage,
			unsigned int errorCounter,
			Pid pid,
			unsigned int status,
			void *pVirtBuffer);
	void Init(QTD *pVirtNext, bool nextTerm,
			QTD *pVirtAltNext, bool altNextTerm,
			bool dataToggle,
			unsigned int totalBytes,
			bool interruptOnComplete,
			unsigned int currentPage,
			unsigned int errorCounter,
			Pid pid,
			unsigned int status,
			void *pVirtBuffer);

	unsigned int GetBytesToTransfer(void);
	unsigned int GetErrorCount(void);
	unsigned int GetStatus(void);

	volatile unsigned int m_words[8];
};

struct QH
{
	QH(FrameListElement link,
			unsigned int nakReload, bool controlEndpoint, unsigned int maxPacketLength,
			bool headOfList, bool dataToggleControl, Speed eps, unsigned int endPt,
			bool inactiveOnNext, unsigned int deviceAddr,
			unsigned int multiplier, unsigned int portNumber, unsigned int hubAddr,
			unsigned int splitCompletionMask, unsigned int interruptSchedMask,
			volatile QTD *pCurrent);
	void Init(FrameListElement link,
			unsigned int nakReload, bool controlEndpoint, unsigned int maxPacketLength,
			bool headOfList, bool dataToggleControl, Speed eps, unsigned int endPt,
			bool inactiveOnNext, unsigned int deviceAddr,
			unsigned int multiplier, unsigned int portNumber, unsigned int hubAddr,
			unsigned int splitCompletionMask, unsigned int interruptSchedMask,
			volatile QTD *pCurrent);

	FrameListElement m_fle;
	unsigned int m_words[2];
	volatile QTD *m_pCurrent;
	volatile QTD m_overlay;
};
#pragma pack(pop)

///////////////////////////////////////////////

class Ehci : public Hcd
{
	friend class EhciRootHub;
public:
	Ehci(volatile void *pBase);
	virtual ~Ehci();

	virtual bool Initialise(void);
	virtual void Shutdown(void);

	virtual bool SubmitControlMessage(EndPoint &rEndPoint, UsbDevice &rDevice, void *pBuffer, unsigned int length, SetupPacket);

	virtual Hub &GetRootHub(void);

protected:

	void EnableAsync(bool e);
	void EnablePeriodic(bool e);

	bool IsPortPowered(unsigned int p);
	bool IsDeviceAttached(unsigned int p);
	void PortPower(unsigned int p, bool o);
	void PortReset(unsigned int p);

//	bool GetDescriptor(void *p, DescriptorType t, unsigned int index);
//	void SetAddress(unsigned int addr);
//	void SetConfiguration(unsigned int addr, unsigned int conf);

#pragma pack(push)
#pragma pack(1)
	struct Caps
	{
		volatile unsigned char m_capLength;
		volatile unsigned char m_reserved;
		volatile unsigned short m_hciVersion;
		volatile unsigned int m_hcsParams;
		volatile unsigned int m_hccParams;
		volatile unsigned long long m_hcspPortRoute;
	};

	struct Ops
	{
		volatile unsigned int m_usbCmd;
		volatile unsigned int m_usbSts;
		volatile unsigned int m_usbIntr;
		volatile unsigned int m_frIndex;
		volatile unsigned int m_ctrlDsSegment;
		volatile FrameListElement *m_periodicListBase;			//phys
		volatile unsigned int m_asyncListAddr;
		volatile unsigned int m_reserved[0x24 >> 2];
		volatile unsigned int m_configFlag;
		volatile unsigned int m_portsSc[];
	};
#pragma pack(pop)

	volatile Caps *m_pCaps;
	volatile Ops *m_pOps;

	FrameListElement *m_pPeriodicFrameList;

	FixedSizeAllocator<ITD, 4096 / sizeof(ITD)> m_itdAllocator;
	FixedSizeAllocator<QH, 4096 / sizeof(QH)> m_qhAllocator;
	FixedSizeAllocator<SITD, 4096 / sizeof(SITD)> m_sitdAllocator;
	FixedSizeAllocator<FSTN, 4096 / sizeof(FSTN)> m_fstnAllocator;

	FixedSizeAllocator<QTD, 4096 / sizeof(QTD)> m_qtdAllocator;

	EhciRootHub m_rootHub;
};

}

#endif /* EHCI_H_ */
