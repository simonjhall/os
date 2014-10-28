/*
 * EndPoint.h
 *
 *  Created on: 8 Aug 2013
 *      Author: simon
 */

#ifndef ENDPOINT_H_
#define ENDPOINT_H_

#include "Usb.h"

namespace USB
{

struct EndPoint
{
	EndPoint(unsigned int maxSize);			//EP zero
	EndPoint(EndpointDescriptor &rOther);

	EndpointDescriptor m_descriptor __attribute__ ((aligned (8)));

	enum TransferType
	{
		kControl = 0,
		kIsochronous = 1,
		kBulk = 2,
		kInterrupt = 3,
	};
};

} /* namespace USB */
#endif /* ENDPOINT_H_ */
