/*
 * DSS.cpp
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#include "DSS.h"
#include "phys_memory.h"
#include "print_uart.h"

#include <algorithm>

namespace OMAP4460
{

namespace Dispcc
{
	volatile unsigned int *DISPC_REVISION = (volatile unsigned int *)0xFC001000;
	volatile unsigned int *DISPC_SYSCONFIG = (volatile unsigned int *)0xFC001010;
	volatile unsigned int *DISPC_SYSSTATUS = (volatile unsigned int *)0xFC001014;
	volatile unsigned int *DISPC_IRQSTATUS = (volatile unsigned int *)0xFC001018;
	volatile unsigned int *DISPC_IRQENABLE = (volatile unsigned int *)0xFC00101C;
	volatile unsigned int *DISPC_CONTROL1  = (volatile unsigned int *)0xFC001040;
	volatile unsigned int *DISPC_CONFIG1 = (volatile unsigned int *)0xFC001044;
	volatile unsigned int *DISPC_DEFAULT_COLOR0 = (volatile unsigned int *)0xFC00104C;
	volatile unsigned int *DISPC_DEFAULT_COLOR1 = (volatile unsigned int *)0xFC001050;
	volatile unsigned int *DISPC_TRANS_COLOR0 = (volatile unsigned int *)0xFC001054;
	volatile unsigned int *DISPC_TRANS_COLOR1 = (volatile unsigned int *)0xFC001058;
	volatile unsigned int *DISPC_LINE_STATUS = (volatile unsigned int *)0xFC00105C;
	volatile unsigned int *DISPC_LINE_NUMBER = (volatile unsigned int *)0xFC001060;
	volatile unsigned int *DISPC_TIMING_H1 = (volatile unsigned int *)0xFC001064;
	volatile unsigned int *DISPC_TIMING_V1 = (volatile unsigned int *)0xFC001068;
	volatile unsigned int *DISPC_POL_FREQ1 = (volatile unsigned int *)0xFC00106C;
	volatile unsigned int *DISPC_DIVISOR1 = (volatile unsigned int *)0xFC001070;
	volatile unsigned int *DISPC_GLOBAL_ALPHA = (volatile unsigned int *)0xFC001074;
	volatile unsigned int *DISPC_SIZE_TV = (volatile unsigned int *)0xFC001078;
	volatile unsigned int *DISPC_SIZE_LCD1 = (volatile unsigned int *)0xFC00107C;

	volatile unsigned int *DISPC_GFX_BA_0 = (volatile unsigned int *)(0xFC001080 + 4 * 0);
	volatile unsigned int *DISPC_GFX_BA_1 = (volatile unsigned int *)(0xFC001080 + 4 * 1);
	volatile unsigned int *DISPC_GFX_POSITION = (volatile unsigned int *)0xFC001088;
	volatile unsigned int *DISPC_GFX_SIZE = (volatile unsigned int *)0xFC00108C;
	volatile unsigned int *DISPC_GFX_ATTRIBUTES = (volatile unsigned int *)0xFC0010A0;
	volatile unsigned int *DISPC_GFX_BUF_THRESHOLD = (volatile unsigned int *)0xFC0010A4;
	volatile unsigned int *DISPC_GFX_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC0010A8;
	volatile unsigned int *DISPC_GFX_ROW_INC = (volatile unsigned int *)0xFC0010AC;
	volatile unsigned int *DISPC_GFX_PIXEL_INC = (volatile unsigned int *)0xFC0010B0;
	volatile unsigned int *DISPC_GFX_TABLE_BA = (volatile unsigned int *)0xFC0010B8;

	volatile unsigned int *DISPC_VID1_BA_0 = (volatile unsigned int *)(0xFC0010BC + 4 * 0);
	volatile unsigned int *DISPC_VID1_BA_1 = (volatile unsigned int *)(0xFC0010BC + 4 * 1);
	volatile unsigned int *DISPC_VID1_POSITION = (volatile unsigned int *)0xFC0010C4;
	volatile unsigned int *DISPC_VID1_SIZE = (volatile unsigned int *)0xFC0010C8;
	volatile unsigned int *DISPC_VID1_ATTRIBUTES = (volatile unsigned int *)0xFC0010CC;
	volatile unsigned int *DISPC_VID1_BUF_THRESHOLD = (volatile unsigned int *)0xFC0010D0;
	volatile unsigned int *DISPC_VID1_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC0010D4;
	volatile unsigned int *DISPC_VID1_ROW_INC = (volatile unsigned int *)0xFC0010D8;
	volatile unsigned int *DISPC_VID1_PIXEL_INC = (volatile unsigned int *)0xFC0010DC;
	volatile unsigned int *DISPC_VID1_FIR = (volatile unsigned int *)0xFC0010E0;
	volatile unsigned int *DISPC_VID1_PICTURE_SIZE = (volatile unsigned int *)0xFC0010E4;
	volatile unsigned int *DISPC_VID1_ACCU_0 = (volatile unsigned int *)(0xFC0010E8 + 4 * 0);
	volatile unsigned int *DISPC_VID1_ACCU_1 = (volatile unsigned int *)(0xFC0010E8 + 4 * 1);
	volatile unsigned int *DISPC_VID1_FIR_COEF_H_0 = (volatile unsigned int *)(0xFC0010F0 + 8 * 0);
	volatile unsigned int *DISPC_VID1_FIR_COEF_H_1 = (volatile unsigned int *)(0xFC0010F0 + 8 * 1);
	volatile unsigned int *DISPC_VID1_FIR_COEF_HV_0 = (volatile unsigned int *)(0xFC0010F4 + 8 * 0);
	volatile unsigned int *DISPC_VID1_FIR_COEF_HV_1 = (volatile unsigned int *)(0xFC0010F4 + 8 * 1);
//	volatile unsigned int *DISPC_VID1_CONV_COEF0 = (volatile unsigned int *)0xFC001130;
//	volatile unsigned int *DISPC_VID1_CONV_COEF1 = (volatile unsigned int *)0xFC001134;
//	volatile unsigned int *DISPC_VID1_CONV_COEF2 = (volatile unsigned int *)0xFC001138;
//	volatile unsigned int *DISPC_VID1_CONV_COEF3 = (volatile unsigned int *)0xFC00113C;
//	volatile unsigned int *DISPC_VID1_CONV_COEF4 = (volatile unsigned int *)0xFC001140;

	volatile unsigned int *DISPC_VID2_BA_0 = (volatile unsigned int *)(0xFC00114C + 4 * 0);
	volatile unsigned int *DISPC_VID2_BA_1 = (volatile unsigned int *)(0xFC00114C + 4 * 1);
	volatile unsigned int *DISPC_VID2_POSITION = (volatile unsigned int *)0xFC001154;
	volatile unsigned int *DISPC_VID2_SIZE = (volatile unsigned int *)0xFC001158;
	volatile unsigned int *DISPC_VID2_ATTRIBUTES = (volatile unsigned int *)0xFC00115C;
	volatile unsigned int *DISPC_VID2_BUF_THRESHOLD = (volatile unsigned int *)0xFC001160;
	volatile unsigned int *DISPC_VID2_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001164;
	volatile unsigned int *DISPC_VID2_ROW_INC = (volatile unsigned int *)0xFC001168;
	volatile unsigned int *DISPC_VID2_PIXEL_INC = (volatile unsigned int *)0xFC00116C;
	volatile unsigned int *DISPC_VID2_FIR = (volatile unsigned int *)0xFC001170;
	volatile unsigned int *DISPC_VID2_PICTURE_SIZE = (volatile unsigned int *)0xFC001174;
	volatile unsigned int *DISPC_VID2_ACCU_0 = (volatile unsigned int *)(0xFC001178 + 4 * 0);
	volatile unsigned int *DISPC_VID2_ACCU_1 = (volatile unsigned int *)(0xFC001178 + 4 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_H_0 = (volatile unsigned int *)(0xFC001180 + 8 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_H_1 = (volatile unsigned int *)(0xFC001180 + 8 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_HV_0 = (volatile unsigned int *)(0xFC001184 + 8 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_HV_1 = (volatile unsigned int *)(0xFC001184 + 8 * 1);

	volatile unsigned int *DISPC_VID2_CONV_COEF0 = (volatile unsigned int *)0xFC0011C0;
	volatile unsigned int *DISPC_VID2_CONV_COEF1 = (volatile unsigned int *)0xFC0011C4;
	volatile unsigned int *DISPC_VID2_CONV_COEF2 = (volatile unsigned int *)0xFC0011C8;
	volatile unsigned int *DISPC_VID2_CONV_COEF3 = (volatile unsigned int *)0xFC0011CC;
	volatile unsigned int *DISPC_VID2_CONV_COEF4 = (volatile unsigned int *)0xFC0011D0;

	volatile unsigned int *DISPC_DATA1_CYCLE1 = (volatile unsigned int *)0xFC0011D4;
	volatile unsigned int *DISPC_DATA1_CYCLE2 = (volatile unsigned int *)0xFC0011D8;
	volatile unsigned int *DISPC_DATA1_CYCLE3 = (volatile unsigned int *)0xFC0011DC;

//	volatile unsigned int *DISPC_VID1_FIR_COEF_V_0 = (volatile unsigned int *)(0xFC0011E0 + 4 * 0);
//	volatile unsigned int *DISPC_VID1_FIR_COEF_V_1 = (volatile unsigned int *)(0xFC0011E0 + 4 * 1);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_V_0 = (volatile unsigned int *)(0xFC001200 + 4 * 0);
//	volatile unsigned int *DISPC_VID2_FIR_COEF_V_1 = (volatile unsigned int *)(0xFC001200 + 4 * 1);

	volatile unsigned int *DISPC_CPR1_COEF_R = (volatile unsigned int *)0xFC001220;
	volatile unsigned int *DISPC_CPR1_COEF_G = (volatile unsigned int *)0xFC001224;
	volatile unsigned int *DISPC_CPR1_COEF_B = (volatile unsigned int *)0xFC001228;

	volatile unsigned int *DISPC_GFX_PRELOAD = (volatile unsigned int *)0xFC00122C;
	volatile unsigned int *DISPC_VID1_PRELOAD = (volatile unsigned int *)0xFC001230;
	volatile unsigned int *DISPC_VID2_PRELOAD = (volatile unsigned int *)0xFC001234;

	volatile unsigned int *DISPC_CONTROL2 = (volatile unsigned int *)0xFC001238;

	volatile unsigned int *DISPC_VID3_ACCU_0 = (volatile unsigned int *)(0xFC001300 + 4 * 0);
	volatile unsigned int *DISPC_VID3_ACCU_1 = (volatile unsigned int *)(0xFC001300 + 4 * 1);
	volatile unsigned int *DISPC_VID3_BA_0 = (volatile unsigned int *)(0xFC001308 + 4 * 0);
	volatile unsigned int *DISPC_VID3_BA_1 = (volatile unsigned int *)(0xFC001308 + 4 * 1);
	volatile unsigned int *DISPC_VID3_ATTRIBUTES = (volatile unsigned int *)0xFC001370;
	volatile unsigned int *DISPC_VID3_CONV_COEF0 = (volatile unsigned int *)0xFC001374;
	volatile unsigned int *DISPC_VID3_CONV_COEF1 = (volatile unsigned int *)0xFC001378;
	volatile unsigned int *DISPC_VID3_CONV_COEF2 = (volatile unsigned int *)0xFC00137C;
	volatile unsigned int *DISPC_VID3_CONV_COEF3 = (volatile unsigned int *)0xFC001380;
	volatile unsigned int *DISPC_VID3_CONV_COEF4 = (volatile unsigned int *)0xFC001384;
	volatile unsigned int *DISPC_VID3_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001388;
	volatile unsigned int *DISPC_VID3_BUF_THRESHOLD = (volatile unsigned int *)0xFC00138C;
	volatile unsigned int *DISPC_VID3_FIR = (volatile unsigned int *)0xFC001390;
	volatile unsigned int *DISPC_VID3_PICTURE_SIZE = (volatile unsigned int *)0xFC001394;
	volatile unsigned int *DISPC_VID3_PIXEL_INC = (volatile unsigned int *)0xFC001398;
	volatile unsigned int *DISPC_VID3_POSITION = (volatile unsigned int *)0xFC00139C;
	volatile unsigned int *DISPC_VID3_PRELOAD = (volatile unsigned int *)0xFC0013A0;
	volatile unsigned int *DISPC_VID3_ROW_INC = (volatile unsigned int *)0xFC0013A4;
	volatile unsigned int *DISPC_VID3_SIZE = (volatile unsigned int *)0xFC0013A8;
	volatile unsigned int *DISPC_DEFAULT_COLOR2 = (volatile unsigned int *)0xFC0013AC;
	volatile unsigned int *DISPC_TRANS_COLOR2 = (volatile unsigned int *)0xFC0013B0;
	volatile unsigned int *DISPC_CPR2_COEF_B = (volatile unsigned int *)0xFC0013B4;
	volatile unsigned int *DISPC_CPR2_COEF_G = (volatile unsigned int *)0xFC0013B8;
	volatile unsigned int *DISPC_CPR2_COEF_R = (volatile unsigned int *)0xFC0013BC;
	volatile unsigned int *DISPC_DATA2_CYCLE1 = (volatile unsigned int *)0xFC0013C0;
	volatile unsigned int *DISPC_DATA2_CYCLE2 = (volatile unsigned int *)0xFC0013C4;
	volatile unsigned int *DISPC_DATA2_CYCLE3 = (volatile unsigned int *)0xFC0013C8;
	volatile unsigned int *DISPC_SIZE_LCD2 = (volatile unsigned int *)0xFC0013CC;
	volatile unsigned int *DISPC_TIMING_H2 = (volatile unsigned int *)0xFC001400;
	volatile unsigned int *DISPC_TIMING_V2 = (volatile unsigned int *)0xFC001404;
	volatile unsigned int *DISPC_POL_FREQ2 = (volatile unsigned int *)0xFC001408;
	volatile unsigned int *DISPC_DIVISOR2 = (volatile unsigned int *)0xFC00140C;

	volatile unsigned int *DISPC_WB_ACCU_0 = (volatile unsigned int *)(0xFC001500 + 4 * 0);
	volatile unsigned int *DISPC_WB_ACCU_1 = (volatile unsigned int *)(0xFC001500 + 4 * 1);
	volatile unsigned int *DISPC_WB_BA_0 = (volatile unsigned int *)(0xFC001508 + 4 * 0);
	volatile unsigned int *DISPC_WB_BA_1 = (volatile unsigned int *)(0xFC001508 + 4 * 1);
	volatile unsigned int *DISPC_WB_ATTRIBUTES = (volatile unsigned int *)0xFC001570;
	volatile unsigned int *DISPC_WB_CONV_COEF0 = (volatile unsigned int *)0xFC001574;
	volatile unsigned int *DISPC_WB_CONV_COEF1 = (volatile unsigned int *)0xFC001578;
	volatile unsigned int *DISPC_WB_CONV_COEF2 = (volatile unsigned int *)0xFC00157C;
	volatile unsigned int *DISPC_WB_CONV_COEF3 = (volatile unsigned int *)0xFC001580;
	volatile unsigned int *DISPC_WB_CONV_COEF4 = (volatile unsigned int *)0xFC001584;
	volatile unsigned int *DISPC_WB_BUF_SIZE_STATUS = (volatile unsigned int *)0xFC001588;
	volatile unsigned int *DISPC_WB_BUF_THRESHOLD = (volatile unsigned int *)0xFC00158C;
	volatile unsigned int *DISPC_WB_FIR = (volatile unsigned int *)0xFC001590;
	volatile unsigned int *DISPC_WB_PICTURE_SIZE = (volatile unsigned int *)0xFC001594;
	volatile unsigned int *DISPC_WB_PIXEL_INC = (volatile unsigned int *)0xFC001598;
	volatile unsigned int *DISPC_WB_ROW_INC = (volatile unsigned int *)0xFC0015A4;
	volatile unsigned int *DISPC_WB_SIZE = (volatile unsigned int *)0xFC0015A8;

	volatile unsigned int *DISPC_VID1_BA_UV_0 = (volatile unsigned int *)(0xFC001600 + 4 * 0);
	volatile unsigned int *DISPC_VID1_BA_UV_1 = (volatile unsigned int *)(0xFC001600 + 4 * 1);
	volatile unsigned int *DISPC_VID2_BA_UV_0 = (volatile unsigned int *)(0xFC001608 + 4 * 0);
	volatile unsigned int *DISPC_VID2_BA_UV_1 = (volatile unsigned int *)(0xFC001608 + 4 * 1);
	volatile unsigned int *DISPC_VID3_BA_UV_0 = (volatile unsigned int *)(0xFC001610 + 4 * 0);
	volatile unsigned int *DISPC_VID3_BA_UV_1 = (volatile unsigned int *)(0xFC001610 + 4 * 1);
	volatile unsigned int *DISPC_WB_BA_UV_0 = (volatile unsigned int *)(0xFC001618 + 4 * 0);
	volatile unsigned int *DISPC_WB_BA_UV_1 = (volatile unsigned int *)(0xFC001618 + 4 * 1);

	volatile unsigned int *DISPC_CONFIG2 = (volatile unsigned int *)0xFC001620;
	volatile unsigned int *DISPC_VID1_ATTRIBUTES2 = (volatile unsigned int *)0xFC001624;
	volatile unsigned int *DISPC_VID2_ATTRIBUTES2 = (volatile unsigned int *)0xFC001628;
	volatile unsigned int *DISPC_VID3_ATTRIBUTES2 = (volatile unsigned int *)0xFC00162C;
	volatile unsigned int *DISPC_GAMMA_TABLE0 = (volatile unsigned int *)0xFC001630;
	volatile unsigned int *DISPC_GAMMA_TABLE1 = (volatile unsigned int *)0xFC001634;
	volatile unsigned int *DISPC_GAMMA_TABLE2 = (volatile unsigned int *)0xFC001638;
	volatile unsigned int *DISPC_VID1_FIR2 = (volatile unsigned int *)0xFC00163C;

	volatile unsigned int *DISPC_GLOBAL_BUFFER = (volatile unsigned int *)0xFC001800;
	volatile unsigned int *DISPC_DIVISOR = (volatile unsigned int *)0xFC001804;
	volatile unsigned int *DISPC_WB_ATTRIBUTES2 = (volatile unsigned int *)0xFC001810;
}

DSS::DSS()
{
	FillTimingsList();
	pPhysGammaTable = PhysPages::FindPage();
}

DSS::~DSS()
{
	PhysPages::ReleasePage((unsigned int)pPhysGammaTable >> 12);
}

bool DSS::EnableDisplay(Modeline mode)
{
	PrinterUart<PL011> p;

	volatile unsigned int * const pCM_DIV_M5_DPLL_PER = (volatile unsigned int *)0xe020815c;

	//get a pixel clock just above what we want
	unsigned int wantkHz = mode.m_pixClkkHz;
	unsigned int havekHz;
	bool changed_clocks = false;

	for (auto it = m_pclks.begin(); it != m_pclks.end(); it++)
	{
		havekHz = it->m_pclk / 1000;

		if (havekHz > wantkHz)
		{
			changed_clocks = true;

			//change the fclk
			*pCM_DIV_M5_DPLL_PER = (*pCM_DIV_M5_DPLL_PER & ~31) | it->m_f;

			//change the lcd and pcd divisors
			*Dispcc::DISPC_DIVISOR2 = (it->m_l << 16) | it->m_p;

			//change dispc_core_clk to dispc_fclk/1
			*Dispcc::DISPC_DIVISOR = (1 << 16) | 1;

			p << "changed clocks to " << it->m_f << " " << it->m_l << " " << it->m_p << "\n";

			break;
		}
	}

	p << "here\n";

	if (!changed_clocks)
	{
		p << "failed to change clock\n";
		return false;
	}

	p << "trying mode fit\n";
	//now up the mode until it fits the pixel clock
	Modeline new_mode = mode.AdjustToFitPixClk(havekHz);

	*Dispcc::DISPC_CONFIG1 |= (1 << 3);					//lut as gamma table
	*Dispcc::DISPC_GFX_TABLE_BA = (unsigned int)pPhysGammaTable;

	*Dispcc::DISPC_CONTROL2 |= (3 << 8);				//24-bit display
	*Dispcc::DISPC_CONTROL2 |= (1 << 5);				//golcd 2

	//something to do with gpio - not sure why needed
	*Dispcc::DISPC_CONTROL1 |= (1 << 14) | (1 << 15);

	//lcd2 panel background colour,
	*Dispcc::DISPC_DEFAULT_COLOR2 = 0x7f7f7f7f;

	*Dispcc::DISPC_POL_FREQ2 = (PolarityToBit(new_mode.m_hsyncPol) << 13) | (PolarityToBit(new_mode.m_vsyncPol) << 12);
	*Dispcc::DISPC_SIZE_LCD2 = ((new_mode.m_height - 1) << 16) | (new_mode.m_width - 1);
	*Dispcc::DISPC_CONTROL2 |= (1 << 3);						//active tft
	*Dispcc::DISPC_TIMING_V2 = (new_mode.m_vBp << 20) | (new_mode.m_vFp << 8) | (new_mode.m_vSync - 1);
	*Dispcc::DISPC_TIMING_H2 = ((new_mode.m_hBp - 1) << 20) | ((new_mode.m_hFp - 1) << 8) | (new_mode.m_hSync - 1);

	*Dispcc::DISPC_CONTROL2 |= (1 << 5);						//golcd2
	*Dispcc::DISPC_CONTROL2 |= (1 << 0);						//lcdenable

	p << "display enabled\n";

	return true;
}

void DSS::SpinWaitForVsyncAndClear(void)
{
	*Dispcc::DISPC_IRQSTATUS &= ~0x40000;
	while ((*Dispcc::DISPC_IRQSTATUS & 0x40000) == 0);
}

void DSS::FillTimingsList(void)
{
	for (int f = 1; f < 32; f++)				//functional clock divider
	{
		unsigned int fclk = 1536000000 / f;
		if (fclk > 186000000)
			continue;

		for (int l = 1; l <= 255; l++)			//logical clock divider
		{
			unsigned int lclk = fclk / l;

			for (int p = 1; p <= 255; p++)		//pixel clock divider
			{
				unsigned int pclk = lclk / p;
				if (pclk < 25000000)
					continue;
				else
					m_pclks.push_back(ClockCombo(pclk, f, l, p));
			}
		}
	}

	std::sort(m_pclks.begin(), m_pclks.end(), ClockCombo::comp_func);
}

} /* namespace OMAP4460 */

