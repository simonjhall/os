/*
 * svc.cpp
 *
 *  Created on: 8 May 2013
 *      Author: simon
 */

#include <string.h>

#include "print_uart.h"
#include "common.h"
#include "WrappedFile.h"
#include "VirtualFS.h"
#include "phys_memory.h"
#include "virt_memory.h"
#include "translation_table.h"
#include "Process.h"
#include "Scheduler.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/signal.h>
#include <poll.h>
#include <errno.h>

//extern unsigned int stored_state;

//extern unsigned int _binary_libgcc_s_so_1_start;
//extern unsigned int _binary_libgcc_s_so_1_size;
//unsigned int libgcc_offset = 0;

//extern unsigned int _binary_libc_2_17_so_start;
//extern unsigned int _binary_libc_2_17_so_size;
//unsigned int libc_offset = 0;

//extern ProcessFS *pfsA;
//extern ProcessFS *pfsB;
//extern VirtualFS *vfs;

uid_t g_uid = 0;
gid_t g_gid = 0;

unsigned int Semaphore::WaitInternal(Thread &rBlocked, Thread::State &rNewState)
{
	//already attached, time to unblock
	if (rBlocked.GetEqBlocker() == &m_eq)
	{
		ASSERT(!m_eq.Exists(rBlocked));
		rBlocked.SetEqBlocker(0);

		rNewState = Thread::kRunnable;
		return 0;
	}
	else		//new attachment
	{
		m_value--;

		if (m_value < 0)
		{
			m_eq.AddThread(rBlocked);
			rBlocked.SetEqBlocker(&m_eq);

			rNewState = Thread::kBlockedOnEq;
		}
		else
			rNewState = Thread::kRunnable;

		return (unsigned int)this;
	}
}

void Semaphore::SignalInternal(void)
{
	SignalAtomic();
}

