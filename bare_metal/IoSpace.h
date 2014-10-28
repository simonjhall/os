/*
 * IoSpace.h
 *
 *  Created on: 26 Jul 2013
 *      Author: simon
 */

#ifndef IOSPACE_H_
#define IOSPACE_H_

#include <list>

class IoSpace
{
public:
	struct Entry
	{
		Entry(void)
		: m_valid(false)
		{
		}

		Entry(const char *pName,
				volatile unsigned *pVirt, volatile unsigned *pPhys,
				unsigned int length)
		: m_valid(true),
		  m_pName(pName),
		  m_pVirt(pVirt),
		  m_pPhys(pPhys),
		  m_length(length)
		{
		}

		operator bool() const
		{
			return m_valid;
		}

		operator volatile unsigned int *() const
		{
			if (!m_valid)
				asm ("bkpt");
			return m_pVirt;
		}

		bool m_valid;

		const char *m_pName;
		volatile unsigned *m_pVirt, *m_pPhys;
		unsigned int m_length;
	};

	Entry &Get(const char *pName);
	void Map(void);
	void MapVirtAsPhys(void);

	static void SetDefaultIoSpace(IoSpace *p)
	{
		m_pSpace = p;
	}

	static IoSpace *GetDefaultIoSpace(void)
	{
		return m_pSpace;
	}

protected:
	IoSpace(volatile unsigned int *pVirtBase);
	IoSpace();
	virtual ~IoSpace();

	virtual void Fill(void);

	std::list<Entry> m_all;
	Entry m_invalid;

	volatile unsigned int *m_pVirtBase;

	static IoSpace *m_pSpace;
};

namespace OMAP4460
{

class OmapIoSpace : public IoSpace
{
public:
	OmapIoSpace(volatile unsigned int *pVirtBase);
	OmapIoSpace(void);
	~OmapIoSpace(void);

protected:
	virtual void Fill(void);
};

}

namespace VersatilePb
{

class VersIoSpace : public IoSpace
{
public:
	VersIoSpace(volatile unsigned int *pVirtBase);
	VersIoSpace(void);
	~VersIoSpace(void);

protected:
	virtual void Fill(void);
};

}

#endif /* IOSPACE_H_ */
