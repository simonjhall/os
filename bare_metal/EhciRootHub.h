/*
 * EhciRootHub.h
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#ifndef EHCIROOTHUB_H_
#define EHCIROOTHUB_H_

#include "Hub.h"

namespace USB
{

class EhciRootHub: public USB::Hub
{
public:
	EhciRootHub();
	virtual ~EhciRootHub();
};

} /* namespace USB */
#endif /* EHCIROOTHUB_H_ */
