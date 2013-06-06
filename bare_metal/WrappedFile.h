/*
 * WrappedFile.h
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#ifndef WRAPPEDFILE_H_
#define WRAPPEDFILE_H_

#include "BaseFS.h"
#include <vector>

class WrappedFile
{
public:
	WrappedFile(File *p)
	: m_pFile(p),
	  m_pos(0),
	  m_dupCount(0)
	{
	}
	virtual ~WrappedFile()
	{
	};

	virtual void Inc(void) { m_dupCount++; };
	virtual unsigned int Dec(void) { return --m_dupCount; };

	virtual void Close(void);

	virtual ssize_t Read(void *pBuf, size_t count);
	virtual ssize_t Write(const void *pBuf, size_t count);
	virtual ssize_t Writev(const struct iovec *iov, int iovcnt);
	virtual ssize_t Readv(const struct iovec *iov, int iovcnt);

	virtual off_t Lseek(off_t offset, int whence);
	virtual bool Fstat(struct stat &rBuf);

protected:
	File *m_pFile;
	off_t m_pos;
	unsigned int m_dupCount;
};

class ProcessFS
{
public:
	ProcessFS(const char *pRootFilename, const char *pInitialDirectory);
	virtual ~ProcessFS();

	virtual void Chdir(const char *pPath);

	virtual const char *BuildFullPath(const char *pIn, char *pOut, size_t outLen);

	virtual int Open(BaseDirent &rHopefullyFile);
	virtual bool Close(int file);
	virtual int Dup(int file);

	virtual WrappedFile *GetFile(int file);

protected:
	//todo do better?
	static const size_t sm_maxLength = 256;
	char m_rootDirectory[sm_maxLength];
	char m_currentDirectory[sm_maxLength];

	std::vector<WrappedFile *> m_fileHandles;
};

#endif /* WRAPPEDFILE_H_ */
