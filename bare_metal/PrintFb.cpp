/*
 * PrintFb.cpp
 *
 *  Created on: 7 Sep 2014
 *      Author: simon
 */

#include <string.h>
#include "PrintFb.h"

#include "vincent.h"

PrinterFb::PrinterFb(unsigned int *pFramebuffer, int width, int height, PixelFormat pf,
		Printer *p)
: m_pFramebuffer(pFramebuffer),
  m_pFramebufferBin((unsigned char *)pFramebuffer),
  m_width(width),
  m_height(height),
  m_pixelFormat(pf),
  m_x(0),
  m_y(0),
  m_pChainedPrinter(p)
{
	//initialise the translation table
	for (unsigned int i = 0; i < 256; i++)
	{
		for (unsigned int j = 0; j < 8; j++)
		{
			unsigned int bit = i & (1 << j);
			if (bit)
				byte_to_line[i][7 - j] = 0xffffff;
			else
				byte_to_line[i][7 - j] = 0;
		}
	}
}

PrinterFb::~PrinterFb()
{
}

void PrinterFb::PrintChar(char c)
{
	if (m_pChainedPrinter)
		m_pChainedPrinter->PrintChar(c);

	if (c == '\n')
		return;
	else if (c == '\r')
	{
		m_x = 0;
		m_y += 10;
		return;
	}
	else if (c == '\t')
	{
		m_x += 3 * 8;
		return;
	}

	if (m_x + 8 > m_width)
	{
		m_x = 0;
		m_y += 10;
	}

	if (m_pixelFormat == kxRGB8888)
	{
		if (m_y + 10 >= m_height)
		{
		/*	memcpy(m_pFramebuffer, m_pFramebuffer + m_width * 10, m_width * (m_height - 10) * 4);
			memset(m_pFramebuffer + m_width * (m_height - 10), 0, m_width * 10);
			m_y -= 10;*/
			memset(m_pFramebufferBin, 0, m_width * m_height * 4);
			m_y = 0;
		}

		for (int row = 0; row < 8; row++)
		{
			unsigned char v = vincent_data[c][row];
			for (int col = 0; col < 8; col++)
				m_pFramebuffer[(m_y + row) * m_width + (m_x + col)] = byte_to_line[v][col];
		}
	}
	else if (m_pixelFormat == kBinary1)
	{
		if (m_y + 10 >= m_height)
		{
			memcpy(m_pFramebufferBin, m_pFramebufferBin + (m_width >> 3) * 10, (m_width >> 3) * (m_height - 10));
			memset(m_pFramebufferBin + (m_width >> 3) * (m_height - 10), 0, (m_width >> 3) * 10);
			m_y -= 10;
		}

		for (int row = 0; row < 8; row++)
		{
			unsigned char v = vincent_data[c][row];
			m_pFramebufferBin[(m_y + row) * (m_width >> 3) + (m_x >> 3)] = v;
		}
	}

	m_x += 8;
}

void PrinterFb::SetChainedPrinter(Printer* p)
{
	m_pChainedPrinter = p;
}
