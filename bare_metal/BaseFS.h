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
	enum Mode
	{
		kReadOnly,
		kWriteOnly,
		kReadWrite,
	};
	enum Whence
	{
		kSeekSet,
		kSeekCur,
		kSeekEnd,
	};

	virtual ssize_t Read(void *pBuf, size_t count);
	virtual ssize_t Write(const void *pBuf, size_t count);
	virtual ssize_t Writev(const struct iovec *iov, int iovcnt);
	virtual ssize_t Readv(const struct iovec *iov, int iovcnt);

	virtual off_t Lseek(off_t offset, Whence w);

	virtual void *Mmap(void *addr, size_t length, int prot, int flags, off_t offset);
	virtual int Munmap(void *addr, size_t length);
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

	virtual WrappedFile &Open(const char *pFilename, BaseFile::Mode mode);
	virtual bool Close(WrappedFile &);
	virtual WrappedFile &Dup(WrappedFile &);
};

#endif /* BASEFS_H_ */
