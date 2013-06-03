/*
 * BaseFS.h
 *
 *  Created on: 2 Jun 2013
 *      Author: simon
 */

#ifndef BASEFS_H_
#define BASEFS_H_

#include <vector>
#include <sys/uio.h>

class Directory;
class BaseFS;

class BaseDirent
{
public:
	virtual ~BaseDirent()
	{
	};

	virtual bool Fstat(struct stat &rBuf)
	{
		return false;
	};

	const char *GetName(void) { return m_pName; };
	Directory *GetParent(void) { return m_pParent; };

	bool IsDirectory(void) { return m_directory; }

protected:
	BaseDirent(const char *pName, Directory *pParent, BaseFS &fileSystem,
			bool isDirectory);

	char *m_pName;
	Directory *m_pParent;
	BaseFS &m_rFileSystem;
	bool m_directory;			//no rtti

	bool m_orphan;
};

class File : public BaseDirent
{
public:
	virtual ~File()
	{
	};

	virtual ssize_t Read(void *pBuf, size_t count);
	virtual ssize_t Write(const void *pBuf, size_t count);
	virtual ssize_t Writev(const struct iovec *iov, int iovcnt);
	virtual ssize_t Readv(const struct iovec *iov, int iovcnt);

	virtual off_t Lseek(off_t offset, int whence);

	virtual void *Mmap(void *addr, size_t length, int prot, int flags, off_t offset);
	virtual void *Mmap2(void *addr, size_t length, int prot,
                    int flags, off_t pgoffset);
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

protected:
	Directory(const char *pName, Directory *pParent, BaseFS &fileSystem)
	: BaseDirent(pName, pParent, fileSystem, true)
	{
	};

};

/*
class WrappedFile : public BaseFile
{
public:
	virtual ~WrappedFile()
	{
	};

	int m_handle;	//needs to be per-process
};*/

class BaseFS
{
public:
	BaseFS();
	virtual ~BaseFS();

	virtual bool Open(const char *pFilename, unsigned int flags, BaseDirent &rOut) = 0;
	virtual bool Close(BaseDirent &) = 0;
//	virtual WrappedFile &Dup(WrappedFile &) = 0;

	virtual bool Stat(const char *pFilename, struct stat &rBuf) = 0;
	virtual bool Lstat(const char *pFilename, struct stat &rBuf) = 0;

	virtual bool Mkdir(const char *pFilePath, const char *pFilename) = 0;
	virtual bool Rmdir(const char *pFilename) = 0;

	virtual bool Attach(BaseFS &, const char *pTarget);
	virtual bool Detach(const char *pTarget);

//protected:
	virtual BaseDirent *Locate(const char *pFilename, Directory *pParent = 0) = 0;

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

class ProcessFS
{
public:
	ProcessFS(BaseFS &, const char *pRootFilename, const char *pInitialDirectory);
	virtual ~ProcessFS();

	bool Chdir(const char *pPath);
};

#endif /* BASEFS_H_ */
