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
#include <errno.h>
#include <dirent.h>

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

BaseFS *BaseFS::IsAttachment(const Directory *p)
{
	for (std::vector<Attachment *>::iterator it = m_attachments.begin(); it != m_attachments.end(); it++)
		if ((*it)->m_pMountPoint == p)
			return &(*it)->m_rFilesystem;

	return 0;
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

BaseDirent *BaseFS::Locate(const char *pFilename, Directory *pParent)
{
	ASSERT(pFilename);

	Directory &rRoot = GetRootDirectory();

	if (!pParent)
		pParent = &rRoot;

	//we want this actual directory
	if (pFilename[0] == 0)
		return pParent;

	//we want the root directory
	if (pFilename[0] == '/')
	{
		if (pParent == &rRoot)
			return Locate(pFilename + 1, &rRoot);
		else
			return Locate(pFilename + 1, pParent);
	}

	//check if there's a path in here or just a lone name
	const char *slash = strstr(pFilename, "/");
	unsigned int length = strlen(pFilename);

	if (slash)
		length = slash - pFilename;

	if (strncmp(pFilename, ".", length) == 0)
		return Locate(pFilename + 1, pParent);

	if (strncmp(pFilename, "..", length) == 0)
	{
		ASSERT(pParent);
		ASSERT(pParent->GetParent());
		return Locate(pFilename + 1, pParent->GetParent());
	}

	BaseDirent *local = 0;
	for (unsigned int count = 0; count < pParent->GetNumChildren(); count++)
	{
		BaseDirent *c = pParent->GetChild(count);
		const char *pOtherName = c->GetName();
		ASSERT(pOtherName);

		if (c && strlen(pOtherName) == length && strncmp(pOtherName, pFilename, length) == 0)
		{
			local = c;
			break;
		}
	}

	if (!local)
		return 0;

	if (slash)
		ASSERT(local->IsDirectory());

	if (!local->IsDirectory() || !slash)
	{
		//found what we're looking for, double check it's not an attach point
		if (local->IsDirectory())
		{
			BaseFS *pMp = IsAttachment((Directory *)local);
			if (pMp)
				return &pMp->GetRootDirectory();
		}

		return local;
	}

	//check attach points
	for (std::vector<Attachment *>::iterator it = m_attachments.begin(); it != m_attachments.end(); it++)
		if ((*it)->m_pMountPoint == local)
		{
			//change filesystem
			return (*it)->m_rFilesystem.Locate(slash + 1);
		}

	//not a mount point
	return Locate(slash + 1, (Directory *)local);
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

///////////////////////////////
int Directory::FillLinuxDirent(linux_dirent64* pOut, unsigned int len,
		off_t& rChild)
{
	Printer &p = Printer::Get();

	int written = 0;
	int base_size = sizeof(linux_dirent64);
	bool no_more_children = false;

	ASSERT(pOut);

	linux_dirent64 out;

	while (len)
	{
		if (base_size >= len)
			return -EINVAL;

		BaseDirent *pChild = GetChild(rChild);
		if (!pChild)
		{
			no_more_children = true;
			break;
		}

		out.d_ino = (unsigned long long)pChild;
		out.d_ino = out.d_ino & 0xffffffff;
		out.d_off = rChild;
		out.d_type = pChild->IsDirectory() ? DT_DIR : DT_REG;

		size_t name_len = strlen(pChild->GetName()) + 1;

		if (base_size + name_len > len)
			break;

		len -= base_size;
		len -= name_len;

		//align the size up to a multiple of eight bytes for the ldrd which reads the d_ino
		int align = ((base_size + name_len) + 7) & ~7;
		align -= (base_size + name_len);

		len -= align;

		out.d_reclen = base_size + name_len + align;
		written += out.d_reclen;

		//copy in to avoid alignment faults, this is really bad
		memcpy(pOut, &out, sizeof(linux_dirent64));
		strcpy(pOut->d_name, pChild->GetName());

		pOut = (linux_dirent64 *)((char *)pOut + pOut->d_reclen);
		rChild++;
	}

	if (!written && !no_more_children)
		return -EINVAL;
	else
		return written;
}
