/*
 * PrintFb.h
 *
 *  Created on: 7 Sep 2014
 *      Author: simon
 */

#ifndef PRINTFB_H_
#define PRINTFB_H_

#include "print.h"

class PrinterFb : public Printer
{
public:
	enum PixelFormat
	{
		kBinary1,
		kxRGB8888
	};
	PrinterFb(unsigned int *pFramebuffer, int width, int height, PixelFormat pf, Printer *p = 0);
	virtual ~PrinterFb();

	virtual void PrintChar(char c);
	void SetChainedPrinter(Printer *p);

	inline unsigned int GetWidth(void) { return m_width; }
	inline unsigned int GetHeight(void) { return m_height; }
	inline unsigned int *GetVirtBase(void) { return m_pFramebuffer; }

protected:
	unsigned int *m_pFramebuffer;
	unsigned char *m_pFramebufferBin;
	unsigned int m_width;
	unsigned int m_height;
	PixelFormat m_pixelFormat;

	unsigned int m_x;
	unsigned int m_y;

	Printer *m_pChainedPrinter;
};

#endif /* PRINTFB_H_ */
