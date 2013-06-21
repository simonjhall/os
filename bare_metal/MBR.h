/*
 * MBR.h
 *
 *  Created on: 21 Jun 2013
 *      Author: simon
 */

#ifndef MBR_H_
#define MBR_H_

#include "BlockDevice.h"

class MBR
{
public:
	MBR(BlockDevice &rRoot);
	virtual ~MBR();

	virtual unsigned int GetNumPartitions(void);
	virtual BlockDevice *GetPartition(unsigned int);

private:
	#pragma pack(push)
#pragma pack(1)
	struct PTE
	{
		unsigned m_bootable :8;
		unsigned m_startHead :8;
		unsigned m_startSector :6;
		unsigned m_startCylinder :10;
		unsigned m_systemId :8;
		unsigned m_endHead :8;
		unsigned m_endSector :6;
		unsigned m_endCylinder :10;

		unsigned m_relativeSectorA :8;
		unsigned m_relativeSectorB :8;
		unsigned m_relativeSectorC :8;
		unsigned m_relativeSectorD :8;

		unsigned m_totalSectorsA :8;
		unsigned m_totalSectorsB :8;
		unsigned m_totalSectorsC :8;
		unsigned m_totalSectorsD :8;
	} __attribute__((aligned(1)));
#pragma pack(pop)

	static const unsigned int sm_partitionOffsets[4];

	class WrappedBlockDevice : public BlockDevice
	{
	public:
		WrappedBlockDevice()
		: BlockDevice(),
		  m_pRoot(0),
		  m_offset(0)
		{
		}

		void Init(BlockDevice &rRoot, unsigned int offset)
		{
		  m_pRoot = &rRoot;
		  m_offset = offset;
		}

		virtual ~WrappedBlockDevice()
		{
			m_pRoot = 0;
		}

		virtual bool ReadDataFromLogicalAddress(unsigned int address, void *pDest, unsigned int byteCount)
		{
			if (!m_pRoot)
				return false;

			return m_pRoot->ReadDataFromLogicalAddress(address + m_offset, pDest, byteCount);
		}

	private:
		BlockDevice *m_pRoot;
		unsigned int m_offset;
	};

	BlockDevice &m_rRoot;
	WrappedBlockDevice m_partitions[4];
	unsigned int m_numPartitions;
};

#endif /* MBR_H_ */
