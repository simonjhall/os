/*
 * FatFS.h
 *
 *  Created on: 31 May 2013
 *      Author: simon
 */

#ifndef FatFS_H_
#define FatFS_H_

#include "BlockDevice.h"
#include "common.h"

#include <string.h>

class FatFS
{
public:
	FatFS(BlockDevice &rDevice);
	virtual ~FatFS();

	void ReadBpb(void)
	{
		m_rDevice.ReadDataFromLogicalAddress(0, &m_biosParameterBlock, sizeof(Bpb));
	}

	void ReadEbr(void)
	{
		m_rDevice.ReadDataFromLogicalAddress(36, &m_extendedBootRecord32, sizeof(Ebr32));

		if (m_biosParameterBlock.m_totalSectors == 0)
			m_fatVersion = 32;
//		else if (m_biosParameterBlock.m_totalSectors / m_biosParameterBlock.m_sectorsPerCluster < 4085)
//			m_fatVersion = 12;
		else
			m_fatVersion = 16;
	}

	unsigned int FatSize(void) const
	{
		if (m_fatVersion == 32)
			return SectorToBytes(m_extendedBootRecord32.m_sectorsPerFat);
		else
			return SectorToBytes(m_biosParameterBlock.m_sectorsPerFat);
	}

	unsigned int RootDirectoryAbsCluster(void) const
	{
		if (m_fatVersion == 32)
			return m_biosParameterBlock.m_reservedSectors + m_biosParameterBlock.m_numFats * m_extendedBootRecord32.m_sectorsPerFat;		//div sectors/cluster
		else
			return m_biosParameterBlock.m_reservedSectors + m_biosParameterBlock.m_numFats * m_biosParameterBlock.m_sectorsPerFat;		//div sectors/cluster
	}

	unsigned int RootDirectoryRelCluster(void) const
	{
		return 2;
	}

	unsigned int RelativeToAbsoluteCluster(unsigned int c)
	{
		return c - 2 + RootDirectoryAbsCluster();
	}

	void ReadCluster(void *pDest, unsigned int cluster, unsigned int sizeInBytes)
	{
		m_rDevice.ReadDataFromLogicalAddress(SectorToBytes(ClusterToSector(cluster)), pDest, sizeInBytes);
	}

	void HaveFatMemory(void *pFat)
	{
		m_pFat = (unsigned char *)pFat;
	}

	void LoadFat(void)
	{
		m_rDevice.ReadDataFromLogicalAddress(SectorToBytes(m_biosParameterBlock.m_reservedSectors),
				m_pFat, FatSize());
	}

	bool NextCluster(unsigned int current, unsigned int &rNext)
	{
		switch (m_fatVersion)
		{
		case 32:
			return NextCluster32(current, rNext);
		case 16:
			return NextCluster16(current, rNext);
		case 12:
			return NextCluster12(current, rNext);
		default:
			return false;
		}
	}

	unsigned int CountClusters(unsigned int start)
	{
		unsigned int total = 1;
		while (NextCluster(start, start))
			total++;
		return total;
	}

	inline unsigned int ClusterSizeInBytes(void) const
	{
		return SectorToBytes(ClusterToSector(1));
	}

	void ReadClusterChain(void *pCluster, unsigned int cluster)
	{
		do
		{
			ReadCluster(pCluster, RelativeToAbsoluteCluster(cluster), ClusterSizeInBytes());
			pCluster = (void *)((unsigned char *)pCluster + ClusterSizeInBytes());

		} while (NextCluster(cluster, cluster));
	}

	void ListDirectory(Printer &p, unsigned int cluster, int indent = 0)
	{
		void *pDir = __builtin_alloca(ClusterSizeInBytes() * CountClusters(cluster));
		ReadClusterChain(pDir, cluster);

		unsigned int slot = 0;
		DirEnt dirent;

		bool ok;

		do
		{
			ok = IterateDirectory(pDir, slot, dirent);
			if (ok)
			{
				for (int count = 0; count < indent; count++)
					p.PrintChar(' ');

				p << "file " << dirent.m_name;
				p << " rel cluster " << dirent.m_cluster;
				p << " abs cluster " << RelativeToAbsoluteCluster(dirent.m_cluster);
				p << " size " << dirent.m_size;
				p << " attr " << dirent.m_type;
				p << "\n";

				if (dirent.m_type == DirEnt::kDirectory && dirent.m_cluster != cluster)
					ListDirectory(p, dirent.m_cluster, indent + 1);
			}
		} while (ok);

		p << "\n";
	}

	struct DirEnt
	{
		char m_name[255];
		unsigned int m_cluster;
		unsigned int m_size;

		enum DirEntType
		{
			kReadOnly = 1,
			kHidden = 2,
			kSystem = 4,
			kVolumeId = 8,
			kDirectory = 16,
			kArchive = 32,
		} m_type;
	};

