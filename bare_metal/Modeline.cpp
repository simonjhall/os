/*
 * Modeline.cpp
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#include "Modeline.h"
#include "common.h"
#include <string.h>
#include "print_uart.h"

std::list<Modeline> *Modeline::m_pList = 0;

Modeline::~Modeline()
{
	// TODO Auto-generated destructor stub
}

Modeline Modeline::AdjustToFitPixClk(unsigned int targetPixkHz)
{
	Printer &p = Printer::Get();

	unsigned int startkHz = ComputePixRatekHz();
	ASSERT(startkHz <= targetPixkHz);

	Modeline best = *this;
	int difference = targetPixkHz - startkHz;

	p << "target "; p.PrintDec(targetPixkHz, false); p << "\n";
	p << "start "; p.PrintDec(startkHz, false); p << "\n";
	p << "initial difference\n";
	p.PrintDec(difference, false);
	p << "\n";

	Modeline working = *this;

	int added = 0;

	while (difference > 0)
	{
		working.m_vSync++;
		added++;

		int new_difference = targetPixkHz - working.ComputePixRatekHz();

		if (new_difference > 0)
		{
			if (new_difference < difference)
			{
				best = working;
				best.m_pixClkkHz = working.ComputePixRatekHz();
				difference = new_difference;
			}
		}
		else
			break;
	}

	p << "added "; p.PrintDec(added, false); p << " rows\n";

	return best;
}

unsigned int Modeline::ComputePixRatekHz(void)
{
	unsigned int totalPix = (m_width + m_hFp + m_hSync + m_hBp) * (m_height + m_vFp + m_vSync + m_vBp);
	unsigned int pixPerSec = totalPix * m_idealVHz;

	return pixPerSec / 1000;
}

void Modeline::Print(Printer& p)
{
	p << "mode " << m_pName << "\n";
	p << "width "; p.PrintDec(m_width, false); p << " front porch "; p.PrintDec(m_hFp, false); p << " sync "; ; p.PrintDec(m_hSync, false); p << " back porch "; ; p.PrintDec(m_hBp, false); p << "\n";
	p << "height "; p.PrintDec(m_height, false); p << " front porch "; p.PrintDec(m_vFp, false); p << " sync "; ; p.PrintDec(m_vSync, false); p << " back porch "; ; p.PrintDec(m_vBp, false); p << "\n";
	p << "horiz polarity" << (m_hsyncPol == kPositive ? " POSITIVE\n" : " NEGATIVE\n");
	p << "vert polarity" << (m_vsyncPol == kPositive ? " POSITIVE\n" : " NEGATIVE\n");
	p << "pixel clock "; p.PrintDec(m_pixClkkHz, false); p << " kHz\n";
	p << "ideal vertical refresh "; p.PrintDec(m_idealVHz, false); p << " Hz\n";
}

void Modeline::AddDefaultModes(void)
{
	auto pList = GetDefaultModesList();
	ASSERT(pList);

	pList->push_back(Modeline("640x480@60", 25170, 60,
			640, 656, 752, 800,
			480, 491, 493, 524,
			Modeline::kNegative, Modeline::kNegative));

	pList->push_back(Modeline("800x600@60", 40000, 60,
			800, 840, 968, 1056,
			600, 601, 605, 628,
			Modeline::kPositive, Modeline::kPositive));

	pList->push_back(Modeline("720x480@60",
			720, 480, 27027, 60, 62, 16, 60, 6, 9, 30,
			Modeline::kNegative, Modeline::kNegative, Modeline::kTiHdmi));

	pList->push_back(Modeline("1024x768@60", 65000, 60,
			1024, 1048, 1184, 1344,
			768, 771, 777, 806,
			Modeline::kNegative, Modeline::kNegative));

	pList->push_back(Modeline("1280x1024@60", 108000, 60,
			1280, 1328, 1440, 1688,
			1024, 1025, 1028, 1066,
			Modeline::kPositive, Modeline::kPositive));

	pList->push_back(Modeline("1280x720@60", 74180, 60,
			1280, 1390, 1430, 1650,
			720, 725, 730, 750,
			Modeline::kPositive, Modeline::kPositive));

	pList->push_back(Modeline("1920x1080@60", 148350, 60,
			1920, 2008, 2052, 2200,
			1080, 1084, 1089, 1125,
			Modeline::kPositive, Modeline::kPositive));

	pList->push_back(Modeline("TV1280x720@60", 74250, 60,
			1280, 1390, 1430, 1650,
			720, 725, 730, 750,
			Modeline::kPositive, Modeline::kPositive));

	pList->push_back(Modeline("TV1920x1080@60", 148500, 60,
			1920, 2008, 2052, 2200,
			1080, 1084, 1089, 1125,
			Modeline::kPositive, Modeline::kPositive));
}

bool Modeline::FindModeline(const char* pName, Modeline& out)
{
	ASSERT(GetDefaultModesList());

	for (auto it = GetDefaultModesList()->begin(); it != GetDefaultModesList()->end(); it++)
		if (strcmp(it->m_pName, pName) == 0)
		{
			out = *it;
			return true;
		}

	return false;
}
