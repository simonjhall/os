/*
 * BlockDevice.h
 *
 *  Created on: 31 May 2013
 *      Author: simon
 */

#ifndef BLOCKDEVICE_H_
#define BLOCKDEVICE_H_

class BlockDevice
{
public:
	BlockDevice();
	virtual ~BlockDevice();

	virtual bool ReadDataFromLogicalAddress(unsigned int address, void *pDest, unsigned int byteCount) = 0;
};

#endif /* BLOCKDEVICE_H_ */