	bool IterateDirectory(void *pCluster, unsigned int &rEntry, DirEnt &rOut, bool reentry = false)
	{
		union {
			FatFS::DirEnt83 *p83;
			FatFS::DirEntLong *pLong;
		} dir;

		dir.p83 = (FatFS::DirEnt83 *)pCluster;

		//does it exist
		if (dir.p83[rEntry].m_name83[0] == 0 || dir.p83[rEntry].m_name83[0] == 0xe5)
		{
			rEntry++;
			return false;
		}

		if (reentry == false)
			memset(rOut.m_name, 0, sizeof(rOut.m_name));

		//is it a long filename
		if (dir.p83[rEntry].m_attributes == 0xf)
		{
			int copyOffset = ((dir.p83[rEntry].m_name83[0] & ~0x40) - 1) * 13;

			for (int count = 0; count < 5; count++)
				rOut.m_name[copyOffset + count] = dir.pLong[rEntry].m_firstFive[count] & 0xff;

			for (int count = 0; count < 6; count++)
				rOut.m_name[copyOffset + count + 5] = dir.pLong[rEntry].m_nextSix[count] & 0xff;

			for (int count = 0; count < 2; count++)
				rOut.m_name[copyOffset + count + 5 + 6] = dir.pLong[rEntry].m_finalTwo[count] & 0xff;

			IterateDirectory(pCluster, ++rEntry, rOut, true);
		}
		else
		{
			//fill in the cluster info
			rOut.m_cluster = dir.p83[rEntry].m_lowCluster | (dir.p83[rEntry].m_highCluster << 16);

			//type
			rOut.m_type = (DirEnt::DirEntType)dir.p83[rEntry].m_attributes;

			//and file size
			rOut.m_size = dir.p83[rEntry].m_fileSize;

			//copy in the name if appropriate
			if (reentry == false)
				memcpy(rOut.m_name, dir.p83[rEntry].m_name83, sizeof(dir.p83[rEntry].m_name83));
			rEntry++;
		}

		return true;
	}

private:
	inline unsigned int SectorToBytes(unsigned int b) const
	{
		return b * m_biosParameterBlock.m_bytesPerSector;
	}

	inline unsigned int ClusterToSector(unsigned int c) const
	{
		return c * m_biosParameterBlock.m_sectorsPerCluster;
	}

	bool NextCluster32(unsigned int current, unsigned int &rNext) const
	{
		unsigned int table_value = ((unsigned int *)m_pFat)[current];
		table_value = table_value & 0xfffffff;

		if (table_value >= 0xffffff7)	//bad or eof
			return false;
		else
		{
			rNext = table_value;
			return true;
		}
	}

	bool NextCluster16(unsigned int current, unsigned int &rNext) const
	{
		unsigned int table_value = ((unsigned int *)m_pFat)[current];

		if (table_value >= 0xfff7)	//bad or eof
			return false;
		else
		{
			rNext = table_value;
			return true;
		}
	}

	bool NextCluster12(unsigned int current, unsigned int &rNext) const
	{
		return false;
	}

	BlockDevice &m_rDevice;

#pragma pack(push)
#pragma pack(1)
	struct Bpb
	{
		unsigned char m_jmpShort3cNop[3];
		char m_oemIdentifier[8];
		unsigned short m_bytesPerSector;
		unsigned char m_sectorsPerCluster;
		unsigned short m_reservedSectors;
		unsigned char m_numFats;
		unsigned short m_numDirectoryEntries;
		unsigned short m_totalSectors;
		unsigned char m_mediaDescriptorType;
		unsigned short m_sectorsPerFat;
		unsigned short m_sectorsPerTrack;
		unsigned short m_numHeads;
		unsigned int m_hiddenSectors;
		unsigned int m_sectorsOnMedia;
	} m_biosParameterBlock;

	struct Ebr1216
	{
		unsigned char m_driveNumber;
		unsigned char m_flags;
		unsigned char m_signature;
		unsigned int m_volumeId;
		char m_volumeLabel[11];
		char m_systemIdentifier[8];
	};
	struct Ebr32
	{
		unsigned int m_sectorsPerFat;
		unsigned short m_flags;
		unsigned short m_fatVersion;
		unsigned int m_rootDirectoryCluster;
		unsigned short m_fsinfoCluster;
		unsigned short m_backupBootSectorCluster;
		unsigned char m_reserved[12];
		unsigned char m_driveNumber;
		unsigned char m_moreFlags;
		unsigned char m_signature;
		unsigned int m_volumeId;
		char m_volumeLabel[11];
		char m_systemIdentifier[8];
	};

	union {
		Ebr1216 m_extendedBootRecord1216;
		Ebr32 m_extendedBootRecord32;
	};

	struct DirEnt83
	{
		char m_name83[11];
		unsigned char m_attributes;
		unsigned char m_reserved;
		unsigned char m_createTimeTenths;
		unsigned short m_createTime;
		unsigned short m_createDate;
		unsigned short m_lastAccessedDate;
		unsigned short m_highCluster;
		unsigned short m_lastModifiedTime;
		unsigned short m_lastModifiedDate;
		unsigned short m_lowCluster;
		unsigned int m_fileSize;
	};

	struct DirEntLong
	{
		unsigned char m_order;
		unsigned short m_firstFive[5];
		unsigned char m_attribute;
		unsigned char m_type;
		unsigned char m_checksum;
		unsigned short m_nextSix[6];
		unsigned char m_zero[2];
		unsigned short m_finalTwo[2];
	};

	int m_fatVersion;
	unsigned char *m_pFat;

#pragma pack(pop)
};

#endif /* FatFS_H_ */
