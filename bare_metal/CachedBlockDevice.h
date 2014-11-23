/*
 * CachedBlockDevice.h
 *
 *  Created on: 23 Nov 2014
 *      Author: simon
 */

#ifndef CACHEDBLOCKDEVICE_H_
#define CACHEDBLOCKDEVICE_H_

#include "BlockDevice.h"
#include <map>

class CachedBlockDevice : public BlockDevice
{
public:
	CachedBlockDevice();
	virtual ~CachedBlockDevice();

	void Init(BlockDevice &rRoot, void *msp, unsigned int blockSize);

	virtual bool ReadDataFromLogicalAddress(unsigned int address, void *pDest, unsigned int byteCount);

protected:

	bool ReadDataFromLogicalAddress_internal(unsigned int address, void *pDest, unsigned int byteCount);

	struct Entry
	{
		unsigned int m_lru;
		void *m_pData;
	};

	BlockDevice *m_pRoot;
	void *m_pool;
	unsigned int m_blockSize;

	std::map<unsigned int, Entry> m_cache;

	unsigned int m_lru;
};

#endif /* CACHEDBLOCKDEVICE_H_ */