int SupervisorCall(Thread &rBlocked, Process *pParent, Thread::State &rNewState)
{
	Printer &p = Printer::Get();
	bool known = false;
	const char *pName = 0;

	/*p.PrintString("svc from ");
	p.Print(stored_state);
	p.PrintString(" code ");
	p.Print(r7);
	p.PrintString("\n");*/

	unsigned int *pRegisters = rBlocked.m_pausedState.m_regs;


	//http://lxr.free-electrons.com/source/include/uapi/asm-generic/unistd.h
	switch (pRegisters[7])
	{
	case 0xf0000001:
	{
		Semaphore *pSem = (Semaphore *)pRegisters[0];

		return pSem->WaitInternal(rBlocked, rNewState);
	}
	case 0xf0000002:
	{
		Semaphore *pSem = (Semaphore *)pRegisters[0];

		pSem->SignalInternal();
		return 0;
	}


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
		ASSERT(pParent);

		const char *pFilename = (char *)pRegisters[0];
		int flags = (int)pRegisters[1];
		mode_t mode = (mode_t)pRegisters[2];

//		p.PrintString("sys_open ");
//		p.PrintString(pFilename);
//		p.PrintString("\n");

		static const int s_nameLength = 256;
		char filename[s_nameLength];
		BaseDirent *f;
//		f = pParent->m_rVfs.OpenByName(pfsB->BuildFullPath(pFilename, filename, s_nameLength), O_RDONLY);
//		if (f)
//			return pfsB->Open(*f);

		f = pParent->m_rVfs.OpenByName(pParent->m_pfs.BuildFullPath(pFilename, filename, s_nameLength), O_RDONLY);
		if (f)
		{
			int fd = pParent->m_pfs.Open(*f);
//			p << "fd = " << fd << "\n";
			return fd;
		}

		return -1;

//		static int fd = 3;
//
//		if (strcmp(pFilename, "/usr/local/lib/libgcc_s.so.1") == 0)
//		{
//			return fd++;
//		}
//		else if (strcmp(pFilename, "/usr/local/lib/libc.so.6") == 0)
//		{
//			return fd++;
//		}
//		else
//			p.PrintString("UNIMPLEMENTED OPEN\n");
//
//		return -1;
	}
	case 146:		//compat_sys_writev
	{
		struct iovec *pIov = (struct iovec *)pRegisters[1];
		int iovcnt = (int)pRegisters[2];

		size_t written = 0;

		for (int count = 0; count < iovcnt; count++)
		{
			p.PrintString((char *)pIov[count].iov_base, true, pIov[count].iov_len);
			written += pIov[count].iov_len;
		}

		return written;
	}
	case 45:		//sys_brk
	{
		ASSERT(pParent);
		unsigned int current = (unsigned int)pParent->GetBrk();

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

					if (!VirtMem::MapPhysToVirt(pPhys, (void *)current, 4096, false,
							TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
					{
						PhysPages::ReleasePage((unsigned int)pPhys >> 12);
						break;
					}

					//zero it
					memset((void *)current, 0, 4096);

					current += 4096;
				}

				pParent->SetBrk((void *)current);
//				SetHighBrk((void *)current);
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
			{
				char c = pBuf[count];
				if (c == '\n')
				{
#ifdef PBES
					p.PrintChar('\r');
#endif
					p.PrintChar('\n');
				}
				else
					p.PrintChar(c);
			}
			return len;
		}
		else
			p.PrintString("UNIMPLEMENTED WRITE\n");

		return -1;
	}
	case 3:			//read
	{
		ASSERT(pParent);

		unsigned int fd = pRegisters[0];
		unsigned char *pBuf = (unsigned char *)pRegisters[1];
		unsigned int len = pRegisters[2];

//		if (fd == 0)	//stdin
//		{
//			for (unsigned int count = 0; count < len; count++)
//			{
//				unsigned char c;
//				while (PL011::ReadByte(c) == false);
//				pBuf[count] = c;
//			}
//			return len;
//		}
//		else if (fd == 3 || fd == 4)	//libgcc and libc
//		{
//			unsigned char *c;
//			unsigned int *offset;
//
//			if (fd == 3)
//			{
////				c = (unsigned char *)&_binary_libgcc_s_so_1_start;		//no thanks
//				ASSERT(0);
//				offset = &libgcc_offset;
//			}
//			else if (fd == 4)
//			{
////				c = (unsigned char *)&_binary_libc_2_17_so_start;		//no thanks
//				ASSERT(0);
//				offset = &libc_offset;
//			}
//
//			for (unsigned int count = 0; count < len; count++)
//			{
//				pBuf[count] = c[*offset + count];
//			}
//
//			*offset += len;
//			return len;
//		}
//		else
//			p.PrintString("UNIMPLEMENTED READ\n");

		WrappedFile *f = pParent->m_pfs.GetFile(fd);
		if (f)
			return f->Read(pBuf, len);

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
		unsigned int length_unrounded = pRegisters[1];
		unsigned int length_pages = length >> 12;
		int file = (int)pRegisters[4];
		int off = (int)pRegisters[5] << 12;
		int off_pg = (int)pRegisters[5];
		int prot = pRegisters[2];
		int flags = pRegisters[3];

		ASSERT(((unsigned int)pDest & 4095) == 0);
		ASSERT(pParent);

		if (pDest == 0)
			pDest = pParent->GetHighZero();

		/*p.PrintString("mmap of file ");
		p.Print(file);
		p.PrintString(" off ");
		p.Print(off);
		p.PrintString(" len ");
		p.Print(length);
		p.PrintString(" into vaddr ");
		p.Print((unsigned int)pDest);
		p.PrintString(" highzero is ");
		p.Print(highZero);
*/
		if (flags & MAP_ANONYMOUS)
		{
//			void *to_return = pDest;
//
//			for (unsigned int count = 0; count < length_pages; count++)
//			{
//				void *pPhys = PhysPages::FindPage();
//				if (pPhys == (void *)-1)
//				{
////					if (usingHigh)
////						highZero -= l
//					ASSERT(0);
//					return -1;
//				}
//
//				if (!VirtMem::MapPhysToVirt(pPhys, pDest, 4096, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
//				{
//					ASSERT(0);
//					return -1;
//				}
//
//				memset(pDest, 0, 4096);
//
//				pDest = (void *)((unsigned int)pDest + 4096);
//			}
//
//			if ((unsigned int)pDest > highZero)
//				highZero = (unsigned int)pDest;
//
//			/*p.PrintString(" highzero now ");
//			p.Print(highZero);
//			p.PrintString("\n");*/
//
//			return (unsigned int)to_return;

			void *mmap_result = internal_mmap(pDest, length_unrounded, prot, flags, 0, 0, false);
			if (mmap_result != (void *)-1)
			{
				pDest = (void *)((unsigned int)pDest + length);		//rounded up length
				if (pDest > pParent->GetHighZero())
					pParent->SetHighZero(pDest);
			}

			return (unsigned int)mmap_result;
		}
//		else if (file == 3 || file == 4)			//gcc and c
//		{
//			unsigned char *to_return = (unsigned char *)pDest;
//			unsigned char *pVirtSource;
//
//			/*if (file == 3)
//				pVirtSource = (unsigned char *)&_binary_libgcc_s_so_1_start + off;
//			else if (file == 4)
//				pVirtSource = (unsigned char *)&_binary_libc_2_17_so_start + off;*/	//no thanks
////			ASSERT((unsigned int)pVirtSource & 4095);
//
//			ASSERT(0);
//
//			for (unsigned int count = 0; count < length_pages; count++)
//			{
//				void *pPhys = PhysPages::FindPage();
//				if (pPhys == (void *)-1)
//				{
////					if (usingHigh)
////						highZero -= l
//					ASSERT(0);
//					return -1;
//				}
//
//				if (!VirtMem::MapPhysToVirt(pPhys, pDest, 4096, TranslationTable::kRwRw, TranslationTable::kExec, TranslationTable::kOuterInnerWbWa, 0))
//				{
//					ASSERT(0);
//					return -1;
//				}
//
//				pDest = (void *)((unsigned int)pDest + 4096);
//			}
//
//			for (unsigned int count = 0; count < length_unrounded; count++)
//				to_return[count] = pVirtSource[count];
//
//			if ((unsigned int)pDest > highZero)
//				highZero = (unsigned int)pDest;
//
//		/*	p.PrintString(" highzero now ");
//			p.Print(highZero);
//			p.PrintString("\n");*/
//
//			return (unsigned int)to_return;
//		}
//		else
//			p.PrintString("UNIMPLEMENTED mmap\n");
		else
		{
			//do it via the file handle instead
			WrappedFile *f = pParent->m_pfs.GetFile(file);
			if (f)
			{
				void *mmap_result = f->Mmap2(pDest, length_unrounded, prot, flags, off_pg, false);
				if (mmap_result != (void *)-1)
				{
					pDest = (void *)((unsigned int)pDest + length);		//rounded up length
					if (pDest > pParent->GetHighZero())
						pParent->SetHighZero(pDest);
				}

				return (unsigned int)mmap_result;
			}
		}

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

		p.PrintString("unimplemented mprotect\n");
		return 0;
	}
	case 20:		//sys_getpid
	{
//		p.PrintString("UNIMPLEMENTED GETPID\n");
		return 0;
	}
	case 195:		//stat64
	case 196:		//lstat64
	{
		ASSERT(pParent);

		char *pFilename = (char *)pRegisters[0];
		struct stat64 *pBuf = (struct stat64 *)pRegisters[1];

		if (!pFilename || !pBuf)
			return -EFAULT;

//		if (strcmp(pFilename, "/usr/local/lib/libgcc_s.so.1") == 0)
//		{
//			memset(pBuf, 0, sizeof(struct stat64));
////			pBuf->st_size = *(unsigned int *)&_binary_libgcc_s_so_1_size;		//no thanks
//			pBuf->st_ino = 1;
//			ASSERT(0);
//			return 0;
//		}
//		else if (strcmp(pFilename, "/usr/local/lib/libc.so.6") == 0)
//		{
//			memset(pBuf, 0, sizeof(struct stat64));
////			pBuf->st_size = *(unsigned int *)&_binary_libc_2_17_so_size;		//no thanks
//			ASSERT(0);
//			pBuf->st_ino = 2;
//			return 0;
//		}
//		else
//			p.PrintString("UNIMPLEMENTED STAT\n");

		static const int s_nameLength = 256;
		char filename[s_nameLength];
		BaseDirent *f = pParent->m_rVfs.OpenByName(pParent->m_pfs.BuildFullPath(pFilename, filename, s_nameLength), O_RDONLY);

		if (f)
		{
			bool ok = f->Fstat(*pBuf);
			pParent->m_rVfs.Close(*f);

			if (ok)
				return 0;
			else
				ASSERT(0);		//?
		}

		return -EBADF;
	}
	case 197:		//fstat64
	{
		ASSERT(pParent);

		int handle = (int)pRegisters[0];
		struct stat64 *pBuf = (struct stat64 *)pRegisters[1];

		if (!pBuf)
			return -EFAULT;

//		if (handle == 3)
//		{
//			memset(pBuf, 0, sizeof(struct stat64));
////			pBuf->st_size = (unsigned int)&_binary_libgcc_s_so_1_size;	//no thanks
//			ASSERT(0);
//			pBuf->st_ino = 1;
//			return 0;
//		}
//		else if (handle == 4)
//		{
//			memset(pBuf, 0, sizeof(struct stat64));
////			pBuf->st_size = (unsigned int)&_binary_libc_2_17_so_size;	//no thanks
//			ASSERT(0);
//			pBuf->st_ino = 2;
//			return 0;
//		}
//		else
//			p.PrintString("UNIMPLEMENTED FSTAT\n");

		WrappedFile *f = pParent->m_pfs.GetFile(handle);

		if (f)
		{
			bool ok = f->Fstat(*pBuf);

			if (ok)
				return 0;
			else
				ASSERT(0);		//?
		}

		return -EBADF;
	}
	case 6:			//close
	{
		ASSERT(pParent);

		int fd = pRegisters[0];
//		p.PrintString("UNIMPLEMENTED CLOSE\n");
//		if (fd == 3)
//		{
//			libgcc_offset = 0;
//			return 0;
//		}
//		else if (fd == 4)
//		{
//			libc_offset = 0;
//			return 0;
//		}

		if (pParent->m_pfs.Close(fd))
			return 0;

		return -1;
	}
	case 217:		//getdents64
	{
		ASSERT(pParent);

		int handle = (int)pRegisters[0];
		linux_dirent64 *pDir = (linux_dirent64 *)pRegisters[1];
		unsigned int len = pRegisters[2];

		WrappedFile *f = pParent->m_pfs.GetFile(handle);

		if (f)
			return f->Getdents64(pDir, len);

		return -EBADF;
	}
	case 213:		//setuid32
	{
		uid_t uid = (uid_t)pRegisters[0];
		p << "UIMPLEMENTED setuid32 changing uid to " << uid << "\n";
		g_uid = uid;

		return 0;
	}
	case 214:		//setgid32
	{
		gid_t gid = (gid_t)pRegisters[0];
		p << "UIMPLEMENTED setgid32 changing gid to " << gid << "\n";
		g_gid = gid;

		return 0;
	}
	case 168:		//poll
	{
		struct pollfd *fds = (struct pollfd *)pRegisters[0];
		nfds_t n = (nfds_t)pRegisters[1];
		int timeout_ms = (int)pRegisters[2];

		return 0;
	}
	case 199:		//getuid32
	{
		return g_uid;
	}
	case 200:		//getgid32
	{
		return g_gid;
	}
	case 12:		//chdir
	{
		ASSERT(pParent);

		char *pBuf = (char *)pRegisters[0];

		static const int s_nameLength = 256;
		char filename[s_nameLength];
		BaseDirent *f = pParent->m_rVfs.OpenByName(pParent->m_pfs.BuildFullPath(pBuf, filename, s_nameLength), O_RDONLY);

		if (f)
		{
			if (f->IsDirectory())
			{
				pParent->m_rVfs.Close(*f);

				pParent->m_pfs.Chdir(pBuf);
				return 0;
			}
			else
			{
				pParent->m_rVfs.Close(*f);
				return -ENOTDIR;
			}
		}
		else
			return -ENOENT;
	}
	case 183:		//getcwd
	{
		ASSERT(pParent);

		char *pBuf = (char *)pRegisters[0];
		size_t size = (size_t)pRegisters[1];

		if (pParent->m_pfs.Getcwd(pBuf, size))
			return (unsigned int)size;			//I disagree...should be ptr
		else
			return -ENAMETOOLONG;
	}
	case 33:		//access
	{
		ASSERT(pParent);

		char *pFilename = (char*)pRegisters[0];
		int mode = (int)pRegisters[1];

		static const int s_nameLength = 256;
		char filename[s_nameLength];
		BaseDirent *f = pParent->m_rVfs.OpenByName(pParent->m_pfs.BuildFullPath(pFilename, filename, s_nameLength), O_RDONLY);

		if (f)
		{
			pParent->m_rVfs.Close(*f);
			return 0;
			//todo check mode
		}

		return -ENOENT;
	}
	case 64:		//getppid
	{
		return 0;
	}
	case 174:		//compat_sys_rt_sigaction
	{
		int signum = (int)pRegisters[0];
		const struct sigaction *act = (const struct sigaction *)pRegisters[1];
		const struct sigaction *oldact = (const struct sigaction *)pRegisters[2];

		p << "sigaction for signal " << signum << " ";
		if (act->sa_handler == SIG_ERR)
			p << "ERROR";
		else if (act->sa_handler == SIG_DFL)
			p << "DEFAULT";
		else if (act->sa_handler == SIG_IGN)
			p << "IGNORE";
		else
			p << (unsigned int)act->sa_handler;

		p << "\n";

		return 0;
	}
	case 322:		//openat
	{
		ASSERT(pParent);

		int dirfd = pRegisters[0];
		const char *pFilename = (const char *)pRegisters[1];
		int flags = pRegisters[2];
		mode_t mode = (mode_t)pRegisters[3];

		if (dirfd != AT_FDCWD)
		{
			p << "openat used with non-special dirfd, " << dirfd << "\n";
			return -1;
		}

		static const int s_nameLength = 256;
		char filename[s_nameLength];
		BaseDirent *f;

		f = pParent->m_rVfs.OpenByName(pParent->m_pfs.BuildFullPath(pFilename, filename, s_nameLength), O_RDONLY);
		if (f)
		{
			int fd = pParent->m_pfs.Open(*f);
			return fd;
		}

		return -1;
	}
	case 248:		//sys_exit_group
	{
		if (pParent)
		{
			//kill all threads
			pParent->Deschedule(Scheduler::GetMasterScheduler());

			delete pParent;
			pParent = 0;

			//do not use
			rBlocked.m_pParent = 0;
		}
		else
		{
			if (!rBlocked.SetState(Thread::kDead))
				ASSERT(0);
			Scheduler::GetMasterScheduler().RemoveThread(rBlocked);
		}

		return 0;
	}
	case 120:		//clone
		if (!pName)
			pName = "clone";
	case 114:		//wait4
		if (!pName)
			pName = "wait4";
	case 40:		//rmdir
		if (!pName)
			pName = "rmdir";
	case 36:	//sys_symlinkat
		if (!pName)
			pName = "symlinkat";
	case 221:		//fcntl64
		if (!pName)
			pName = "fcntl64";
	case 54:		//ioctl
		if (!pName)
			pName = "ioctl";
	case 19:		//lseek
		if (!pName)
			pName = "lseek";
	case 78:		//compat_sys_gettimeofday
		if (!pName)
			pName = "compat_sys_gettimeofday";
	case 175:		//compat_sys_rt_sigprocmask
		if (!pName)
			pName = "compat_sys_rt_sigprocmask";
	case 201:		//sys_geteuid
		if (!pName)
			pName = "sys_geteuid";
	case 202:		//getegid32
		if (!pName)
			pName = "getegid32";
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
		p.PrintString("\n");
		p.PrintString("supervisor call ");
		p.PrintHex(pRegisters[7]);
		p.PrintString("\n");

		for (int count = 0; count < 7; count++)
		{
			p.PrintString("\t");
			p.PrintHex(pRegisters[count]);
			p.PrintString("\n");
		}
		return (unsigned int)-4095;
	}
}


