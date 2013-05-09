/*
 * svc.cpp
 *
 *  Created on: 8 May 2013
 *      Author: simon
 */

#include <string.h>
#include "print_uart.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>

extern "C" unsigned int SupervisorCall(unsigned int r7, const unsigned int * const pRegisters)
{
	PrinterUart p;
	p.PrintString("supervisor call ");
	p.Print(r7);
	p.PrintString("\r\n");

	for (int count = 0; count < 7; count++)
	{
		p.PrintString("\t");
		p.Print(pRegisters[count]);
		p.PrintString("\r\n");
	}

	switch (r7)
	{
	case 122:	//sys_newuname
	{
		static const int sLength = 64;
		struct new_utsname {
			char sysname[sLength + 1];
			char nodename[sLength + 1];
			char release[sLength + 1];
			char version[sLength + 1];
			char machine[sLength + 1];
			char domainname[sLength + 1];
		};
		new_utsname *pDest = (new_utsname *)pRegisters[0];

		strncpy(pDest->domainname, "domainname", sLength + 1);
		strncpy(pDest->machine, "machine", sLength + 1);
		strncpy(pDest->nodename, "nodename", sLength + 1);
		strncpy(pDest->release, "2.6.32", sLength + 1);
		strncpy(pDest->sysname, "sysname", sLength + 1);
		strncpy(pDest->version, "version", sLength + 1);

		return 0;
	}
	case 5:			//compat_sys_open
	{
		const char *pFilename = (char *)pRegisters[0];
		int flags = (int)pRegisters[1];
		mode_t mode = (mode_t)pRegisters[2];

		p.PrintString("sys_open ");
		p.PrintString(pFilename);
		p.PrintString("\r\n");

		return -1;
	}
	case 146:		//compat_sys_writev
	{
		struct iovec *pIov = (struct iovec *)pRegisters[1];
		int iovcnt = (int)pRegisters[2];

		size_t written = 0;

		for (int count = 0; count < iovcnt; count++)
		{
			p.PrintString((char *)pIov[count].iov_base);
			written += pIov[count].iov_len;
		}

		return written;
	}
	case 1:			//sys_exit
	case 20:		//sys_getpid
	case 45:		//sys_brk
	case 174:		//compat_sys_rt_sigaction
	case 175:		//compat_sys_rt_sigprocmask
	case 192:		//sys_mmap_pgoff
	case 248:		//sys_exit_group
	case 268:		//sys_tgkill
	case 281:		//sys_socket
	case 0xf0005:	//__ARM_NR_compat_set_tls
		return (unsigned int)-4095;
	default:
		return (unsigned int)-4095;
	}
}


