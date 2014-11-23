/*
 * VirtualFS.cpp
 *
 *  Created on: 3 Jun 2013
 *      Author: simon
 */

#include "VirtualFS.h"
#include <string.h>
#include <fcntl.h>

VirtualFS::VirtualFS()
{
	//insert the root directory
	m_pRoot = new VirtualDirectory("", 0, *this);
}

VirtualFS::~VirtualFS()
{
	delete m_pRoot;
	// TODO Auto-generated destructor stub
}

BaseDirent *VirtualFS::OpenByHandle(BaseDirent &rFile, unsigned int flags)
{
	if (flags & O_CREAT)
		return 0;

	if (((flags & O_ACCMODE) == O_RDONLY) && rFile.LockRead())
		return &rFile;
	else if (((flags & O_ACCMODE) == O_WRONLY) && rFile.LockWrite())
		return &rFile;
	else if (((flags & O_ACCMODE) == O_RDWR) && rFile.LockWrite())
		return &rFile;
	else
		return 0;
}


bool VirtualFS::Close(BaseDirent &f)
{
	if (&f.GetFilesystem() == this)
	{
		return true;
	}
	else
		return f.GetFilesystem().Close(f);
}

bool VirtualFS::Stat(const char *pFilename, struct stat &rBuf)
{
	return false;
}

bool VirtualFS::Lstat(const char *pFilename, struct stat &rBuf)
{
	return false;
}

bool VirtualFS::Mkdir(const char *pFilePath, const char *pFilename)
{
	BaseDirent *mp = Locate(pFilePath, 0);
	if (!mp || !mp->IsDirectory())
		return false;

	//change filesystem if necessary
	if (&mp->GetFilesystem() != this)
		return mp->GetFilesystem().Mkdir(pFilePath, pFilename);

	VirtualDirectory *d = new VirtualDirectory(pFilename, (VirtualDirectory *)mp, *this);
	if (!d)
		return false;

	if (d->IsOrphan())
	{
		delete d;
		return false;
	}

	return true;
}

bool VirtualFS::Rmdir(const char *pFilename)
{
	return false;
}

bool VirtualFS::AddOrphanFile(const char *pFilePath, BaseDirent &rFile)
{
	ASSERT(pFilePath);
	BaseDirent *f = Locate(pFilePath);

	if (!f->IsDirectory())
		return false;

	Directory *d = (Directory *)f;
	if (!d->AddChild(&rFile))
		return false;

	if (rFile.Reparent(d) == false)
		ASSERT(0);

	return true;
}

Directory &VirtualFS::GetRootDirectory(void)
{
	ASSERT(m_pRoot);
	return *m_pRoot;
}

bool VirtualDirectory::Fstat(struct stat64& rBuf)
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
