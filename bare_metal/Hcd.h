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

	bool SubmitControlMessage(EndPoint &rEndPoint, void *pBuffer, unsigned int length, SetupPacket);

public:
	int AllocateBusAddress(void);
	void ReleaseBusAddress(int);
};

} /* namespace USB */
#endif /* HCD_H_ */
