/*
 * MBR.cpp
 *
 *  Created on: 21 Jun 2013
 *      Author: simon
 */

#include "MBR.h"
#include "print_uart.h"
#include "common.h"

const unsigned int MBR::sm_partitionOffsets[4] = {0x1be, 0x1ce, 0x1de, 0x1ee};

MBR::MBR(BlockDevice &rRoot)
: m_rRoot(rRoot), m_numPartitions(0)
{
	PrinterUart<PL011> p;

	unsigned char head[512];
	bool ok = m_rRoot.ReadDataFromLogicalAddress(0, head, 512);
	ASSERT(ok);

	for (unsigned int count = 0; count < 4; count++)
	{
		PTE *partition = (PTE *)&head[sm_partitionOffsets[count]];

		if (partition->m_systemId)
		{
			p << "partition " << count << "\n";

			unsigned int relativeSector = partition->m_relativeSectorA | (partition->m_relativeSectorB << 8)
					| (partition->m_relativeSectorC << 16) | (partition->m_relativeSectorD << 24);

			unsigned int totalSectors = partition->m_totalSectorsA | (partition->m_totalSectorsB << 8)
					| (partition->m_totalSectorsC << 16) | (partition->m_totalSectorsD << 24);

			p << partition->m_bootable
					<< " "
					<< partition->m_startHead
					<< " "
					<< partition->m_startSector
					<< " "
					<< partition->m_startCylinder
					<< " "
					<< partition->m_systemId
					<< " "
					<< partition->m_endHead
					<< " "
					<< partition->m_endSector
					<< " "
					<< partition->m_endCylinder
					<< " "
					<< relativeSector
					<< " "
					<< totalSectors
					<< "\n";
			p << "\n";

			m_partitions[m_numPartitions].Init(m_rRoot, relativeSector * 512);
			m_numPartitions++;
		}
	}

	p << "there are " << m_numPartitions << " partitions\n";
}

MBR::~MBR()
{

}

unsigned int MBR::GetNumPartitions(void)
{
	return m_numPartitions;
}

BlockDevice *MBR::GetPartition(unsigned int p)
{
	if (p < m_numPartitions)
		return &m_partitions[p];
	else
		return 0;
}
