/*
	none.c	screen driver
	video board / chip dependent processing: None (dummy)

	Copyright 2021 by SASANO Takayoshi
	This software is distributed under the T-License 2.0.
*/
#include "screen.h"
#include "videomode.h"
#include <btron/memory.h>

#ifdef USE_DEVICE_VIDEOMODE_H
#define	VIDEOMODE	DM1600x32
#else
#define	VIDEOMODE	0
#endif

#define	SUPPORT_MODEMAP	(1 << VIDEOMODE)

#define	NONE_VRAM_SIZE	16777216

LOCAL	ERR	Nonesetup(void)
{
	ERR	err;
	W	nblk;
	M_STATE	sts;

	Vinf.framebuf_addr = NULL;
	Vinf.framebuf_total = NONE_VRAM_SIZE;

	/* get memory block size */
	err = b_mbk_sts(&sts);
	if (err < ER_OK) goto fin0;
	nblk = (Vinf.framebuf_total - 1) / sts.blksz + 1;

	/* allocate (dummy) FrameBuffer */
	err = b_get_mbk(&Vinf.f_addr, nblk, M_SYSTEM | M_RESIDENT);
	if (err < ER_OK) goto fin0;

	err = ER_OK;
fin0:
	return err;
}

/* set color map */
LOCAL	void	Nonesetcmap(COLOR *cmap, W index, W entries)
{
	/* do nothing */
	return;
}

/* set display mode */
LOCAL	void	Nonesetmode(W flg)
{
	W	bpp;

	/* exit */
	if (flg < 0) return;

	bpp = (Vinf.pixbits >> 8) & 0xff;

	/* fix display mode information */
	Vinf.width = Vinf.fb_width = Vinf.act_width;
	Vinf.height = Vinf.fb_height = Vinf.act_height;
	Vinf.rowbytes = Vinf.framebuf_rowb = Vinf.act_width * (bpp / 8);
	Vinf.vramsz = Vinf.framebuf_rowb * Vinf.fb_height;

	return;
}

/* initialization processing */
EXPORT	W	NoneInit(void)
{
	ERR	err;

	err = Nonesetup();
	if (err < ER_OK) goto fin0;

	/* set Vinf */
	Vinf.reqmode = Vinf.curmode = VIDEOMODE;
	// Vinf.framebuf_addr is already set
	// Vinf.f_addr is already set
	strncpy(Vinf.chipinf, "None", L_CHIPINF);
	// Vinf.framebuf_total is already set
	Vinf.attr |= (LINEAR_FRAMEBUF | NEED_FINPROC);
	Vinf.attr &= ~(BPP_24 | USE_VVRAM);
	Vinf.fn_setcmap = Nonesetcmap;
	Vinf.fn_setmode = Nonesetmode;
	Vinf.modemap = SUPPORT_MODEMAP;

	/* these values are temporally, fix them at Nonesetmode() */
	Vinf.fb_width = Vinf.framebuf_rowb = VideoHsize(Vinf.reqmode);
	Vinf.fb_height = VideoVsize(Vinf.reqmode);
	Vinf.framebuf_rowb *= (VideoPixBits(Vinf.reqmode) >> 11) & 0x1f;

	err = 1;
	goto fin0;

fin0:
	return err;
}
