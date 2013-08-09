/*
 * UsbHub.cpp
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#include "UsbHub.h"

namespace USB
{

UsbHub::UsbHub(UsbDevice &rOther)
: Hub(),
  UsbDevice(rOther)
{
	// TODO Auto-generated constructor stub

}

UsbHub::~UsbHub()
{
	// TODO Auto-generated destructor stub
}

} /* namespace USB */
