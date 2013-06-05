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

BaseDirent *VirtualFS::Open(const char *pFilename, unsigned int flags)
{
	if ((flags & O_CREAT) || ((flags & O_ACCMODE) == O_WRONLY) || ((flags & O_ACCMODE) == O_RDWR))
		return 0;

	BaseDirent *f = Locate(pFilename);

	if (!f)
		return 0;

	if (&f->GetFilesystem() == this)
		return Open(*f, flags);
	else
		return f->GetFilesystem().Open(*f, flags);
}

BaseDirent *VirtualFS::Open(BaseDirent &rFile, unsigned int flags)
{
	return 0;
}


bool VirtualFS::Close(BaseDirent &f)
{
	if (&f.GetFilesystem() == this)
	{
		ASSERT(0);
		return false;
	}
	else
		return f.GetFilesystem().Close(f);
}

//WrappedFile &VirtualFS::Dup(WrappedFile &)
//{
//}

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
	if (!mp->IsDirectory())
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
		ASSERT(pParent == m_pRoot);
		return Locate(pFilename + 1, m_pRoot);
	}

	//check if there's a path in here or just a lone name
	const char *slash = strstr(pFilename, "/");
	unsigned int length = strlen(pFilename);

	if (slash)
		length = slash - pFilename;

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

	if (!local->IsDirectory())
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

