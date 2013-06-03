/*
 * BaseFS.cpp
 *
 *  Created on: 2 Jun 2013
 *      Author: simon
 */

#include "BaseFS.h"
#include "common.h"
#include <string.h>

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

BaseDirent::BaseDirent(const char *pName, Directory *pParent, BaseFS &fileSystem, bool directory)
: m_pParent(pParent),
  m_rFileSystem(fileSystem),
  m_directory(directory)
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

