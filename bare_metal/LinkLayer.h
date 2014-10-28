/*
 * LinkLayer.h
 *
 *  Created on: 11 Oct 2014
 *      Author: simon
 */

#ifndef LINKLAYER_H_
#define LINKLAYER_H_

class LinkAddrIntf
{
protected:
	inline LinkAddrIntf()
	{
	}
};

class LinkIntf
{
public:
	inline virtual ~LinkIntf() {};

	virtual bool SendTo(LinkAddrIntf &rDest, void *pData, unsigned int len) = 0;
	virtual bool Recv(LinkAddrIntf **ppFrom, void *pData, unsigned int &rLen, unsigned int maxLen) = 0;

	virtual int GetMtu() = 0;

	//todo
	virtual LinkAddrIntf *GetAddr(void) = 0;
	virtual LinkAddrIntf *GetPtpAddr(void) = 0;
};


#endif /* LINKLAYER_H_ */
