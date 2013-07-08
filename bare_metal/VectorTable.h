/*
 * VectorTable.h
 *
 *  Created on: 7 Jul 2013
 *      Author: simon
 */

#ifndef VECTORTABLE_H_
#define VECTORTABLE_H_

namespace VectorTable
{
	enum ExceptionType
	{
		kReset = 0,					//the initial supervisor stack
		kUndefinedInstruction = 1,
		kSupervisorCall = 2,
		kPrefetchAbort = 3,
		kDataAbort = 4,
		kIrq = 6,
		kFiq = 7,
	};
	typedef void(*ExceptionVector)(void);

	void EncodeAndWriteBranch(ExceptionVector v, ExceptionType e, unsigned int subtract = 0);
	void EncodeAndWriteLiteralLoad(ExceptionVector v, ExceptionType e);
	unsigned int *GetTableAddress(void);
};


#endif /* VECTORTABLE_H_ */
