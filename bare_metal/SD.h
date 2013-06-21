/*
 * SD.h
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#ifndef SD_H_
#define SD_H_

#include "BlockDevice.h"

/*
http://rtds.cs.tamu.edu/web_462/techdocs/ARM/soc/DDI0205B_mmci_trm.pdf
http://www.eetasia.com/ARTICLES/2004NOV/A/2004NOV26_MSD_AN.PDF
*/

enum SdCommand
{
	kGoIdleState = 0,
	kSendOpCmd = 1,
	kAllSendCid = 2,
	kSendRelativeAddr = 3,
	kSendDsr = 4,
	kSleepAwake = 5,
	kSwitchFunction = 6,
	kSelectDeselectCard = 7,
	kVoltageCheck = 8,
	kSendCsd = 9,
	kSendCid = 10,
	kReadDatUntilStop = 11,
	kStopTransmission = 12,
	kSendStatus = 13,
	kGoInactiveState = 15,

	kSdAppOpCond = 41,
	kSdAppCmd = 55,
};

enum Response
{
	kNoResponse,
	k48bResponse,
	k136bResponse,
};

class SD : public BlockDevice
{
public:
	SD();
	virtual ~SD();

	virtual void GoIdleState(void) = 0;
	virtual bool GoReadyState(void) = 0;
	virtual bool GoIdentificationState(void) = 0;
	virtual void GoInactiveState(void) = 0;
	virtual bool GoStandbyState(unsigned int rca) = 0;
	virtual bool GoTransferState(unsigned int rca) = 0;

	virtual bool GetCardRcaAndGoStandbyState(unsigned int &rRca) = 0;

protected:
	enum State
	{
		kIdleState,
		kReadyState,
		kInactiveState,
		kIdentificationState,
		kStandbyState,
		kTransferState,
		kSendingDataState,
		kReceiveDataState,
		kProgrammingState,
	};

	void SetState(State s)
	{
		m_cardState = s;
	}

	State GetState(void)
	{
		return m_cardState;
	}

private:
	State m_cardState;
};

#endif /* SD_H_ */
