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

	VirtualDirectory *d = new VirtualDirectory(pFilename, (VirtualDirectory *)mp, *this);
	if (!d)
		return false;

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

BaseDirent *VirtualFS::Locate(const char *pFilename, Directory *pParent)
{
	ASSERT(pFilename);

	if (!pParent)
		pParent = m_pRoot;

	//we want this actual directory
	if (pFilename[0] == 0)
		return pParent;

	//we want the root directory
	if (pFilename[0] == '/')
	{
		if (pParent == m_pRoot)
			return Locate(pFilename + 1, m_pRoot);
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
		if (c && strncmp(c->GetName(), pFilename, length) == 0)
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
		return local;

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

