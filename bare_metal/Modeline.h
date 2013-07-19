/*
 * Modeline.h
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#ifndef MODELINE_H_
#define MODELINE_H_

#include <list>
#include "print.h"

class Modeline
{
public:
	enum Polarity
	{
		kPositive,
		kNegative
	};

	Modeline(void)
	{
	}

	Modeline(const char *pName, unsigned int pixClkkHz, unsigned int idealVHz,
			unsigned int width, unsigned int widthPlusFp, unsigned int widthPlusFpPlusSync, unsigned int widthPlusFpPlusSyncPlusBp,
			unsigned int height, unsigned int heightPlusFp, unsigned int heightPlusFpPlusSync, unsigned int heightPlusFpPlusSyncPlusBp,
			Polarity hsync, Polarity vsync)
	: m_pName(pName),
	  m_pixClkkHz(pixClkkHz),
	  m_idealVHz(idealVHz),

	  m_width(width),
	  m_hFp(widthPlusFp - width),
	  m_hSync(widthPlusFpPlusSync - widthPlusFp),
	  m_hBp(widthPlusFpPlusSyncPlusBp - widthPlusFpPlusSync),

	  m_height(height),
	  m_vFp(heightPlusFp - height),
	  m_vSync(heightPlusFpPlusSync - heightPlusFp),
	  m_vBp(heightPlusFpPlusSyncPlusBp - heightPlusFpPlusSync),

	  m_hsyncPol(hsync),
	  m_vsyncPol(vsync)
	{
	}

	virtual ~Modeline();

	Modeline AdjustToFitPixClk(unsigned int targetPixkHz);
	unsigned int ComputePixRatekHz(void);

	void Print(Printer &p);

	//////////////////////////

	static void SetDefaultModesList(std::list<Modeline> *p)
	{
		m_pList = p;
	}

	static std::list<Modeline> *GetDefaultModesList(void)
	{
		return m_pList;
	}

	static void AddDefaultModes(void);

	static bool FindModeline(const char *pName, Modeline &out);

	static std::list<Modeline> *m_pList;

	//////////////////////////

	const char *m_pName;
	unsigned int m_pixClkkHz, m_idealVHz;

	unsigned int m_width, m_hFp, m_hSync, m_hBp;
	unsigned int m_height, m_vFp, m_vSync, m_vBp;
	Polarity m_hsyncPol, m_vsyncPol;
};

#endif /* MODELINE_H_ */
