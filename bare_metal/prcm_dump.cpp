/*
 * prcm_dump.cpp
 *
 *  Created on: 19 Jul 2013
 *      Author: simon
 */

#ifdef __ARM_ARCH_7A__

#include "print.h"
#include "common.h"

static unsigned int conv_u(unsigned int u)
{
	if ((u >> 20) == 0xe02)
		return (u & 1048575) | (0x4a0 << 20);
	else if ((u >> 20) == 0xe01)
		return (u & 1048575) | (0x4a3 << 20);
	else
	{
		ASSERT(0);
		return 0;
	}
}

void PrcmDump(Printer &p)
{
	p << "DeviceName OMAP4460_ES1.x\n";
	unsigned int u;

	// 0xe0204 == CM1
	// 0xe0208 == CM2

	u = 0xe0204100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204120; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204124; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204128; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020412c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204130; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204134; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204138; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020413c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204140; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204144; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204148; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020414c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204150; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204160; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204164; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204168; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020416c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204170; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204188; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020418c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020419c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041a8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041ac; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041b8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041bc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041c8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041cc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041dc; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041e8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041ec; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041f0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02041f4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020420c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204260; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204264; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204270; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204280; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204404; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204408; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204420; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204500; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204520; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204528; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204530; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204538; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204540; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204548; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204550; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204558; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204560; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204568; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204570; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204578; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204580; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0204588; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208114; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208118; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020811c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208124; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208128; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020812c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208130; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208138; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208140; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208144; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208148; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020814c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208150; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208154; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208158; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020815c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208160; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208164; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208180; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208184; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208188; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe020818c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208190; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02081b4; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208600; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208628; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208630; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208638; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208640; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208700; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208708; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208720; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208800; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208808; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208820; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208828; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208830; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208900; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208904; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208908; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208920; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208a20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b38; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208b40; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208c30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d30; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208d38; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208e40; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f04; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0208f28; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209000; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209004; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209008; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209020; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209028; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209120; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209200; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209204; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209220; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209300; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209304; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209308; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209328; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209330; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209338; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209358; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209360; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209368; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02093d0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02093e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209408; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209428; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209430; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209438; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209440; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209448; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209450; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209458; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209460; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209468; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209470; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209478; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209480; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209488; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094a0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094a8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094b0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094b8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094c0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094e0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094f0; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe02094f8; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209500; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209508; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209520; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209528; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209538; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209540; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209548; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209550; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209558; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0209560; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	//PRM
	u = 0xe0106100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0106108; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010610c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0106110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107800; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107820; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107830; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107838; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107840; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107850; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107860; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107878; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107888; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a00; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a08; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe0107a20; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	//SCRM
	u = 0xe010a000; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a100; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a104; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a110; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a11c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a204; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a208; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a210; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a214; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a218; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a21c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a220; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a224; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a234; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a310; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a314; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a318; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a31c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a320; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a324; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a400; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a41c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a420; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a510; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a514; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
	u = 0xe010a51c; p << "0x" << conv_u(u) <<  " 0x" << *(volatile unsigned int *)u << "\n";
}

#endif
