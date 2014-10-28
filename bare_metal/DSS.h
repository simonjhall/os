/*
 * DSS.h
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#ifndef DSS_H_
#define DSS_H_

#include "InterruptSource.h"
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

class DSS : public InterruptSource
{
public:
	DSS();
	virtual ~DSS();

	bool EnableDisplay(Modeline mode);
	bool EnableTv(Modeline mode);
	void SpinWaitForVsyncAndClear(void);

	virtual void EnableInterrupt(bool e);
	virtual unsigned int GetInterruptNumber(void);
	virtual bool HasInterruptFired(void);
	virtual void ClearInterrupt(void);

	void SetInterruptMask(unsigned int);
	unsigned int GetInterruptMask(void);

	unsigned int RawIrqStatus(void);

	inline void *GetPhysTable(void) { return m_pPhysGammaTable; };

	static const unsigned int sm_frameDone1			= 1 << 0;
	static const unsigned int sm_vSync1				= 1 << 1;
	static const unsigned int sm_eVsyncEven			= 1 << 2;
	static const unsigned int sm_eVsyncOdd			= 1 << 3;
	static const unsigned int sm_acBiasCount1		= 1 << 4;
	static const unsigned int sm_programmedLine		= 1 << 5;
	static const unsigned int sm_gfxBufferUnderflow	= 1 << 6;
	static const unsigned int sm_gfxEndWindow		= 1 << 7;
	static const unsigned int sm_paletteGammaLoad	= 1 << 8;
	static const unsigned int sm_ocpError			= 1 << 9;
	static const unsigned int sm_vid1BufferUnderflow= 1 << 10;
	static const unsigned int sm_vid1EndWindow		= 1 << 11;
	static const unsigned int sm_vid2BufferUnderflow= 1 << 12;
	static const unsigned int sm_vid2EndWindow		= 1 << 13;
	static const unsigned int sm_syncLost1			= 1 << 14;
	static const unsigned int sm_syncLostTv			= 1 << 15;
	static const unsigned int sm_wakeUp				= 1 << 16;
	static const unsigned int sm_syncLost2			= 1 << 17;
	static const unsigned int sm_vSync2				= 1 << 18;
	static const unsigned int sm_vid3EndWindow		= 1 << 19;
	static const unsigned int sm_vid3BufferUnderflow= 1 << 20;
	static const unsigned int sm_acBiasCount2		= 1 << 21;
	static const unsigned int sm_frameDone2			= 1 << 22;
	static const unsigned int sm_frameDoneWb		= 1 << 23;
	static const unsigned int sm_frameDoneTv		= 1 << 24;
	static const unsigned int sm_wbBufferOverflow	= 1 << 25;
	static const unsigned int sm_wbErrorUncomplete	= 1 << 26;

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

	void *m_pPhysGammaTable;
	unsigned int m_interruptMask;
	bool m_interruptsEnabled;
};

} /* namespace OMAP4460 */
#endif /* DSS_H_ */