OMAP4460::Gfx::Gfx(void* pPhysBuffer, PixelFormat pixelFormat,
		unsigned int width, unsigned int height,
		int pixSkip, int rowSkip,
		unsigned int posX, unsigned int posY)
: Pipeline(),
  m_pPhysBuffer(pPhysBuffer),
  m_pf(pixelFormat),
  m_width(width & 2047),
  m_height(height & 2047),
  m_pixSkip(pixSkip),
  m_rowSkip(rowSkip),
  m_posX(posX & 2047),
  m_posY(posY & 2047)
{
}

void OMAP4460::Gfx::Attach(void)
{
	//disable the graphics pipeline first
	*Dispcc::DISPC_GFX_ATTRIBUTES &= ~1;

	//set data base address =
	*Dispcc::DISPC_GFX_BA_0 = (unsigned int)m_pPhysBuffer;

	//select the format of image
	*Dispcc::DISPC_GFX_ATTRIBUTES |= ((unsigned int)m_pf << 1);

	//set window size
	*Dispcc::DISPC_GFX_SIZE = (m_width - 1) | ((m_height - 1) << 16);

	//set position
	*Dispcc::DISPC_GFX_POSITION = m_posX | (m_posY << 16);

	//dma increment
	ASSERT(m_rowSkip == 1);
	ASSERT(m_pixSkip == 1);

	//channelout2, for lcd2
	*Dispcc::DISPC_GFX_ATTRIBUTES |= (1 << 30);

	*Dispcc::DISPC_CONTROL2 |= (1 << 5);				//golcd 2

	//enable graphics pipeline
	*Dispcc::DISPC_GFX_ATTRIBUTES |= 1;
}

void OMAP4460::Gfx::Detach(void)
{
	*Dispcc::DISPC_GFX_ATTRIBUTES &= ~1;
	*Dispcc::DISPC_CONTROL2 |= (1 << 5);						//golcd2
}

void OMAP4460::Gfx::SetNewBaseAddrOnVsync(void *p)
{
	m_pPhysBuffer = p;
	*Dispcc::DISPC_GFX_BA_0 = (unsigned int)m_pPhysBuffer;
	*Dispcc::DISPC_CONTROL2 |= (1 << 5);						//golcd2
}

void OMAP4460::Gfx::SetNewPositionOnVsync(unsigned int posX, unsigned int posY)
{
	m_posX = posX & 2047;
	m_posY = posY & 2047;
	*Dispcc::DISPC_GFX_POSITION = m_posX | (m_posY << 16);
	*Dispcc::DISPC_CONTROL2 |= (1 << 5);						//golcd2
}
