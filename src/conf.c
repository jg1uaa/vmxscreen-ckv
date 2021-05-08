/*
 *----------------------------------------------------------------------
 *    T-Kernel 2.0 Software Package
 *
 *    Copyright 2011 by Ken Sakamura.
 *    This software is distributed under the T-License 2.0.
 *----------------------------------------------------------------------
 *
 *    Released by T-Engine Forum(http://www.t-engine.org/) at 2011/05/17.
 *    Modified by SASANO Takayoshi at 2021/05/08.
 *
 *----------------------------------------------------------------------
 */

/*
 *      conf.c          screen driver
 *
 *      initialization table that is tailored to the needs of video board and chip
 *
 */
#include	<basic.h>

/* video controller initialization processing */
IMPORT	W	VMSVGAInit(void);
IMPORT	W	BGAInit(void);
IMPORT	W	NoneInit(void);

/*
        initialization table that is tailored to the needs of video board and chip
*/
EXPORT	FUNCP	VideoFunc[] = {
	(FUNCP)VMSVGAInit,
	(FUNCP)BGAInit,
	(FUNCP)NoneInit,
	NULL,
};
