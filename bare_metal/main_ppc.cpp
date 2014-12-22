/*
 * main_ppc.cpp
 *
 *  Created on: 29 Nov 2014
 *      Author: simon
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>

#include "print.h"
#include "PrintfPrinter.h"
#include "common.h"



Printer *Printer::sm_defaultPrinter = 0;




extern "C" unsigned long long gettime();
extern "C" unsigned int diff_usec(unsigned long long start, unsigned long long end);

extern "C" unsigned int TimingTest(unsigned int *);



static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void *Initialise();

int main(int argc, char **argv) {

        xfb = Initialise();

        PrintfPrinter printer;
        Printer::sm_defaultPrinter = &printer;

        Printer *p = &Printer::Get();

        //for some reason we need a newline at the top
        *p << "\n";

        *p << "we\'re in!\n";

        if (1)
		{
			unsigned int *pBuf = (unsigned int *)(malloc(4096));
//        	unsigned int *pBuf = (unsigned int *)(0xc8000000 + 0);
			*p << "buffer is at " << pBuf << "\n";
			for (int count = 0; count < 1024; count++)
				pBuf[count] = (unsigned int)&pBuf[count + 1];

			pBuf[1023] = (unsigned int)pBuf;

			for (int count = 0; count < 10; count++)
				*p << pBuf[count] << "\n";

			unsigned long long start_time = gettime();
//			for (int count = 0; count < 10; count++)
			{
				unsigned int a = TimingTest(pBuf);
//				*p << "a is " << a << "\n";
//				ASSERT(a == 10 * (1024 << 16));
			}
			unsigned long long end_time = gettime();
			unsigned int took = diff_usec(start_time, end_time);

			*p << "took " << took << " us\n";
		}

        unsigned long long prev;
        prev = gettime();

        while(1) {

                VIDEO_WaitVSync();
                PAD_ScanPads();

                int buttonsDown = PAD_ButtonsDown(0);

                if( buttonsDown & PAD_BUTTON_A ) {
                		unsigned long long curr = gettime();
                		unsigned int diff = diff_usec(prev, curr);
                		prev = curr;

                        *p << "difference " << Formatter<float>((float)diff / 1000000, 2) << "\n";
                        *(volatile unsigned long*)0xCC006400 = 0x00400001; *(volatile unsigned long*)0xCC006438 = 0x80000000;

                }

                if( buttonsDown & PAD_BUTTON_B ) {
                        *p << "Button B pressed.\n";
                        *(volatile unsigned long*)0xCC006400 = 0x00400000; *(volatile unsigned long*)0xCC006438 = 0x80000000;
                }

                if (buttonsDown & PAD_BUTTON_START) {
//                	ASSERT(0);
                        exit(0);
                }
        }

        return 0;
}



void * Initialise() {

        void *framebuffer;

        VIDEO_Init();
        PAD_Init();

        rmode = VIDEO_GetPreferredMode(NULL);

        framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
        console_init(framebuffer,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

        VIDEO_Configure(rmode);
        VIDEO_SetNextFramebuffer(framebuffer);
        VIDEO_SetBlack(FALSE);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

        return framebuffer;

}
