/*
 * BaseFS.h
 *
 *  Created on: 2 Jun 2013
 *      Author: simon
 */

#ifndef BASEFS_H_
#define BASEFS_H_

#include <vector>
#include <sys/types.h>

struct linux_dirent64
{
	unsigned long long d_ino;
	signed long long d_off;
	unsigned short  d_reclen;
	unsigned char   d_type;
	char            d_name[0];
 };

class Directory;
class BaseFS;

class BaseDirent
{
public:
	virtual ~BaseDirent()
	{
	};

	virtual bool Fstat(struct stat64 &rBuf)
	{
		return false;
	};

	const char *GetName(void) { return m_pName; };
	Directory *GetParent(void) { return m_pParent; };
	bool IsDirectory(void) { return m_directory; }
	BaseFS &GetFilesystem(void) { return m_rFileSystem; };

	virtual bool LockRead(void);
	virtual bool LockWrite(void);
	virtual void Unlock(void);

	//todo not so sure here
	virtual bool Reparent(Directory *pParent);
	inline bool IsOrphan(void) { return m_orphan; };

protected:
	BaseDirent(const char *pName, Directory *pParent, BaseFS &fileSystem,
			bool isDirectory);

	char *m_pName;
	Directory *m_pParent;
	BaseFS &m_rFileSystem;
	bool m_directory;			//no rtti

	bool m_orphan;				//no parent

private:
	unsigned int m_readLockCount;
	bool m_writeLock;
};

class File : public BaseDirent
{
public:
	virtual ~File()
	{
	};

	virtual ssize_t ReadFrom(void *pBuf, size_t count, off_t offset) = 0;
	virtual ssize_t WriteTo(const void *pBuf, size_t count, off_t offset) = 0;
	virtual bool Seekable(off_t) = 0;

	virtual void *Mmap(void *addr, size_t length, int prot, int flags, off_t offset, bool isPriv);
	virtual void *Mmap2(void *addr, size_t length, int prot,
                    int flags, off_t pgoffset, bool isPriv);
	virtual bool Munmap(void *addr, size_t length);

protected:
	File(const char *pName, Directory *pParent, BaseFS &fileSystem)
	: BaseDirent(pName, pParent, fileSystem, false)
	{
	};

};

class Directory : public BaseDirent
{
public:
	virtual ~Directory()
	{
	};

	virtual bool AddChild(BaseDirent *p) = 0;
	virtual unsigned int GetNumChildren(void) = 0;
	virtual BaseDirent *GetChild(unsigned int) = 0;

	virtual int FillLinuxDirent(linux_dirent64 *pOut, unsigned int len, off_t &rChild);

protected:
	Directory(const char *pName, Directory *pParent, BaseFS &fileSystem)
	: BaseDirent(pName, pParent, fileSystem, true)
	{
	};

};


class BaseFS
{

public:
	virtual BaseDirent *OpenByName(const char *pFilename, unsigned int flags);
	virtual BaseDirent *OpenByHandle(BaseDirent &rFile, unsigned int flags) = 0;
	virtual bool Close(BaseDirent &) = 0;

	virtual bool Stat(const char *pFilename, struct stat &rBuf) = 0;
	virtual bool Lstat(const char *pFilename, struct stat &rBuf) = 0;

	virtual bool Mkdir(const char *pFilePath, const char *pFilename) = 0;
	virtual bool Rmdir(const char *pFilename) = 0;

	virtual bool Attach(BaseFS &, const char *pTarget);
	virtual bool Detach(const char *pTarget);
	virtual BaseFS *IsAttachment(const Directory *);

	virtual BaseDirent *Locate(const char *pFilename, Directory *pParent = 0);
protected:
	BaseFS();
	virtual ~BaseFS();

	virtual Directory &GetRootDirectory(void) = 0;


	struct Attachment
	{
		Attachment(Directory *pMountPoint, BaseFS &rFilesystem)
		: m_pMountPoint(pMountPoint),
		  m_rFilesystem(rFilesystem)
		{
		}

		Directory *m_pMountPoint;
		BaseFS &m_rFilesystem;
	};

	std::vector<Attachment *> m_attachments;
};

#endif /* BASEFS_H_ */
