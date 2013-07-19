/*
 * DSS.h
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#ifndef DSS_H_
#define DSS_H_

#include "Modeline.h"
#include <vector>

namespace OMAP4460
{

class Pipeline
{
protected:
	Pipeline(void)
	{
	}

	virtual ~Pipeline(void)
	{
	}

	virtual void Attach(void) = 0;
	virtual void Detach(void) = 0;

	virtual void SetNewBaseAddrOnVsync(void *) = 0;
	virtual void SetNewPositionOnVsync(unsigned int posX, unsigned int posY) = 0;
};

class Gfx : public Pipeline
{
public:
	enum PixelFormat
	{
		kRGB16_565 = 0x6,
		kBITMAP2 = 0x1,
		kRGBx12_1444 = 0xa,
		kARGB16_1555 = 0x7,
		kRGBA32_8888 = 0xd,
		kBITMAP1 = 0x0,
		kBITMAP4 = 0x2,
		kxRGB24_8888 = 0x8,
		kRGB24_888 = 0x9,
		kRGBA12_4444 = 0xb,
		kxRGB12_4444 = 0x4,
		kARGB16_4444 = 0x5,
		kxRGB15_1555 = 0xf,
		kARGB32_8888 = 0xc,
		kBITMAP8 = 0x3,
		kRGBx24_8888 = 0xe
	};

	Gfx(void *pPhysBuffer, PixelFormat,
			unsigned int width, unsigned int height,
			int pixSkip, int rowSkip,
			unsigned int posX, unsigned int posY);

	virtual ~Gfx(void)
	{
		//not sure if attached - must detach manually
	}

	virtual void Attach(void);
	virtual void Detach(void);

	virtual void SetNewBaseAddrOnVsync(void *);
	virtual void SetNewPositionOnVsync(unsigned int posX, unsigned int posY);

private:
	void *m_pPhysBuffer;
	PixelFormat m_pf;

	unsigned int m_width, m_height;

	int m_pixSkip, m_rowSkip;

	unsigned int m_posX, m_posY;
};

class DSS
{
public:
	DSS();
	virtual ~DSS();

	bool EnableDisplay(Modeline mode);

	void SpinWaitForVsyncAndClear(void);

private:
	void FillTimingsList(void);
	unsigned int PolarityToBit(Modeline::Polarity p)
	{
		if (p == Modeline::kPositive)
			return 0;
		else
			return 1;
	}

	struct ClockCombo
	{
		ClockCombo(unsigned int pclk, unsigned int f, unsigned int l, unsigned int p)
		: m_pclk(pclk),
		  m_f(f),
		  m_l(l),
		  m_p(p)
		{
		}

		unsigned int m_pclk;
		unsigned int m_f;
		unsigned int m_l;
		unsigned int m_p;

		static bool comp_func(const ClockCombo &i, const ClockCombo &j)
		{
			return i.m_pclk < j.m_pclk;
		}
	};

	std::vector<ClockCombo> m_pclks;

	void *pPhysGammaTable;
};

} /* namespace OMAP4460 */
#endif /* DSS_H_ */
