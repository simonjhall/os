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

		p << "doing voltage check\n";
		Command(kVoltageCheck, k48bResponse, false, (1 << 8) | 0xaa);
		unsigned int resp = Response();

		/////if this times out then it's not SD 2.0 - and we shouldn't set bit 30
		p << "voltage check response is " << resp << "\n";

		const unsigned int acceptable_voltages = (1 << 21) | (1 << 20);

		int timeout = 1000;

		//get the initial reading
		unsigned int ocr = SendOcr(0);
		p << "first ocr is " << m_pBaseAddress[sm_rsp32] << m_pBaseAddress[sm_rsp10] << "\n";
		do
		{
			ocr = SendOcr((ocr & acceptable_voltages) | (1 << 30));
			timeout--;

			if (!timeout)
				break;
		} while (!(ocr & (1 << 31)));
		p << "second ocr is " << m_pBaseAddress[sm_rsp32] << m_pBaseAddress[sm_rsp10] << "\n";

		if (ocr & (1 << 31))
		{
			SetState(kReadyState);
			return true;
		}
		else
		{
			p << "card failed to enable\n";
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
		PrinterUart<PL011> p;

		if (!kIdentificationState)
			return false;

		unsigned int rca_status = SendRelativeAddr();
		SetState(kStandbyState);

		p << "rca status is " << rca_status << "\n";

		rRca = rca_status >> 16;
		return true;
	}

	virtual bool GoTransferState(unsigned int rca)
	{
		State current = GetState();

		PrinterUart<PL011> p;
		/*p << "deselecting card\n";
		SelectDeselectCard(0);
		p << "done\n";*/

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
		ASSERT((address & 3) == 0);
		if (GetState() != kTransferState)
			return false;

//		PrinterUart<PL011> p;
//		p << "sd read from logical address " << address << " byte count " << byteCount << "\n";

		//read the slack into a dummy buffer
		unsigned int left_in_pump = 0;
		if (address & 511)
		{
			char dummy[512];

//			p << "read until stop high\n";

			ReadDataUntilStop(address & ~511);

//			p << "read out data high\n";
			left_in_pump = ReadOutData(dummy, address & 511, 0);
		}
		else
		{
//			p << "read data until stop, address " << address << "\n";
			ReadDataUntilStop(address);
		}

//		p << "read out data\n";
		ReadOutData(pDest, byteCount, left_in_pump);
//		p << "data read ok\n";

		//reset the data lines
		m_pBaseAddress[sm_sysctl] |= (1 << 26);
		while (!(m_pBaseAddress[sm_sysctl] & (1 << 26)));
		while (m_pBaseAddress[sm_sysctl] & (1 << 26));
		StopTransmission();

		return true;
	}

	virtual bool Reset(void);

protected:
	virtual unsigned int SendOcr(int a)
	{
		PrinterUart<PL011> p;
//		Command(kSendOpCmd, k48bResponse, false);
//		unsigned int resp = Response();
//		p << "response is " << resp << "\n";
//		return resp;
//		p << "sending sd app cmd\n";
		CommandArgument(kSdAppCmd, 0, k48bResponse, false);
		unsigned int resp10 = Response(0);
		/*unsigned int resp32 =*/ Response(1);
//		p << "response is " << resp32 << resp10 << "\n";
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
		CommandArgument(kSelectDeselectCard, rca << 16, k48bResponseBusy, false);
		return Response();
	}

	virtual void ReadDataUntilStop(unsigned int address)
	{
		ASSERT((address & 511) == 0);
		CommandArgument(kReadMultipleBlock, address >> 9, kNoResponse, true);
	}

	virtual unsigned int StopTransmission(void)
	{
		Command(kStopTransmission, k48bResponseBusy, false);
		return Response();
	}

	virtual void PumpBlock(void)
	{
		PrinterUart<PL011> p;

//		p << "pump block\n";

		unsigned int status;
		int timeout = 1000000;
		do
		{
			status = 0;
			//get status
			while (!status)
			{
				status = m_pBaseAddress[sm_stat];
				timeout--;

				if (!timeout)
				{
					PrinterUart<PL011> p;
					p << "data read timeout\n";
					while(1);
				}
			}

//			p << "status is " << status << "\n";

		} while (!(status & (1 << 5)));			//while buffer read ready bit not set

//		p << "pumped, " << "status is " << status << "\n";
	}

	virtual unsigned int ReadOutData(void *pDest, unsigned int byteCount, unsigned int bytesLeftInPump)
	{
		PrinterUart<PL011> p;

		unsigned int outA = (unsigned int)pDest;
		unsigned char *pBase = (unsigned char *)&m_pBaseAddress[sm_data];
		unsigned int smallOffset = 0;

		while (byteCount)
		{
			if (!bytesLeftInPump)
			{
				PumpBlock();
				bytesLeftInPump = 512;
				m_pBaseAddress[sm_stat] |= (1 << 5);
			}

			if (byteCount >= 4)
			{
				*(unsigned int *)outA = m_pBaseAddress[sm_data];
//				p << "read out " << *(unsigned int *)outA << "\n";
				outA += 4;
				byteCount -= 4;
				bytesLeftInPump -= 4;
			}
			else
			{
				p << "small\n";
				ASSERT(smallOffset < 4);
				*(unsigned char *)outA = pBase[smallOffset];
				smallOffset++;
				outA++;
				byteCount--;
				bytesLeftInPump--;
			}
		}
		return bytesLeftInPump;
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
	static const unsigned int sm_blk = 0x204 >> 2;
	static const unsigned int sm_arg = 0x208 >> 2;
	static const unsigned int sm_cmd = 0x20c >> 2;
	static const unsigned int sm_rsp10 = 0x210 >> 2;
	static const unsigned int sm_rsp32 = 0x214 >> 2;
	static const unsigned int sm_rsp54 = 0x218 >> 2;
	static const unsigned int sm_rsp76 = 0x21c >> 2;
	static const unsigned int sm_data = 0x220 >> 2;
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
