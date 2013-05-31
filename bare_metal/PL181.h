/*
 * PL181.h
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#ifndef PL181_H_
#define PL181_H_

#include <string.h>
#include "common.h"
#include "SD.h"

class PL181 : public SD
{
public:
	PL181(volatile void *pBaseAddress);
	virtual ~PL181();

//protected:
	virtual void GoIdleState(void)
	{
		Command(kGoIdleState, false);
	}

	virtual unsigned int SendOcr(void)
	{
//		Command(kSendOpCmd, true);
//		return Response();

		Command(kSdAppCmd, true);
		if (Response() & (1 << 5))
		{
			//fake voltage
			CommandArgument(kSdAppOpCond, 1, true);
			return Response();
		}

		return -1;
	}

	virtual unsigned int AllSendCid(void)
	{
		Command(kAllSendCid, true);
		return Response();
	}

	virtual unsigned int SendRelativeAddr(void)
	{
		Command(kSendRelativeAddr, true);
		return Response();
	}

	virtual unsigned int SelectDeselectCard(unsigned int rca)
	{
		CommandArgument(kSelectDeselectCard, rca << 16, true);
		return Response();
	}

	virtual void ReadDataUntilStop(unsigned int address)
	{
		ASSERT((address & 511) == 0);
		CommandArgument(kReadDatUntilStop, address >> 9, false);
	}

	virtual unsigned int StopTransmission(void)
	{
		Command(kStopTransmission, true);
		return Response();
	}

	virtual void kSdAppOpCondUNSURE(unsigned int voltage)
	{
//		Command(kSdAppCmd, true);
//		if (Response() & (1 << 5))
//		{
//			Command(kSdAppOpCond, true);
//		}
	}

	virtual void ReadOutData(void *pDest, unsigned int byteCount)
	{
		unsigned int *pOutW = (unsigned int *)pDest;
		while (byteCount >= 4)
		{
			//data length
			m_pBaseAddress[10] = 4;
			//data timer
			m_pBaseAddress[9] = 0xffffffff;
			//data control
			m_pBaseAddress[11] = 1 | (1 << 1);		//enable + read

			*pOutW++ = ((unsigned int *)m_pBaseAddress)[32];
			byteCount -= 4;
		}

		unsigned char *pOutB = (unsigned char *)pOutW;
		while (byteCount)
		{
			//data length
			m_pBaseAddress[10] = 1;
			//data timer
			m_pBaseAddress[9] = 0xffffffff;
			//data control
			m_pBaseAddress[11] = 1 | (1 << 1);		//enable + read

			*pOutB++ = ((unsigned char *)m_pBaseAddress)[0x80];
			byteCount--;
		}
	}

private:
	void Command(SdCommand c, bool wait);
	void CommandArgument(SdCommand c, unsigned int a, bool wait);
	unsigned int Response(unsigned int word = 0);
	unsigned int Status(void);

	volatile unsigned int *m_pBaseAddress;

	enum Status
	{
		kCmdTimeout = 2,
		kDataTimeout = 3,
		kTxUnderrun = 4,
		kRxUnderrun = 5,
		kDataEnd = 8,
		kDataBlockEnd = 10,
		kCmdActive = 11,
		kTxActive = 12,
		kRxActive = 13,
		kTxDataAvailable = 20,
		kRxDataAvailable = 21,
	};
};

#endif /* PL181_H_ */
