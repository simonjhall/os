/*
 * BaseFS.h
 *
 *  Created on: 2 Jun 2013
 *      Author: simon
 */

#ifndef BASEFS_H_
#define BASEFS_H_

#include <sys/uio.h>

class BaseFile
{
public:
	virtual ssize_t Read(void *pBuf, size_t count);
	virtual ssize_t Write(const void *pBuf, size_t count);
	virtual ssize_t Writev(const struct iovec *iov, int iovcnt);
	virtual ssize_t Readv(const struct iovec *iov, int iovcnt);

	virtual off_t Lseek(off_t offset, Whence w);

	virtual void *Mmap(void *addr, size_t length, int prot, int flags, off_t offset);
	virtual void *Mmap2(void *addr, size_t length, int prot,
                    int flags, off_t pgoffset);
	virtual int Munmap(void *addr, size_t length);

	virtual bool Fstat(struct stat &rBuf);
};

class WrappedFile : public BaseFile
{
public:
	int m_handle;	//needs to be per-process
};

class BaseFS
{
public:
	BaseFS();
	virtual ~BaseFS();

	virtual WrappedFile &Open(const char *pFilename, unsigned int flags);
	virtual bool Close(WrappedFile &);
	virtual WrappedFile &Dup(WrappedFile &);

	virtual bool Stat(const char *pFilename, struct stat &rBuf);
	virtual bool Lstat(const char *pFilename, struct stat &rBuf);

	virtual bool Attach(BaseFS &, const char *pTarget);
	virtual bool Detach(const char *pTarget);
};

//maybe - chdir also needs to be considered
class ProcessRootFS
{
	ProcessRootFS(BaseFS, const char *pRootFilename);
	virtual ~ProcessRootFS();
};

#endif /* BASEFS_H_ */
