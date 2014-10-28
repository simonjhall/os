/*
 * WrappedFile.cpp
 *
 *  Created on: 4 Jun 2013
 *      Author: simon
 */

#include "common.h"
#include "WrappedFile.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

ssize_t WrappedFile::Read(void *pBuf, size_t count)
{
	ASSERT(m_dupCount);

	if (!m_pFile || m_pFile->IsDirectory())
		return -EBADF;

	ssize_t out = ((File *)m_pFile)->ReadFrom(pBuf, count, m_pos);
	if (out >= 0)
		m_pos += out;

	return out;
}

ssize_t WrappedFile::Write(const void *pBuf, size_t count)
{
	ASSERT(m_dupCount);

	if (!m_pFile || m_pFile->IsDirectory())
		return -EBADF;

	ssize_t out = ((File *)m_pFile)->WriteTo(pBuf, count, m_pos);
	if (out >= 0)
		m_pos += out;

	return out;
}

off_t WrappedFile::Lseek(off_t offset, int whence)
{
	ASSERT(m_dupCount);

	if (!m_pFile || m_pFile->IsDirectory())
		return -EBADF;

	switch (whence)
	{
		case SEEK_SET:
			m_pos = offset;
			break;
		case SEEK_CUR:
			m_pos += offset;
			break;
		case SEEK_END:
			ASSERT(0);
			break;
		default:
			ASSERT(0);
			break;
	}

	if (((File *)m_pFile)->Seekable(m_pos) == false)
		return -EINVAL;

	return m_pos;
}

#ifdef __ARM_ARCH_7A__
ssize_t WrappedFile::Writev(const struct iovec *pIov, int iovcnt)
{
	ASSERT(m_dupCount);

	if (!m_pFile || m_pFile->IsDirectory())
		return -EBADF;

	ssize_t written = 0;

	for (int count = 0; count < iovcnt; count++)
	{
		Write((char *)pIov[count].iov_base, pIov[count].iov_len);
		written += pIov[count].iov_len;
	}

	return written;
}

ssize_t WrappedFile::Readv(const struct iovec *pIov, int iovcnt)
{
	ASSERT(m_dupCount);

	if (!m_pFile || m_pFile->IsDirectory())
		return -EBADF;

	ssize_t read = 0;

	for (int count = 0; count < iovcnt; count++)
	{
		Read((char *)pIov[count].iov_base, pIov[count].iov_len);
		read += pIov[count].iov_len;
	}

	return read;
}
#endif

void WrappedFile::Close(void)
{
	ASSERT(m_dupCount);

	if (m_pFile)
	{
		bool ok = m_pFile->GetFilesystem().Close(*m_pFile);
		ASSERT(ok);

		m_pFile = 0;
	}
}

bool WrappedFile::Fstat(struct stat64 &rBuf)
{
	ASSERT(m_dupCount);

	if (m_pFile)
		return m_pFile->Fstat(rBuf);
	else
		return 0;
}

void *WrappedFile::Mmap(void *addr, size_t length, int prot, int flags, off_t offset, bool isPriv)
{
	ASSERT(m_dupCount);

	if (m_pFile && !m_pFile->IsDirectory())
		return ((File *)m_pFile)->Mmap(addr, length, prot, flags, offset, isPriv);
	else
		return false;
}

void *WrappedFile::Mmap2(void *addr, size_t length, int prot,
				int flags, off_t pgoffset, bool isPriv)
{
	ASSERT(m_dupCount);

	if (m_pFile && !m_pFile->IsDirectory())
		return ((File *)m_pFile)->Mmap2(addr, length, prot, flags, pgoffset, isPriv);
	else
		return false;
}

bool WrappedFile::Munmap(void *addr, size_t length)
{
	ASSERT(m_dupCount);

	if (m_pFile && !m_pFile->IsDirectory())
		return ((File *)m_pFile)->Munmap(addr, length);
	else
		return false;
}

int WrappedFile::Getdents64(linux_dirent64 *pDir, unsigned int count)
{
	ASSERT(m_dupCount);

	if (m_pFile && m_pFile->IsDirectory())
		return ((Directory *)m_pFile)->FillLinuxDirent(pDir, count, m_pos);
	else
		return -1;
}

///////////////

ProcessFS::ProcessFS(const char *pRootFilename, const char *pInitialDirectory)
{
	if (pRootFilename)
	{
		ASSERT(strlen(pRootFilename) + 1 < sm_maxLength);
		strncpy(m_rootDirectory, pRootFilename, strlen(pRootFilename) + 1);
	}
	else
		strcpy(m_rootDirectory, "/");

	if (pInitialDirectory)
	{
		Chdir(pInitialDirectory);
	}
	else
		strcpy(m_currentDirectory, "/");
}

ProcessFS::~ProcessFS()
{
	//delete files
	for (unsigned int count = 0; count < m_fileHandles.size(); count++)
		Close(count);
}

void ProcessFS::Chdir(const char *pPath)
{
	ASSERT(pPath);

	if (pPath[0] == '/')
	{
		ASSERT(strlen(pPath) + 1 < sm_maxLength);
		memset(m_currentDirectory, 0, sm_maxLength);
		strncpy(m_currentDirectory, pPath, strlen(pPath) + 1);
	}
	else
	{
		size_t existing = strlen(m_currentDirectory);
		ASSERT(existing + strlen(pPath) + 1 + 1 < sm_maxLength);
		strcpy(m_currentDirectory + existing, "/");
		strncpy(m_currentDirectory + existing + 1, pPath, strlen(pPath) + 1);
	}
}

bool ProcessFS::Getcwd(char *pPath, size_t len)
{
	ASSERT(pPath);

	if (strlen(m_currentDirectory) >= len)
		return false;

	memset(pPath, 0, len);
	strncpy(pPath, m_currentDirectory, strlen(m_currentDirectory));
	return true;
}

const char *ProcessFS::BuildFullPath(const char *pIn, char *pOut, size_t outLen)
{
	size_t rootLen = strlen(m_rootDirectory);
	size_t inLen = strlen(pIn);
	size_t curLen = strlen(m_currentDirectory);

	if (pIn[0] == '/')
		curLen = 0;

	if (rootLen + curLen + inLen + 1 + 1 > outLen)
		return 0;

	memset(pOut, 0, outLen);

	//copy in the root
	strcpy(pOut, m_rootDirectory);
	//copy in the current directory
	strncpy(pOut + rootLen, m_currentDirectory, curLen);
	if (curLen)
	{
		strncpy(pOut + rootLen + curLen, "/", curLen);
		curLen++;
	}
	//copy in the actual path
	strcpy(pOut + rootLen + curLen, pIn);

	return pOut;
}

int ProcessFS::Open(BaseDirent &rFile)
{
	WrappedFile *f = new WrappedFile(&rFile);
	ASSERT(f);
	f->Inc();

	m_fileHandles.push_back(f);
	return m_fileHandles.size() - 1;
}

bool ProcessFS::Close(int file)
{
	WrappedFile *f = GetFile(file);
	if (f)
	{
		f->Close();

		if (!f->Dec())
			delete f;

		//todo do a better job
		m_fileHandles[file] = 0;
		return true;
	}
	else
		return false;
}

int ProcessFS::Dup(int file)
{
	WrappedFile *f = GetFile(file);
	if (f)
	{
		f->Inc();
		m_fileHandles.push_back(f);
		return m_fileHandles.size() - 1;
	}
	else
		return -1;
}

WrappedFile *ProcessFS::GetFile(int file)
{
	//todo fix warning
	if (file >= 0 && file < m_fileHandles.size())
		return m_fileHandles[file];
	else
		return 0;
}
