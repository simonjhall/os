/*
 * TimeFromBoot.h
 *
 *  Created on: 22 Nov 2014
 *      Author: simon
 */

#ifndef TIMEFROMBOOT_H_
#define TIMEFROMBOOT_H_

#include "PeriodicTimer.h"
#include "InterruptController.h"

namespace TimeFromBoot
{

void Init(PeriodicTimer *, InterruptController *pPic);
unsigned long long GetMicroseconds(void);


}


#endif /* TIMEFROMBOOT_H_ */
