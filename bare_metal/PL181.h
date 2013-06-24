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

namespace VersatilePb
{

class PL181 : public SD
{
public:
	PL181(volatile void *pBaseAddress);
	virtual ~PL181();

	virtual void GoIdleState(void)
	{
		Command(kGoIdleState, false);
		SetState(kIdleState);
	}

	virtual bool GoReadyState(void)
	{
		if (GetState() != kIdleState)
			return false;

		unsigned int ocr = SendOcr();
		if (ocr == 0)
		{
			SetState(kInactiveState);
			return false;
		}
		else
		{
			SetState(kReadyState);
			return true;
		}
	}

	virtual bool GoIdentificationState(void)
	{
		if (GetState() != kReadyState && GetState() != kStandbyState)
			return false;

		unsigned int cid = AllSendCid();
		if (cid == 0)
		{
			SetState(kStandbyState);
			return false;
		}
		else
		{
			SetState(kIdentificationState);
			return true;
		}
	}

	virtual void GoInactiveState(void)
	{
		Command(kGoInactiveState, false);
		SetState(kInactiveState);
	}

	virtual bool GoStandbyState(unsigned int rca)
	{
		State current = GetState();

		if (current == kStandbyState)
		{
			//nothing
		}
		else if (current == kIdentificationState)
			SelectDeselectCard(rca);
		else if (current == kTransferState || current == kSendingDataState)
			SelectDeselectCard(rca);
		else if (current == kReceiveDataState || current == kProgrammingState)
			return false;
		else
		{
			//invalid state transition
			return false;
		}

		SetState(kStandbyState);
		return true;
	}

	virtual bool GetCardRcaAndGoStandbyState(unsigned int &rRca)
	{
		if (!kIdentificationState)
			return false;

		unsigned int rca_status = SendRelativeAddr();
		SetState(kStandbyState);

		rRca = rca_status >> 16;
		return true;
	}

	virtual bool GoTransferState(unsigned int rca)
	{
		State current = GetState();

		if (current == kTransferState)
		{
			//nothing
		}
		else if (current == kStandbyState)
			SelectDeselectCard(rca);
		else if (current == kSendingDataState)
			StopTransmission();
		else
		{
			//invalid state transition
			return false;
		}

		SetState(kTransferState);
		return true;
	}

	virtual bool ReadDataFromLogicalAddress(unsigned int address, void *pDest, unsigned int byteCount)
	{
		if (GetState() != kTransferState)
			return false;

		//read the slack into a dummy buffer
		if (address & 511)
		{
			char dummy[512];

			ReadDataUntilStop(address & ~511);
			ReadOutData(dummy, address & 511);
		}
		else
			ReadDataUntilStop(address);

		ReadOutData(pDest, byteCount);
		StopTransmission();

		return true;
	}

protected:
	virtual unsigned int SendOcr(void)
	{
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
		CommandArgument(kReadDatUntilStop, address, false);
	}

	virtual unsigned int StopTransmission(void)
	{
		Command(kStopTransmission, true);
		return Response();
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

			//qemu issue
			Status();
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

			//qemu issue
			Status();
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

}

#endif /* PL181_H_ */
