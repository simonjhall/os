/*
 * MMCi.cpp
 *
 *  Created on: 17 Jun 2013
 *      Author: simon
 */

#include "MMCi.h"
#include "print_uart.h"
#include "virt_memory.h"

namespace OMAP4460
{

MMCi::MMCi(volatile void *pBaseAddress, int i)
: SD(),
  m_pBaseAddress ((unsigned int *)pBaseAddress),
  m_controllerInstance (i)
{
	// TODO Auto-generated constructor stub

}

MMCi::~MMCi()
{
	// TODO Auto-generated destructor stub
}

bool MMCi::Reset(void)
{
	Printer &p = Printer::Get();
	volatile unsigned int *pPhys;
	VirtMem::VirtToPhys(m_pBaseAddress, &pPhys);
	p << "base address is " << m_pBaseAddress << " phys addr " << pPhys << "\n";
	p << "sysconfig address is " << &m_pBaseAddress[sm_sysConfig] << "\n";

	/* SOFTWARE RESET OF THE CONTROLLER 24.5.1.1.2.2 */

	//soft reset, set sysconfig[1]=1
	m_pBaseAddress[sm_sysConfig] = m_pBaseAddress[sm_sysConfig] | 2;

	//check that it's done, check sysstatus[0]=1
	int timeout = 100000;
	while (!(m_pBaseAddress[sm_sysStatus] & 1))
	{
		p << "sys status " << m_pBaseAddress[sm_sysStatus] << "\n";
		timeout--;

		if (!timeout)
			return false;
	}

	//reset sysctl - U-BOOT
	m_pBaseAddress[sm_sysctl] |= (1 << 24);

	timeout = 100000;
	while (m_pBaseAddress[sm_sysctl] & (1 << 24))
	{
		p << "sys ctl " << m_pBaseAddress[sm_sysctl] << "\n";
		timeout--;

		if (!timeout)
			return false;
	}

	/* SET MODULE HARDWARE CAPABILITIES 24.5.1.1.2.3 */

	unsigned int capabilities = m_pBaseAddress[sm_capa];
	unsigned int current_capabilities = m_pBaseAddress[sm_curCapa];

	if (capabilities & (1 << 21))
		p << "high speed supported\n";

	if (capabilities & (1 << 26))
		p << "1.8v supported\n";
	else
		p << "1.8v NOT supported\n";

	if (capabilities & (1 << 25))
		p << "3.0v supported\n";
	else
		p << "3.0v NOT supported\n";

	if (capabilities & (1 << 24))
		p << "3.3v supported\n";
	else
		p << "3.3v NOT supported\n";

	p << "max current for 1.8v: " << ((current_capabilities >> 16) & 0xff) << "\n";
	p << "max current for 3.0v: " << ((current_capabilities >> 8) & 0xff) << "\n";
	p << "max current for 3.3v: " << (current_capabilities & 0xff) << "\n";

//	m_pBaseAddress[sm_curCapa] = 0xffffff;
	current_capabilities = m_pBaseAddress[sm_curCapa];

	p << "max current for 1.8v: " << ((current_capabilities >> 16) & 0xff) << "\n";
	p << "max current for 3.0v: " << ((current_capabilities >> 8) & 0xff) << "\n";
	p << "max current for 3.3v: " << (current_capabilities & 0xff) << "\n";

	/* SET MODULE IDLE AND WAKE-UP MODES 24.5.1.1.2.4 */

	//set wake-up bit
	m_pBaseAddress[sm_sysConfig] |= (1 << 2);
	//enable wake-up events on SD card interrupt
	m_pBaseAddress[sm_hctl] |= (1 << 24);

	/* MMC HOST AND BUS CONFIGURATION 24.5.1.1.2.5 */

	//no need to turn on open drain (OD), 8-bit mode (DW8), or CE-ATA

	//set voltage (sdvs)
	m_pBaseAddress[sm_hctl] |= (6 << 9);		//3.0v

	//set the data transfer width (dtw) to 4-bit
//	m_pBaseAddress[sm_hctl] |= 2;

	//turn on bus power (sdbp)
	m_pBaseAddress[sm_hctl] |= (1 << 8);

	//check sdvs is compliant
	if (!(m_pBaseAddress[sm_hctl] & (1 << 8)))
	{
		p << "unhappy voltage\n";
		return false;
	}

	//turn on the internal clock; sysctl[0]
	m_pBaseAddress[sm_sysctl] |= 1;

	//set the clock rate
	m_pBaseAddress[sm_sysctl] |= ((sm_initSeqClock / 2) << 6);

	p << "sysctl init " << m_pBaseAddress[sm_sysctl] << "\n";

	//check the clock is stable; sysctl[1]
	while (!(m_pBaseAddress[sm_sysctl] & 2))
		p << "waiting for clock to go stable\n";

	p << "sysctl post init " << m_pBaseAddress[sm_sysctl] << "\n";

	//set clockactivity
	m_pBaseAddress[sm_sysConfig] = m_pBaseAddress[sm_sysConfig] | (3 << 8);
	//sidlemode
	m_pBaseAddress[sm_sysConfig] = (m_pBaseAddress[sm_sysConfig] & ~(3 << 3)) | (1 << 3);
	//autoidle
	m_pBaseAddress[sm_sysConfig] = m_pBaseAddress[sm_sysConfig] & ~1;

	//set data timeout
	m_pBaseAddress[sm_sysctl] |= (5 << 16);

	m_pBaseAddress[sm_ie] = 1 | (1 << 1) | (1 << 4) | (1 << 5)
			| (1 << 16) | (1 << 17) | (1 << 18)
			| (1 << 19) | (1 << 20) | (1 << 21)
			| (1 << 22) | (1 << 28) | (1 << 29);

	/* MMC/SD/SDIO CONTROLLER CARD IDENTIFICATION AND SELECTION 24.5.1.2.1.1 */

	//sent initialisation stream; con[1]=1
	m_pBaseAddress[sm_con] |= 2;

	//write 0 to cmd
	m_pBaseAddress[sm_cmd] = 0;

//	//wait 1ms
//	DelayMillisecond();

	timeout = 100000;
	while (!(m_pBaseAddress[sm_stat] & 1))
	{
		timeout--;
		if (!timeout)
		{
			p << "stat is " << m_pBaseAddress[sm_stat] << "\n";
			return false;
		}
	}

	//clear flag; stat[0]=1
	m_pBaseAddress[sm_stat] |= 1;

	//end init sequence; con[1]=0
	m_pBaseAddress[sm_con] &= ~2;

	//clear stat register
	m_pBaseAddress[sm_stat] = 0xffffffff;

	timeout = 100000;
	while (m_pBaseAddress[sm_pstate] & 1)
	{
		timeout--;
		if (!timeout)
		{
			p << "pstate is " << m_pBaseAddress[sm_pstate] << "\n";
			return false;
		}
	}

	ClockFrequencyChange(/*sm_400kHzClock*/5);

	p << "going idle state\n";
	GoIdleState();

	p << "sleep awake\n";
	Command(kSleepAwake, kNoResponse, false);

	timeout = 100000;
	while (1)
	{
		//check for SDIO
		if (m_pBaseAddress[sm_stat] & 1)
			p << "SDIO\n";

		//check for timeout
		if (m_pBaseAddress[sm_stat] & (1 << 16))
		{
			timeout--;
			if (!timeout)
				return false;
		}
		else
			break;
	}

//	p << "doing voltage check\n";
//	Command(kVoltageCheck, k48bResponse, false, (1 << 8) | 0xaa);
//	unsigned int resp = Response();
//
//	p << "voltage check response is " << resp << "\n";

	return true;
}


void MMCi::Command(SdCommand c, ::Response wait, bool stream, unsigned int a)
{
	Printer &p = Printer::Get();

	unsigned int w = 0;
	switch (wait)
	{
		case kNoResponse:
			w = 0; break;
		case k136bResponse:
			w = 1; break;
		case k48bResponse:
			w = 2; break;
		case k48bResponseBusy:
			w = 3; break;
		default:
			ASSERT(0);
	}

	//wait for the command line to stop being in use; pstate[0]=0
	while (m_pBaseAddress[sm_pstate] & 1);
//	p << "waiting for the line to stop\n";
//		p << "pstate is " << m_pBaseAddress[sm_pstate];

	//set the stream command
//	m_pBaseAddress[sm_con] = (m_pBaseAddress[sm_con] & ~(1 << 3)) /*| ((unsigned int)stream << 3) */| (1 << 6);

	//clear error
	m_pBaseAddress[sm_csre] = 0;

	//u-boot clear status
//	p << "stat was " << m_pBaseAddress[sm_stat] << "\n";
	m_pBaseAddress[sm_stat] = 0xffffffff;

	//write argument
//	p << "argument is " << a << "\n";
	m_pBaseAddress[sm_arg] = a;

	//send the command
	//cmd_type[22]=0 for normal command
	unsigned int command = ((unsigned int)c << 24) | ((unsigned int)w << 16);		//command and wait

	if (c == kReadMultipleBlock)
	{
		command |= (1 << 5);			//multi-block, but leave block count enable=0
		command |= (1 << 4);			//card to host, ddir
		command |= (1 << 21);			//data present, dp

		m_pBaseAddress[sm_blk] = 512;	//block size
	}

	if (c == kStopTransmission)
	{
		command |= (3 << 22);			//abort cmd12
	}

	m_pBaseAddress[sm_cmd] = command;

	int count = 0;
	while (1)
	{
		//check status
		unsigned int stat = m_pBaseAddress[sm_stat];
		unsigned int cc = stat & 1;
		unsigned int cto = (stat >> 16) & 1;
		unsigned int ccrc = (stat >> 17) & 1;

		if (cto && ccrc)
		{
			DelayMillisecond();
			CommandLineReset();
			count++;
			if (count > 10)
			{
				p << "conflict on the line\n";
				while(1);
			}
		}
		else if (cto && !ccrc)
		{
			DelayMillisecond();
			CommandLineReset();
			count++;
			if (count > 10)
			{
				p << "line reset; timeout\n";
				while(1);
			}
		}
		else if (cc)
		{
			break;
		}
		else
		{
			/*DelayMillisecond();
			count++;
			if (count > 10)
			{
				p << "command " << c << " did not complete, stat is " << stat << "\n";
				return;
			}*/
		}
	}
}

void MMCi::CommandArgument(SdCommand c, unsigned int a, ::Response wait, bool stream)
{
	Command(c, wait, stream, a);
}

void MMCi::CommandLineReset(void)
{
	m_pBaseAddress[sm_sysctl] |= (1 << 25);

	while (!(m_pBaseAddress[sm_sysctl] & (1 << 25)));
	ASSERT(m_pBaseAddress[sm_sysctl] & (1 << 25));

	while (m_pBaseAddress[sm_sysctl] & (1 << 25));
	ASSERT((m_pBaseAddress[sm_sysctl] & (1 << 25)) == 0);
}

void MMCi::ClockFrequencyChange(int divider)
{
	ASSERT(divider >= 0 && divider <= 1023);

	Printer &p = Printer::Get();
	p << "sysctl was " << m_pBaseAddress[sm_sysctl] << "\n";

	//disable the clock; set sysctl[2]=0
	m_pBaseAddress[sm_sysctl] &= ~4;

	p << "sysctl was " << m_pBaseAddress[sm_sysctl] << "\n";

	//clear the clock
	m_pBaseAddress[sm_sysctl] &= ~(1023 << 6);

	p << "sysctl was " << m_pBaseAddress[sm_sysctl] << "\n";

	//set the clock
	m_pBaseAddress[sm_sysctl] |= (divider << 6);

//	p << "sysctl is " << m_pBaseAddress[sm_sysctl] << "\n";

	//check the clock is stable; sysctl[1]
	while (!(m_pBaseAddress[sm_sysctl] & 2));

	//re-enable the clock; set sysctrl[2]=1
	m_pBaseAddress[sm_sysctl] |= 4;

	p << "sysctl is " << m_pBaseAddress[sm_sysctl] << "\n";
}

unsigned int MMCi::Response(unsigned int word)
{
	return m_pBaseAddress[sm_rsp10 + word];
}

unsigned int MMCi::Status(void)
{
	ASSERT(0);
	return m_pBaseAddress[13];
}


} /* namespace OMAP4460 */
