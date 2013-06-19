/*
 * MMCi.h
 *
 *  Created on: 17 Jun 2013
 *      Author: simon
 */

#ifndef MMCI_H_
#define MMCI_H_

#include "SD.h"
#include "common.h"

namespace OMAP4460
{

class MMCi: public SD
{
public:
	MMCi(volatile void *pBaseAddress, int i);
	virtual ~MMCi();

	virtual void GoIdleState(void)
	{
		Command(kGoIdleState, false, false);
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
		Command(kGoInactiveState, false, false);
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

	virtual bool Reset(void);

protected:
	virtual unsigned int SendOcr(void)
	{
		Command(kSdAppCmd, true, false);
		if (Response() & (1 << 5))
		{
			//fake voltage
			CommandArgument(kSdAppOpCond, 1, true, false);
			return Response();
		}

		return -1;
	}

	virtual unsigned int AllSendCid(void)
	{
		Command(kAllSendCid, true, false);
		return Response();
	}

	virtual unsigned int SendRelativeAddr(void)
	{
		Command(kSendRelativeAddr, true, false);
		return Response();
	}

	virtual unsigned int SelectDeselectCard(unsigned int rca)
	{
		CommandArgument(kSelectDeselectCard, rca << 16, true, false);
		return Response();
	}

	virtual void ReadDataUntilStop(unsigned int address)
	{
		ASSERT((address & 511) == 0);
		CommandArgument(kReadDatUntilStop, address, false, true);
	}

	virtual unsigned int StopTransmission(void)
	{
		Command(kStopTransmission, true, false);
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
	void Command(SdCommand c, bool wait, bool stream);
	void CommandArgument(SdCommand c, unsigned int a, bool wait, bool stream);
	void CommandLineReset(void);
	void ClockFrequencyChange(int divider);
	unsigned int Response(unsigned int word = 0);
	unsigned int Status(void);

	volatile unsigned int *m_pBaseAddress;
	int m_controllerInstance;

	static const unsigned int sm_clockReference = 96;
	static const unsigned int sm_initSeqClock = sm_clockReference * 1000 / 80;
	static const unsigned int sm_400kHzClock = sm_clockReference * 1000 / 400;

	static const unsigned int sm_sysConfig = 0x110 >> 2;
	static const unsigned int sm_sysStatus = 0x114 >> 2;
	static const unsigned int sm_csre = 0x124 >> 2;
	static const unsigned int sm_con = 0x12c >> 2;
	static const unsigned int sm_arg = 0x208 >> 2;
	static const unsigned int sm_cmd = 0x20c >> 2;
	static const unsigned int sm_pstate = 0x224 >> 2;
	static const unsigned int sm_hctl = 0x228 >> 2;
	static const unsigned int sm_sysctl = 0x22c >> 2;
	static const unsigned int sm_stat = 0x230 >> 2;
	static const unsigned int sm_capa = 0x240 >> 2;
	static const unsigned int sm_curCapa = 0x248 >> 2;
};

} /* namespace OMAP4460 */
#endif /* MMCI_H_ */
