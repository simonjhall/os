/*
 * FatFS.cpp
 *
 *  Created on: 31 May 2013
 *      Author: simon
 */

#include "FatFS.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

FatFS::FatFS(BlockDevice &rDevice)
: m_rDevice (rDevice)
{
	ReadBpb();
	ReadEbr();

	Printer &p = Printer::Get();
	p << "bpb and ebr read\n";

	p << "BPB\n";
	p << "\t" << m_biosParameterBlock.m_oemIdentifier << "\n";
	p << "\tbytes per sector " << m_biosParameterBlock.m_bytesPerSector << "\n";
	p << "\tsectors per cluster " << m_biosParameterBlock.m_sectorsPerCluster << "\n";
	p << "\treserved sectors " << m_biosParameterBlock.m_reservedSectors << "\n";
	p << "\tnum fats " << m_biosParameterBlock.m_numFats << "\n";

	unsigned int fatSize = FatSize();
	p << "fat size is " << fatSize << "\n";

	void *pFat = (void *)new char[fatSize];
	ASSERT(pFat);

	HaveFatMemory(pFat);
	LoadFat();

	p << "fat loaded\n";

	//insert the root directory
	m_pRoot = new FatDirectory("", 0, *this, RootDirectoryRelCluster());

	BuildDirectoryStructure();

	p << "directory structure built\n";
}

FatFS::~FatFS()
{
	delete m_pRoot;
	// TODO Auto-generated destructor stub
}

/////////////main interface

BaseDirent *FatFS::OpenByHandle(BaseDirent &rFile, unsigned int flags)
{
	if ((flags & O_CREAT) || ((flags & O_ACCMODE) == O_WRONLY) || ((flags & O_ACCMODE) == O_RDWR))
		return 0;

	if (((flags & O_ACCMODE) == O_RDONLY) && rFile.LockRead())
		return &rFile;
	else
		return 0;
}

bool FatFS::Close(BaseDirent &f)
{
	if (&f.GetFilesystem() == this)
	{
		f.Unlock();
		return true;
	}
	else
		return f.GetFilesystem().Close(f);
}

bool FatFS::Stat(const char *pFilename, struct stat &rBuf)
{
	return false;
}

bool FatFS::Lstat(const char *pFilename, struct stat &rBuf)
{
	return false;
}

bool FatFS::Mkdir(const char *pFilePath, const char *pFilename)
{
	return false;
}

bool FatFS::Rmdir(const char *pFilename)
{
	return false;
}

Directory &FatFS::GetRootDirectory(void)
{
	ASSERT(m_pRoot);
	return *m_pRoot;
}

//stuff done on opened files
ssize_t FatFile::ReadFrom(void *pBuf, size_t count, off_t offset)
{
	size_t initial_count = count;

	//todo double-check it's the FS we think it is
	FatFS *fatfs = (FatFS *)&m_rFileSystem;
	//figure out how many clusters in the offset is
	unsigned int cluster_offset = offset / fatfs->ClusterSizeInBytes();
	unsigned int start_cluster = m_cluster;

	for (unsigned int i = 0; i < cluster_offset; i++)
		if (!fatfs->NextCluster(start_cluster, start_cluster))
			return -EINVAL;

	//we should now have the correct starting cluster
	//se how many bytes to omit
	cluster_offset = offset % fatfs->ClusterSizeInBytes();

	unsigned char *pOutBuf = (unsigned char *)pBuf;

	do
	{
		//compute the starting address in logical bytes
		unsigned int start_pos = fatfs->SectorToBytes(fatfs->ClusterToSector(fatfs->RelativeToAbsoluteCluster(start_cluster))) + cluster_offset;
		//compute amount of bytes to read from this cluster
		unsigned int max_read_bytes = fatfs->ClusterSizeInBytes() - cluster_offset;
		unsigned int read_bytes = count > max_read_bytes ? max_read_bytes : count;

		if (!fatfs->m_rDevice.ReadDataFromLogicalAddress(start_pos, pOutBuf, read_bytes))
			return -EIO;

		//we will now be aligned to the beginning of the cluster
		cluster_offset = 0;
		count -= read_bytes;

		//move up the data buffer
		pOutBuf += read_bytes;

		//next cluster if appropriate
		if (count)
			if (!fatfs->NextCluster(start_cluster, start_cluster))
				return -EINVAL;
	} while (count);


	return initial_count;
}

ssize_t FatFile::WriteTo(const void *pBuf, size_t count, off_t offset)
{
	return 0;
}

bool FatFile::Seekable(off_t o)
{
	if (o >=0 && o < m_size)
		return true;
	else
		return false;
}

bool FatFile::Fstat(struct stat64 &rBuf)
{
	memset(&rBuf, 0, sizeof(rBuf));

	rBuf.st_dev = (dev_t)&m_rFileSystem;
	rBuf.st_ino = (ino_t)this;

	if (m_directory)
		rBuf.st_mode = S_IFDIR;
	else
		rBuf.st_mode = S_IFREG;

	rBuf.st_size = m_size;
	rBuf.st_blksize = 512;			//get cluster size
	rBuf.st_blocks = ((m_size + 511) & ~511) / 512;
	return true;
};

