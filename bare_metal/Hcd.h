/*
 * Hcd.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef HCD_H_
#define HCD_H_

#include "Usb.h"

namespace USB
{

class Hcd
{
protected:
	Hcd();
	virtual ~Hcd();

public:
	int AllocateBusAddress(void);
	void ReleaseBusAddress(int);

	bool SubmitControlMessage(EndPoint &rEndPoint, UsbDevice &rDevice, void *pBuffer, unsigned int length, SetupPacket);
};

} /* namespace USB */
#endif /* HCD_H_ */
