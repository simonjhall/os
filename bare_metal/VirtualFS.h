/*
 * VirtualFS.h
 *
 *  Created on: 3 Jun 2013
 *      Author: simon
 */

#ifndef VIRTUALFS_H_
#define VIRTUALFS_H_

#include "BaseFS.h"
#include "common.h"
#include <vector>

class VirtualFS;

class VirtualDirectory : public Directory
{
public:
	VirtualDirectory(const char *pName, VirtualDirectory *pParent, BaseFS &fileSystem)
	: Directory(pName, pParent, fileSystem)
	{
	};

	virtual bool AddChild(BaseDirent *p)
	{
		if (!p)
			return false;

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

protected:
	std::vector<BaseDirent *> m_files;
};

class VirtualFS : public BaseFS
{
public:
	VirtualFS();
	virtual ~VirtualFS();

	virtual BaseDirent *OpenByHandle(BaseDirent &rFile, unsigned int flags);
	virtual bool Close(BaseDirent &);

	virtual bool Stat(const char *pFilename, struct stat &rBuf);
	virtual bool Lstat(const char *pFilename, struct stat &rBuf);

	virtual bool Mkdir(const char *pFilePath, const char *pFilename);
	virtual bool Rmdir(const char *pFilename);

	virtual bool AddOrphanFile(const char *pFilePath, BaseDirent &rFile);

protected:
	virtual BaseDirent *Locate(const char *pFilename, Directory *pParent = 0);

private:

	VirtualDirectory *m_pRoot;
};

#endif /* VIRTUALFS_H_ */