bool FatDirectory::Fstat(struct stat64 &rBuf)
{
	memset(&rBuf, 0, sizeof(rBuf));
	rBuf.st_dev = (dev_t)&m_rFileSystem;
	rBuf.st_ino = (ino_t)this;
	rBuf.st_size = 0;
	rBuf.st_mode = S_IFDIR;
	rBuf.st_rdev = (dev_t)1;
	rBuf.st_blksize = 512;
	rBuf.st_blocks = 1;
	return true;
}

///////////////////////////

//initialisation
void FatFS::ReadBpb(void)
{
	m_rDevice.ReadDataFromLogicalAddress(0, &m_biosParameterBlock, sizeof(Bpb));
}

void FatFS::ReadEbr(void)
{
	m_rDevice.ReadDataFromLogicalAddress(36, &m_extendedBootRecord32, sizeof(Ebr32));

	if (m_biosParameterBlock.m_totalSectors == 0)
		m_fatVersion = 32;
//		else if (m_biosParameterBlock.m_totalSectors / m_biosParameterBlock.m_sectorsPerCluster < 4085)
//			m_fatVersion = 12;
	else
		m_fatVersion = 16;
}

void FatFS::HaveFatMemory(void *pFat)
{
	m_pFat = (unsigned char *)pFat;
}

void FatFS::LoadFat(void)
{
	m_rDevice.ReadDataFromLogicalAddress(SectorToBytes(m_biosParameterBlock.m_reservedSectors),
			m_pFat, FatSize());
}

//cluster walking
bool FatFS::NextCluster32(unsigned int current, unsigned int &rNext) const
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

bool FatFS::NextCluster16(unsigned int current, unsigned int &rNext) const
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

bool FatFS::NextCluster12(unsigned int current, unsigned int &rNext) const
{
	return false;
}

//reading data
void FatFS::ReadCluster(void *pDest, unsigned int cluster, unsigned int sizeInBytes)
{
	m_rDevice.ReadDataFromLogicalAddress(SectorToBytes(ClusterToSector(cluster)), pDest, sizeInBytes);
}


unsigned int FatFS::CountClusters(unsigned int start)
{
	unsigned int total = 1;
	while (NextCluster(start, start))
		total++;
	return total;
}


void FatFS::ReadClusterChain(void *pCluster, unsigned int cluster)
{
	do
	{
		ReadCluster(pCluster, RelativeToAbsoluteCluster(cluster), ClusterSizeInBytes());
		pCluster = (void *)((unsigned char *)pCluster + ClusterSizeInBytes());

	} while (NextCluster(cluster, cluster));
}

//directory walking and debug
void FatFS::BuildDirectoryStructure(void)
{
	ListDirectory(*m_pRoot);
}

void FatFS::ListDirectory(FatDirectory &rDir)
{
//	Printer &p = Printer::Get();

	unsigned int cluster = rDir.GetStartCluster();

	void *pDir = new char[ClusterSizeInBytes() * CountClusters(cluster)];
	ASSERT(pDir);

	ReadClusterChain(pDir, cluster);

	unsigned int slot = 0;
	unsigned int max_slot = ClusterSizeInBytes() * CountClusters(cluster) / 32;

	//todo not necessary, could just be passed down for reuse
	FATDirEnt *pDirent = new FATDirEnt;
	ASSERT(pDirent);

	bool ok;

	do
	{
		ok = IterateDirectory(pDir, slot, *pDirent);
		if (ok)
		{
			if (pDirent->m_cluster == cluster)
				continue;			//'.'
			if (rDir.GetParent() &&			//may be root
					pDirent->m_cluster == ((FatDirectory *)rDir.GetParent())->GetStartCluster())
				continue;			//'..'
			if (pDirent->m_cluster == 0)
				continue;			//'..' one level in

//			BaseDirent *pFile;

//			p << "making dirent name " << pDirent->m_name << "\n";

			if (pDirent->m_type == FATDirEnt::kDirectory)
			{
				FatDirectory *pFile = new FatDirectory(pDirent->m_name, &rDir, *this, pDirent->m_cluster);
				ListDirectory(*pFile);
			}
			else
				/*FatFile *pFile = */new FatFile(pDirent->m_name, &rDir, *this, pDirent->m_cluster, pDirent->m_size);

//			p << "file " << dirent.m_name;
//			p << " rel cluster " << dirent.m_cluster;
//			p << " abs cluster " << RelativeToAbsoluteCluster(dirent.m_cluster);
//			p << " size " << dirent.m_size;
//			p << " attr " << dirent.m_type;
//			p << "\n";
		}
	} while (slot < max_slot);

	delete pDirent;
	delete[] (char *)pDir;
}

bool FatFS::IterateDirectory(void *pCluster, unsigned int &rEntry, FATDirEnt &rOut, bool reentry)
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

		bool ok = IterateDirectory(pCluster, ++rEntry, rOut, true);
		ASSERT(ok);
	}
	else
	{
		//fill in the cluster info
		rOut.m_cluster = dir.p83[rEntry].m_lowCluster | (dir.p83[rEntry].m_highCluster << 16);

		//type
		rOut.m_type = (FATDirEnt::FATDirEntType)dir.p83[rEntry].m_attributes;

		//and file size
		rOut.m_size = dir.p83[rEntry].m_fileSize;

		//copy in the name if appropriate
		if (reentry == false)
			memcpy(rOut.m_name, dir.p83[rEntry].m_name83, sizeof(dir.p83[rEntry].m_name83));
		rEntry++;
	}

	return true;
}


