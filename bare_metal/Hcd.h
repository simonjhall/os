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

	bool m_used[128];

public:
	virtual bool Initialise(void) = 0;
	virtual void Shutdown(void) = 0;

	virtual int AllocateBusAddress(void);
	virtual void ReleaseBusAddress(int);

	virtual bool SubmitControlMessage(EndPoint &rEndPoint, UsbDevice &rDevice, void *pBuffer, unsigned int length, SetupPacket) = 0;

	virtual Hub &GetRootHub(void) = 0;
};

} /* namespace USB */
#endif /* HCD_H_ */
