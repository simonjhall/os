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
#include "print_uart.h"

namespace OMAP4460
{

class MMCi: public SD
{
public:
	MMCi(volatile void *pBaseAddress, int i);
	virtual ~MMCi();

	virtual void GoIdleState(void)
	{
		Command(kGoIdleState, kNoResponse, false);
		SetState(kIdleState);
	}

	virtual bool GoReadyState(void)
	{
		if (GetState() != kIdleState)
			return false;

		PrinterUart<PL011> p;

		const unsigned int acceptable_voltages = (1 << 21) | (1 << 20);
//
//		unsigned int ocr = acceptable_voltages;
//		unsigned int true_ocr;
//		int times = 0;
//
//		do
//		{
//			p << "about to send ocr of " << ocr << "\n";
//			ocr = SendOcr(ocr & acceptable_voltages & 0xff8000);
//			p << "ocr is " << ocr << "\n";
//			true_ocr = m_pBaseAddress[sm_rsp10];
//			p << "response is " << true_ocr << "\n";
//			p << "stat is " << m_pBaseAddress[sm_stat] << "\n";
//			DelaySecond();
//
//			times++;
//			if (times == 3)
//				while(1);
//		}
//		while (!(true_ocr >> 31));
		unsigned int ocr = SendOcr(0);
		p << "first ocr is " << ocr << "\n";
		ocr = SendOcr(ocr);
		p << "second ocr is " << ocr << "\n";

		while(1);


		if (ocr & (1 << 31))
		{
			SetState(kReadyState);
			return true;
		}
		else
		{
			SetState(kInactiveState);
			return false;
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
		Command(kGoInactiveState, kNoResponse, false);
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
	virtual unsigned int SendOcr(int a)
	{
		PrinterUart<PL011> p;
//		Command(kSendOpCmd, true, false);
//		unsigned int resp = Response();
//		p << "response is " << resp << "\n";
//		return resp;
		p << "sending sd app cmd\n";
		CommandArgument(kSdAppCmd, 0, k48bResponse, false);
		unsigned int resp10 = Response(0);
		unsigned int resp32 = Response(1);
		p << "response is " << resp32 << resp10 << "\n";
		if (resp10 & (1 << 5))
		{
			DelayMillisecond();
			//fake voltage
			CommandArgument(kSdAppOpCond, a, k48bResponse, false);
			DelayMillisecond();
			return Response();
		}
		else
			p << "no sd app cmd\n";

		return -1;
	}

	virtual unsigned int AllSendCid(void)
	{
		Command(kAllSendCid, k136bResponse, false);
		return Response(0);
	}

	virtual unsigned int SendRelativeAddr(void)
	{
		Command(kSendRelativeAddr, k48bResponse, false);
		return Response();
	}

	virtual unsigned int SelectDeselectCard(unsigned int rca)
	{
		CommandArgument(kSelectDeselectCard, rca << 16, k48bResponse, false);
		return Response();
	}

	virtual void ReadDataUntilStop(unsigned int address)
	{
		ASSERT((address & 511) == 0);
		CommandArgument(kReadDatUntilStop, address, kNoResponse, true);
	}

	virtual unsigned int StopTransmission(void)
	{
		Command(kStopTransmission, k48bResponse, false);
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
	void Command(SdCommand c, ::Response wait, bool stream, unsigned int a = 0);
	void CommandArgument(SdCommand c, unsigned int a, ::Response wait, bool stream);
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
	static const unsigned int sm_rsp10 = 0x210 >> 2;
	static const unsigned int sm_rsp32 = 0x214 >> 2;
	static const unsigned int sm_rsp54 = 0x218 >> 2;
	static const unsigned int sm_rsp76 = 0x21c >> 2;
	static const unsigned int sm_pstate = 0x224 >> 2;
	static const unsigned int sm_hctl = 0x228 >> 2;
	static const unsigned int sm_sysctl = 0x22c >> 2;
	static const unsigned int sm_stat = 0x230 >> 2;
	static const unsigned int sm_ie = 0x234 >> 2;
	static const unsigned int sm_capa = 0x240 >> 2;
	static const unsigned int sm_curCapa = 0x248 >> 2;
};

} /* namespace OMAP4460 */
#endif /* MMCI_H_ */
