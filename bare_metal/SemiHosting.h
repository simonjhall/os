/*
 * SemiHosting.h
 *
 *  Created on: 4 Oct 2014
 *      Author: simon
 */

#ifndef SEMIHOSTING_H_
#define SEMIHOSTING_H_

extern "C" unsigned int SemiHosting(unsigned int r0, unsigned int r1);

namespace SemiHostingIntf
{

//http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471g/Bgbjjgij.html
enum {
	kSysClose = 0x2,
	kSysClock = 0x10,
	kSysElapsed = 0x30,
	kSysErrno = 0x13,
	kSysFlen = 0xc,
	kSysGetCmdline = 0x15,
	kSysHeapInfo = 0x16,
	kSysIsError = 0x8,
	kSysIsTty = 0x9,
	kSysOpen = 0x1,
	kSysRead = 0x6,
	kSysReadc = 0x7,
	kSysRemove = 0xe,
	kSysRename = 0xf,
	kSysSeek = 0xa,
	kSysSystem = 0x12,
	kSysTickFreq = 0x31,
	kSysTime = 0x11,
	kSysTmpNam = 0xd,
	kSysWrite = 0x5,
	kSysWritec = 0x3,
	kSysWrite0 = 0x4,
};

inline void SysWrite0(const char *pString)
{
	SemiHosting(kSysWrite0, (unsigned int)pString);
}

}


#endif /* SEMIHOSTING_H_ */
