/*
 * TimeFromBoot.cpp
 *
 *  Created on: 22 Nov 2014
 *      Author: simon
 */

#include "TimeFromBoot.h"
#include "common.h"

namespace TimeFromBoot
{

static PeriodicTimer *s_pMasterTimer = 0;
volatile unsigned int s_seconds;

void Init(PeriodicTimer *pTimer, InterruptController *pPic)
{
	ASSERT(pTimer);
	ASSERT(pPic);

	s_pMasterTimer = pTimer;
	s_seconds = 0;

	bool ok = s_pMasterTimer->SetFrequencyInMicroseconds(1000 * 1000);
	ASSERT(ok);

	pPic->RegisterInterrupt(*s_pMasterTimer, InterruptController::kIrq);
	s_pMasterTimer->OnInterrupt([](InterruptSource &)
			{
				s_seconds++;
			});

	s_pMasterTimer->Enable(true);
}

unsigned long long GetMicroseconds(void)
{
	ASSERT(s_pMasterTimer);

	unsigned int result;
	unsigned int orig_seconds;

	do
	{
		orig_seconds = s_seconds;
		result = orig_seconds * 1000000 + (1000000 - s_pMasterTimer->GetMicrosUntilFire());
	}
	while (s_seconds !=  orig_seconds);

	return result;
}

}
