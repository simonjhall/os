/*
 * CachedBlockDevice.cpp
 *
 *  Created on: 23 Nov 2014
 *      Author: simon
 */

#include "CachedBlockDevice.h"
#include "common.h"

#include "malloc.h"

#include <string.h>

CachedBlockDevice::CachedBlockDevice()
: m_pRoot(0),
  m_pool(0),
  m_blockSize(0),
  m_lru(0)
{
}

CachedBlockDevice::~CachedBlockDevice()
{
	m_pRoot = 0;
}

void CachedBlockDevice::Init(BlockDevice& rRoot, void *msp, unsigned int blockSize)
{
	m_pRoot = &rRoot;
	m_pool = msp;
	m_blockSize = blockSize;
}

bool CachedBlockDevice::ReadDataFromLogicalAddress(unsigned int address,
		void* pDest, unsigned int byteCount)
{
	unsigned int to_read = byteCount;
	while (to_read)
	{
		unsigned int amount = to_read > m_blockSize ? m_blockSize : to_read;

		bool ok = ReadDataFromLogicalAddress_internal(address, pDest, amount);
		if (!ok)
			return false;

		address += amount;
		pDest = (void *)((char *)pDest + amount);
		to_read -= amount;
	}

	return true;
}

bool CachedBlockDevice::ReadDataFromLogicalAddress_internal(unsigned int address,
		void* pDest, unsigned int byteCount)
{
	ASSERT(m_pRoot);
	ASSERT(m_pool);

	m_lru++;

	//try and find it first
	auto f = m_cache.find(address);
	if (f != m_cache.end())
	{
		f->second.m_lru = m_lru;
		memcpy(pDest, f->second.m_pData, byteCount);
		return true;
	}

	//not in the cache...

	//allocate some space
	void *p = 0;

	do
	{
		p = mspace_malloc(m_pool, m_blockSize);

		if (!p)
		{
			unsigned int last = m_lru;
			auto last_it = m_cache.begin();

			//find the oldest
			for (auto it = m_cache.begin(); it != m_cache.end(); it++)
				if (it->second.m_lru < last)
				{
					last = it->second.m_lru;
					last_it = it;
				}

			ASSERT(last != m_lru);			//empty cache??

			//free it
			mspace_free(m_pool, last_it->second.m_pData);
			last_it->second.m_pData = 0;

			m_cache.erase(last_it);
			//try again
		}
	} while (!p);

	Entry e;
	e.m_lru = m_lru;
	e.m_pData = p;

	//read it
	if (m_pRoot->ReadDataFromLogicalAddress(address, p, byteCount))
	{
		//insert it
		m_cache[address] = e;
		//return the data
		memcpy(pDest, p, byteCount);

		return true;
	}
	else
	{
		mspace_free(m_pool, p);
		return false;
	}
}
