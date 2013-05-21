/*
 * svc.cpp
 *
 *  Created on: 8 May 2013
 *      Author: simon
 */

#include <string.h>

#include "print_uart.h"
#include "common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <unistd.h>

extern unsigned int stored_state;

extern unsigned int _binary_libgcc_s_so_1_start;
extern unsigned int _binary_libgcc_s_so_1_size;

extern "C" unsigned int SupervisorCall(unsigned int r7, const unsigned int * const pRegisters)
{
	PrinterUart<PL011> p;
	bool known = false;
	const char *pName = 0;

	/*p.PrintString("svc from ");
	p.Print(stored_state);
	p.PrintString(" code ");
	p.Print(r7);
	p.PrintString("\r\n");*/

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

		if (strcmp(pFilename, "/usr/local/lib/libgcc_s.so.1") == 0)
		{
			static int fd = 3;
			return fd++;
		}
		else
			p.PrintString("UNIMPLEMENTED OPEN\r\rn");

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
	case 45:		//sys_brk
	{
		unsigned int current = (unsigned int)GetHighBrk();

		if (pRegisters[0] != 0)		//move it
		{
			unsigned int want = pRegisters[0];
			if (want > current)
			{
				unsigned int change = want - current;
				//round up to a page
				change = (change + 4095) & ~4095;
				//count the pages
				unsigned int num_pages = change >> 12;

//				EnableMmu(false);

				for (unsigned int count = 0; count < num_pages; count++)
				{
					void *pPhys = PhysPages::FindPage();
					if (pPhys == (void *)-1)
						break;

					if (!VirtMem::MapPhysToVirt(pPhys, (void *)current, 4096, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
					{
						PhysPages::ReleasePage((unsigned int)pPhys >> 12);
						break;
					}

					//zero it
					memset((void *)current, 0, 4096);

					current += 4096;
				}

				SetHighBrk((void *)current);
//				EnableMmu(true);
			}
		}

		return current;
	}
	case 0xf0005:	//__ARM_NR_compat_set_tls
	{
		*(unsigned int *)(0xffff0fe0 - 4) = pRegisters[0];
		asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (pRegisters[0]) : "cc");
		return 0;
	}
	case 4:			//write
	{
		unsigned int fd = pRegisters[0];
		unsigned char *pBuf = (unsigned char *)pRegisters[1];
		unsigned int len = pRegisters[2];

		if (fd == 1 || fd == 2)	//stdout or stderr
		{
			for (unsigned int count = 0; count < len; count++)
				p.PrintChar(pBuf[count]);
			return len;
		}
		else
			p.PrintString("UNIMPLEMENTED WRITE\r\rn");

		return -1;
	}
	case 3:			//read
	{
		unsigned int fd = pRegisters[0];
		unsigned char *pBuf = (unsigned char *)pRegisters[1];
		unsigned int len = pRegisters[2];

		if (fd == 0)	//stdin
		{
			for (unsigned int count = 0; count < len; count++)
			{
				unsigned char c;
				while (PL011::ReadByte(c) == false);
				pBuf[count] = c;
			}
			return len;
		}
		else if (fd == 3)	//libgcc
		{
			static unsigned int offset = 0;
			unsigned char *c = (unsigned char *)&_binary_libgcc_s_so_1_start;
			for (unsigned int count = 0; count < len; count++)
			{
				pBuf[count] = c[offset + count];
			}

			offset += len;
			return len;
		}
		else
			p.PrintString("UNIMPLEMENTED READ\r\rn");

		return -1;
	}

	case 1:			//sys_exit
	{
//		if (!pName)
//			pName = "sys_exit";
		p.PrintString("sys_exit");
		ASSERT(0);
		return 0;
	}
	case 192:		//sys_mmap_pgoff
	{
		void *pDest = (void *)pRegisters[0];
		unsigned int length = (pRegisters[1] + 4095) & ~4095;
		unsigned int length_pages = length >> 12;
		int file = (int)pRegisters[4];
		int off = (int)pRegisters[5] << 12;

		ASSERT(((unsigned int)pDest >> 12) == 0);

		static unsigned int highZero = 0x60000000;
		bool usingHigh = false;

		if (pDest == 0)
		{
			pDest = (void *)highZero;
			highZero += length;
			usingHigh = true;
		}

		if (file == -1)
		{
			void *to_return = pDest;

			for (unsigned int count = 0; count < length_pages; count++)
			{
				void *pPhys = PhysPages::FindPage();
				if (pPhys == (void *)-1)
				{
//					if (usingHigh)
//						highZero -= l
					ASSERT(0);
					return -1;
				}

				if (!VirtMem::MapPhysToVirt(pPhys, pDest, 4096, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
				{
					ASSERT(0);
					return -1;
				}

				pDest = (void *)((unsigned int)pDest + 4096);
			}

			return (unsigned int)to_return;
		}
		else if (file == 3)			//gcc
		{
			void *to_return = pDest;
			void *pVirtSource = (void *)&_binary_libgcc_s_so_1_start;

			pVirtSource = (void *)((unsigned int)pVirtSource + off);

			for (unsigned int count = 0; count < length_pages; count++)
			{
				void *pPhys;
				bool ok = VirtMem::VirtToPhys(pVirtSource,&pPhys);

				if (!ok)
					return -ENOMEM;

				//map it in
				ok = VirtMem::MapPhysToVirt(pPhys, pDest, 4096, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0);

				if (!ok)
					return -ENOMEM;

				pVirtSource = (void *)((unsigned int)pVirtSource + 4096);
				pDest = (void *)((unsigned int)pDest + 4096);
			}

			return (unsigned int)to_return;
		}
		else
			p.PrintString("UNIMPLEMENTED mmap\r\rn");

		return -1;
	}
	case 125:		//mprotect
	{
		void *pVaddr = (void *)pRegisters[0];
		unsigned int len = pRegisters[1];
		int prot = pRegisters[2];

		if (len & 4095)
			return -EINVAL;

		void *pPhys;
		bool ok = VirtMem::VirtToPhys(pVaddr, &pPhys);

		if (!ok)
			return -ENOMEM;

		p.PrintString("unimplemented mprotect\r\n");
		return 0;
	}
	case 20:		//sys_getpid
	{
		p.PrintString("UNIMPLEMENTED GETPID\r\rn");
		return 0;
	}
	case 195:		//stat64
	{
		char *pFilename = (char *)pRegisters[0];
		struct stat *pBuf = (struct stat *)pRegisters[1];

		if (!pFilename || !pBuf)
			return -EFAULT;

		if (strcmp(pFilename, "/usr/local/lib/libgcc_s.so.1") == 0)
		{
			memset(pBuf, 0, sizeof(struct stat));
			pBuf->st_size = *(unsigned int *)&_binary_libgcc_s_so_1_size;
			return 0;
		}
		else
			p.PrintString("UNIMPLEMENTED STAT\r\rn");

		return -EBADF;
	}
	case 197:		//fstat64
	{
		int handle = (int)pRegisters[0];
		struct stat *pBuf = (struct stat *)pRegisters[1];

		if (!pBuf)
			return -EFAULT;

		if (handle == 3)
		{
			memset(pBuf, 0, sizeof(struct stat));
			pBuf->st_size = (unsigned int)&_binary_libgcc_s_so_1_size;
			return 0;
		}
		else
			p.PrintString("UNIMPLEMENTED FSTAT\r\rn");

		return -EBADF;
	}
	case 6:			//close
		if (!pName)
			pName = "close";
	case 19:		//lseek
		if (!pName)
			pName = "lseek";
	case 33:		//access
		if (!pName)
			pName = "access";
	case 78:		//compat_sys_gettimeofday
		if (!pName)
			pName = "compat_sys_gettimeofday";
	case 174:		//compat_sys_rt_sigaction
		if (!pName)
			pName = "compat_sys_rt_sigaction";
	case 175:		//compat_sys_rt_sigprocmask
		if (!pName)
			pName = "compat_sys_rt_sigprocmask";
	case 199:		//getuid32
		if (!pName)
			pName = "getuid32";
	case 200:		//getgid32
		if (!pName)
			pName = "getgid32";
	case 201:		//sys_geteuid
		if (!pName)
			pName = "sys_geteuid";
	case 202:		//getegid32
		if (!pName)
			pName = "getegid32";
	case 248:		//sys_exit_group
		if (!pName)
			pName = "sys_exit_group";
	case 268:		//sys_tgkill
		if (!pName)
			pName = "sys_tgkill";
	case 281:		//sys_socket
		if (!pName)
			pName = "sys_socket";

		p.PrintString("UNIMPLEMENTED ");
		p.PrintString(pName);
		known = true;
		/* no break */
	default:
		if (!known)
			p.PrintString("UNKNOWN");
		p.PrintString("\r\n");
		p.PrintString("supervisor call ");
		p.Print(r7);
		p.PrintString("\r\n");

		for (int count = 0; count < 7; count++)
		{
			p.PrintString("\t");
			p.Print(pRegisters[count]);
			p.PrintString("\r\n");
		}
		return (unsigned int)-4095;
	}
}


