/*
 * BaseFS.cpp
 *
 *  Created on: 2 Jun 2013
 *      Author: simon
 */

#include "BaseFS.h"
#include "common.h"
#include <string.h>
#include <fcntl.h>

BaseFS::BaseFS()
{
}

BaseFS::~BaseFS()
{
}

bool BaseFS::Attach(BaseFS &rFS, const char *pTarget)
{
	//get the mount point
	BaseDirent *pFile = Locate(pTarget);
	if (!pFile)
		return false;

	if (!pFile->IsDirectory())
		return false;

	Attachment *a = new Attachment((Directory *)pFile, rFS);
	m_attachments.push_back(a);

	return true;
}

bool BaseFS::Detach(const char *pTarget)
{
	return false;
}

BaseDirent *BaseFS::OpenByName(const char *pFilename, unsigned int flags)
{
	if (flags & O_CREAT)
		return 0;

	BaseDirent *f = Locate(pFilename);

	if (!f)
		return 0;

	if (&f->GetFilesystem() == this)
		return OpenByHandle(*f, flags);
	else
		return f->GetFilesystem().OpenByHandle(*f, flags);
}


//////////////////////////////////////////

BaseDirent::BaseDirent(const char *pName, Directory *pParent, BaseFS &fileSystem, bool directory)
: m_pParent(pParent),
  m_rFileSystem(fileSystem),
  m_directory(directory),
  m_readLockCount(0),
  m_writeLock(0)
{
	size_t len = strlen(pName);
	m_pName = new char[len + 1];
	ASSERT(m_pName);
	strcpy(m_pName, pName);

	if (pParent && pParent->AddChild(this))
		m_orphan = false;
	else
		m_orphan = true;
}

bool BaseDirent::Reparent(Directory *pParent)
{
	return false;
}

bool BaseDirent::LockRead(void)
{
	if (m_writeLock)
		return false;

	m_readLockCount++;
	return true;
}

void BaseDirent::Unlock(void)
{
	if (m_writeLock)
		m_writeLock = false;
	else if (m_readLockCount)
		m_readLockCount--;
	else
		ASSERT(0);
}

bool BaseDirent::LockWrite(void)
{
	if (m_writeLock)
		return false;
	if (m_readLockCount)
		return false;

	m_writeLock = true;
	return true;
}

///////////////////////////////

void *File::Mmap(void *addr, size_t length, int prot, int flags, off_t offset, bool isPriv)
{
	void *res = internal_mmap(addr, length, prot, flags, this, offset, false);
	if (res)
	{
		ASSERT(res == addr);
		bool ok = ReadFrom(addr, length, offset);
		ASSERT(ok);
	}
	return res;
}

void *File::Mmap2(void *addr, size_t length, int prot,
				int flags, off_t pgoffset, bool isPriv)
{
	return Mmap(addr, length, prot, flags, pgoffset << 12, isPriv);
}

bool File::Munmap(void *addr, size_t length)
{
	return internal_munmap(addr, length) ? true : false;
}
