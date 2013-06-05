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
#include "BaseFS.h"

#include <string.h>
#include <vector>

class FatFS;

class FatDirectory : public Directory
{
public:
	FatDirectory(const char *pName, FatDirectory *pParent, BaseFS &fileSystem,
			unsigned int cluster)
	: Directory(pName, pParent, fileSystem),
	  m_cluster(cluster)
	{
	};

	virtual bool AddChild(BaseDirent *p)
	{
		m_files.push_back(p);
		return true;
	}

	virtual unsigned int GetNumChildren(void)
	{
		return m_files.size();
	}

	virtual BaseDirent *GetChild(unsigned int a)
	{
		if (a < GetNumChildren())
			return m_files[a];
		else
			return 0;
	}

	unsigned int GetStartCluster(void)
	{
		return m_cluster;
	}

protected:
	std::vector<BaseDirent *> m_files;
	unsigned int m_cluster;
};

class FatFile : public File
{
public:
	FatFile(const char *pName, Directory *pParent, BaseFS &fileSystem,
			unsigned int cluster, unsigned int size)
	: File(pName, pParent, fileSystem),
	  m_cluster(cluster),
	  m_size(size)
	{
	};

	virtual ssize_t ReadFrom(void *pBuf, size_t count, off_t offset);
	virtual ssize_t WriteTo(const void *pBuf, size_t count, off_t offset);
	virtual bool Seekable(off_t);

protected:
	unsigned int m_cluster;
	unsigned int m_size;
};

class FatFS : public BaseFS
{
	friend class FatFile;
public:
	FatFS(BlockDevice &rDevice);
	virtual ~FatFS();

	virtual BaseDirent *OpenByHandle(BaseDirent &rFile, unsigned int flags);
	virtual bool Close(BaseDirent &);

	virtual bool Stat(const char *pFilename, struct stat &rBuf);
	virtual bool Lstat(const char *pFilename, struct stat &rBuf);

	virtual bool Mkdir(const char *pFilePath, const char *pFilename);
	virtual bool Rmdir(const char *pFilename);

protected:
	virtual BaseDirent *Locate(const char *pFilename, Directory *pParent = 0);

protected:
	struct FATDirEnt;

	//init
	void ReadBpb(void);
	void ReadEbr(void);
	void HaveFatMemory(void *pFat);
	void LoadFat(void);

	//data reading
	void ReadCluster(void *pDest, unsigned int cluster, unsigned int sizeInBytes);
	unsigned int CountClusters(unsigned int start);
	void ReadClusterChain(void *pCluster, unsigned int cluster);

	//directory walking
	void BuildDirectoryStructure(void);
	void ListDirectory(FatDirectory &rDir);
	bool IterateDirectory(void *pCluster, unsigned int &rEntry, FATDirEnt &rOut, bool reentry = false);

	//translation
	inline unsigned int SectorToBytes(unsigned int b) const
	{
		return b * m_biosParameterBlock.m_bytesPerSector;
	}

	inline unsigned int ClusterToSector(unsigned int c) const
	{
		return c * m_biosParameterBlock.m_sectorsPerCluster;
	}

	inline unsigned int RelativeToAbsoluteCluster(unsigned int c)
	{
		return c - 2 + RootDirectoryAbsCluster();
	}

	//accessors

	inline unsigned int ClusterSizeInBytes(void) const
	{
		return SectorToBytes(ClusterToSector(1));
	}

	unsigned int FatSize(void) const
	{
		if (m_fatVersion == 32)
			return SectorToBytes(m_extendedBootRecord32.m_sectorsPerFat);
		else
			return SectorToBytes(m_biosParameterBlock.m_sectorsPerFat);
	}

	inline unsigned int RootDirectoryAbsCluster(void) const
	{
		if (m_fatVersion == 32)
			return m_biosParameterBlock.m_reservedSectors + m_biosParameterBlock.m_numFats * m_extendedBootRecord32.m_sectorsPerFat;		//div sectors/cluster
		else
			return m_biosParameterBlock.m_reservedSectors + m_biosParameterBlock.m_numFats * m_biosParameterBlock.m_sectorsPerFat;		//div sectors/cluster
	}

	inline unsigned int RootDirectoryRelCluster(void) const
	{
		return 2;
	}

	//walking clusters
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

	bool NextCluster32(unsigned int current, unsigned int &rNext) const;
	bool NextCluster16(unsigned int current, unsigned int &rNext) const;
	bool NextCluster12(unsigned int current, unsigned int &rNext) const;

	BlockDevice &m_rDevice;
	int m_fatVersion;
	unsigned char *m_pFat;

	struct FATDirEnt
	{
		char m_name[255];
		unsigned int m_cluster;
		unsigned int m_size;

		enum FATDirEntType
		{
			kReadOnly = 1,
			kHidden = 2,
			kSystem = 4,
			kVolumeId = 8,
			kDirectory = 16,
			kArchive = 32,
		} m_type;
	};

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

#pragma pack(pop)

private:
	FatDirectory *m_pRoot;
};

#endif /* FatFS_H_ */
