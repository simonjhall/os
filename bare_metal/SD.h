/*
 * SD.h
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#ifndef SD_H_
#define SD_H_

#include "BlockDevice.h"

enum SdCommand
{
	kGoIdleState = 0,
	kSendOpCmd = 1,
	kAllSendCid = 2,
	kSendRelativeAddr = 3,
	kSendDsr = 4,
	kSwitchFunction = 6,
	kSelectDeselectCard = 7,
	kSendCsd = 9,
	kSendCid = 10,
	kReadDatUntilStop = 11,
	kStopTransmission = 12,
	kSendStatus = 13,
	kGoInactiveState = 15,

	kSdAppOpCond = 41,
	kSdAppCmd = 55,
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