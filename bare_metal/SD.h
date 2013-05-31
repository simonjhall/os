/*
 * SD.h
 *
 *  Created on: 30 May 2013
 *      Author: simon
 */

#ifndef SD_H_
#define SD_H_

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
//	kReadSingleBlock = 17,
//	kReadMultipleBlock = 18,
//	kWriteMultipleBlock = 25,
//	kProgramCid = 26,
//	kProgramCsd = 27,
};

class SD
{
public:
	SD();
	virtual ~SD();

//protected:
	virtual void GoIdleState(void) = 0;
	virtual unsigned int SendOcr(void) = 0;
	virtual unsigned int AllSendCid(void) = 0;
	virtual unsigned int SendRelativeAddr(void) = 0;
	virtual unsigned int SelectDeselectCard(unsigned int rca) = 0;
	virtual void ReadDataUntilStop(unsigned int address) = 0;
	virtual unsigned int StopTransmission(void) = 0;

	virtual void ReadOutData(void *pDest, unsigned int byteCount) = 0;

	virtual void kSdAppOpCondUNSURE(unsigned int voltage) = 0;
};

#endif /* SD_H_ */
