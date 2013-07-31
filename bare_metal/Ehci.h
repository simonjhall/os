/*
 * Ehci.h
 *
 *  Created on: 29 Jul 2013
 *      Author: simon
 */

#ifndef EHCI_H_
#define EHCI_H_

#include "fixed_size_allocator.h"

class Ehci
{
public:
	Ehci(volatile void *pBase);
	virtual ~Ehci();

	void Initialise(void);

protected:
	struct ITD;
	struct QH;
	struct SITD;
	struct FSTN;

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
		FrameListElement(ITD *);
		FrameListElement(QH *);
		FrameListElement(SITD *);
		FrameListElement(FSTN *);

		operator ITD *() const;
		operator QH *() const;
		operator SITD *() const;
		operator FSTN *() const;
	};


	struct ITD
	{
		FrameListElement m_fle;
		unsigned int m_words[15];
	};

	struct QH
	{
		FrameListElement m_fle;
		unsigned int m_words[11];
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
};

#endif /* EHCI_H_ */
