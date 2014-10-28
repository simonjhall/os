/*
 * EhciRootHub.h
 *
 *  Created on: 9 Aug 2013
 *      Author: simon
 */

#ifndef EHCIROOTHUB_H_
#define EHCIROOTHUB_H_

#include "Hub.h"
#include <vector>

namespace USB
{

class Ehci;

class EhciRootHub: public USB::Hub
{
	class EhciPort : public USB::Port
	{
		friend class EhciRootHub;

	protected:
		EhciRootHub &m_rHub;
		unsigned int m_portNumber;

	public:
		EhciPort(EhciRootHub &rHub, unsigned int portNo);
		virtual ~EhciPort();

		virtual bool PowerOn(bool o);
		virtual bool Reset(void);
		virtual bool IsPoweredOn(void);
		virtual bool IsDeviceAttached(void);
		virtual Speed GetPortSpeed(void);
	};

	Ehci &m_rHostController;
	std::vector<EhciPort> m_ports;

public:
	EhciRootHub(Ehci &rHostController);
	virtual ~EhciRootHub();

	virtual Port *GetPort(unsigned int);
	virtual unsigned int GetNumPorts(void);

	virtual bool IsRootHub(void);
};

} /* namespace USB */
#endif /* EHCIROOTHUB_H_ */
