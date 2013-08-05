/*
 * Ehci.h
 *
 *  Created on: 29 Jul 2013
 *      Author: simon
 */

#ifndef EHCI_H_
#define EHCI_H_

#include "fixed_size_allocator.h"

namespace USB
{

enum Pid
{
	//token
	kOut = 1,
	kIn = 9,
	kSof = 5,
	kSetup = 13,
	//data
	kData0 = 3,
	kData1 = 11,
	kData2 = 7,
	kMdata = 15,
	//handshake
	kAck = 2,
	kNak = 10,
	kStall = 14,
	kNyet = 6,
	//special
	kPre = 12,
	kErr = 12,
	kSplit = 8,
	kPing = 4,
};

enum DescriptorType
{
	kDevice = 1,
	kConfiguration = 2,
	kInterface = 4,
	kEndpoint = 5,
};

#pragma pack(push)
#pragma pack(1)
struct SetupPacket
{
	SetupPacket(unsigned char requestType, unsigned char request,
			unsigned short value, unsigned short index, unsigned short length)
	: m_requestType(requestType),
	  m_request(request),
	  m_value(value),
	  m_index(index),
	  m_length(length)
	{
	}

	unsigned char m_requestType;
	unsigned char m_request;
	unsigned short m_value;
	unsigned short m_index;
	unsigned short m_length;

};

struct DeviceDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned short m_usbVersion;
	unsigned char m_deviceClass;
	unsigned char m_deviceSubClass;
	unsigned char m_deviceProtocol;
	unsigned char m_maxPacketSize0;
	unsigned short m_idVendor;
	unsigned short m_idProduct;
	unsigned short m_devVersion;
	unsigned char m_manufacturerIndex;
	unsigned char m_productIndex;
	unsigned char m_serialIndex;
	unsigned char m_numConfigurations;
};

struct ConfigurationDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned short m_totalLength;
	unsigned char m_numInterfaces;
	unsigned char m_configurationValue;
	unsigned char m_configurationIndex;
	unsigned char m_attributes;
	unsigned char m_maxPower;
};

struct InterfaceDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned char m_interfaceNumber;
	unsigned char m_alternateSetting;
	unsigned char m_numEndpoints;
	unsigned char m_interfaceClass;
	unsigned char m_interfaceSubclass;
	unsigned char m_interfaceProtocol;
	unsigned char m_interfaceIndex;
};

struct EndpointDescriptor
{
	unsigned char m_length;
	unsigned char m_descriptorType;
	unsigned char m_endpointAddress;
	unsigned char m_attributes;
	unsigned short m_maxPacketSize;
	unsigned char m_interval;
};

#pragma pack(pop)

enum Speed
{
	kLowSpeed,
	kFullSpeed,
	kHighSpeed,
};

struct ITD;
struct QH;
struct SITD;
struct FSTN;

struct QTD;

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

#pragma pack(push)
#pragma pack(1)
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

class Ehci
{
public:
	Ehci(volatile void *pBase);
	virtual ~Ehci();

	void Initialise(void);

	void EnableAsync(bool e);
	void EnablePeriodic(bool e);

	void PortReset(unsigned int p);

	void GetDescriptor(void *p, DescriptorType t, unsigned int index);

protected:

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
};

}

#endif /* EHCI_H_ */
