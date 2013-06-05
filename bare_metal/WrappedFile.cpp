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

ssize_t WrappedFile::Read(void *pBuf, size_t count)
{
	ASSERT(m_dupCount);

	if (!m_pFile)
		return -EBADF;

	ssize_t out = m_pFile->ReadFrom(pBuf, count, m_pos);
	if (out >= 0)
		m_pos += out;

	return out;
}

ssize_t WrappedFile::Write(const void *pBuf, size_t count)
{
	ASSERT(m_dupCount);

	if (!m_pFile)
		return -EBADF;

	ssize_t out = m_pFile->WriteTo(pBuf, count, m_pos);
	if (out >= 0)
		m_pos += out;

	return out;
}

off_t WrappedFile::Lseek(off_t offset, int whence)
{
	ASSERT(m_dupCount);

	if (!m_pFile)
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

	if (m_pFile->Seekable(m_pos) == false)
		return -EINVAL;

	return m_pos;
}

ssize_t WrappedFile::Writev(const struct iovec *pIov, int iovcnt)
{
	ASSERT(m_dupCount);

	if (!m_pFile)
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

	if (!m_pFile)
		return -EBADF;

	ssize_t read = 0;

	for (int count = 0; count < iovcnt; count++)
	{
		Read((char *)pIov[count].iov_base, pIov[count].iov_len);
		read += pIov[count].iov_len;
	}

	return read;
}

void WrappedFile::Close(void)
{
	if (m_pFile)
	{
		bool ok = m_pFile->GetFilesystem().Close(*m_pFile);
		ASSERT(ok);

		m_pFile = 0;
	}
}

ProcessFS::ProcessFS(BaseFS &, const char *pRootFilename, const char *pInitialDirectory)
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
}

void ProcessFS::Chdir(const char *pPath)
{
	ASSERT(pPath);

	ASSERT(strlen(pPath) + 1 < sm_maxLength);
	strncpy(m_currentDirectory, pPath, strlen(pPath) + 1);
}

void ProcessFS::BuildFullPath(const char *pIn, char *pOut, size_t outLen)
{
	ASSERT(0);
}

int ProcessFS::Open(BaseDirent &rFile)
{
	if (rFile.IsDirectory())
		return false;

	File *file = (File *)&rFile;

	WrappedFile *f = new WrappedFile(file);
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
