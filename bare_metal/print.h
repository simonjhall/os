/*
 * print.h
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#ifndef PRINT_H_
#define PRINT_H_

#include <stddef.h>

template <typename T>
struct Formatter
{
	enum Format
	{
		kDecimal,
		kHex,
	};

	Formatter(T v, Format f) : m_value(v), m_format(f) {};
	T m_value;
	Format m_format;
};

template <>
struct Formatter<float>
{
	Formatter(float v, unsigned int p) : m_value(v), m_places(p) {};
	float m_value;
	unsigned int m_places;
};

class Printer
{
public:
	virtual ~Printer(void) {};

	virtual void PrintString(const char *pString, bool with_len = false, size_t len = 0);

	virtual void PrintHex(unsigned int i);
	virtual void PrintHex(int i) { PrintHex((unsigned int)i); };

	virtual void PrintDec(unsigned int i, bool leading);
	virtual void PrintDec(int i, bool leading) { PrintDec((unsigned int)i, leading); };

	virtual void PrintChar(char c) = 0;

	Printer &operator << (const char *pString)
	{
		PrintString(pString);
		return *this;
	}

	Printer &operator << (char c)
	{
		PrintChar(c);
		return *this;
	}

	Printer &operator << (unsigned int i)
	{
		PrintHex(i);
		return *this;
	}

	Printer &operator << (int i)
	{
		PrintHex(i);
		return *this;
	}

	Printer &operator << (void *i)
	{
		PrintHex((unsigned int)i);
		return *this;
	}

	Printer &operator << (volatile void *i)
	{
		PrintHex((unsigned int)i);
		return *this;
	}

	Printer &operator << (Formatter<float> i)
	{
		PrintDec((unsigned int)i.m_value, false);

		float fraction = i.m_value - (float)(unsigned int)i.m_value;

		unsigned int scale = 1;
		for (unsigned int count = 0; count < i.m_places; count++)
			scale *= 10;

		PrintChar('.');

		PrintDec((unsigned int)(fraction * scale), false);
		return *this;
	}

private:
	virtual char *NibbleToHexString(char *, unsigned int);
};

#endif /* PRINT_H_ */
