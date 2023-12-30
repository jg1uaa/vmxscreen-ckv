/*
	bga.c	screen driver
        video board / chip dependent processing: Bochs BGA

	Copyright 2017 by SASANO Takayoshi
	This software is distributed under the T-License 2.0.
*/
#include "screen.h"
#include "videomode.h"
#include <driver/pcat/pci.h>
#include <driver/pcat/sys.h>
#include <kernel/segment.h>
#include <bsys/util.h>

#ifdef USE_DEVICE_VIDEOMODE_H
#define	VIDEOMODE	DM1600x32
#else
#define	VIDEOMODE	0
#endif

#define	SUPPORT_MODEMAP	(1 << VIDEOMODE)

#define	VENDOR_BOCHS	0x1234
#define	DEVICE_BGA	0x1111

#define	VENDOR_INNOTEK	0x80ee
#define	DEVICE_VBOX	0xbeef

#define	BGA_INDEX	0x1ce
#define	BGA_DATA	0x1cf

#define	PALETTE_INDEX	0x3c8
#define	PALETTE_DATA	0x3c9

#define	regID		0
#define	regXRES		1
#define	regYRES		2
#define	regBPP		3
#define	regENABLE	4

#define	SUPPORT_BGA_ID	0xb0c4
#define	BGA_VRAM_SIZE	((BGA_ID == SUPPORT_BGA_ID) ? 8388608 : 16777216)

LOCAL	UH	BGA_ID;

Inline	void	WriteBGA(UH index, UH value)
{
	out_h(BGA_INDEX, index);
	out_h(BGA_DATA, value);
	return;
}

Inline	UH	ReadBGA(UH index)
{
	out_h(BGA_INDEX, index);
	return in_h(BGA_DATA);
}

LOCAL	ERR	BGAinit(void)
{
	ERR	err;

	/* enable FrameBuffer and I/O register */
	outPciConfH(Vinf.pciaddr, PCR_COMMAND,
		    inPciConfH(Vinf.pciaddr, PCR_COMMAND) | 0x0007);

	/* check BGA ID */
	if ((BGA_ID = ReadBGA(regID)) < SUPPORT_BGA_ID) {
		err = ER_OBJ;
		goto fin0;
	}

	/* map FrameBuffer */
	Vinf.framebuf_addr =
		(void *)(inPciConfW(Vinf.pciaddr, PCR_BASEADDR_0) & ~0x0f);
	Vinf.framebuf_total = BGA_VRAM_SIZE;
	err = mapFrameBuf(Vinf.framebuf_addr, Vinf.framebuf_total,
			 &Vinf.f_addr);
	if (err < ER_OK) goto fin0;

	err = ER_OK;
fin0:
	return err;
}

/* set color map */
LOCAL	void	BGAsetcmap(COLOR *cmap, W index, W entries)
{
#if defined(COLOR_CMAP256)
	W	i;
	UW	imask;

	for (i = 0; i < entries; i++) {
		DI(imask);
		out_b(PALETTE_INDEX, index);
		out_b(PALETTE_DATA, *cmap >> 16);
		out_b(PALETTE_DATA, *cmap >> 8);
		out_b(PALETTE_DATA, *cmap);
		cmap++;
		index++;
		EI(imask);
	}

#else
	/* no support, do nothing */

#endif
	return;
}

/* set display mode */
LOCAL	void	BGAsetmode(W flg)
{
	W	bpp;

	WriteBGA(regENABLE, 0);

	/* exit */
	if (flg < 0) return;

	/* enter BGA mode */
	bpp = (Vinf.pixbits >> 8) & 0xff;

	WriteBGA(regXRES, Vinf.act_width);
	WriteBGA(regYRES, Vinf.act_height);
	WriteBGA(regBPP, bpp);
	WriteBGA(regENABLE, 0x61);	/* LFB, 8bit DAC, enabled */

	/* fix display mode information */
	Vinf.width = Vinf.fb_width = Vinf.act_width;
	Vinf.height = Vinf.fb_height = Vinf.act_height;
	Vinf.rowbytes = Vinf.framebuf_rowb = Vinf.act_width * (bpp / 8);
	Vinf.vramsz = Vinf.framebuf_rowb * Vinf.fb_height;

	return;
}

/* initialization processing */
EXPORT	W	BGAInit(void)
{
	ERR	err;

	/* probe device */
	if ((Vinf.pciaddr = searchPciDev(VENDOR_BOCHS, DEVICE_BGA)) < 0 &&
	    (Vinf.pciaddr = searchPciDev(VENDOR_INNOTEK, DEVICE_VBOX)) < 0) {
		err = 0;
		goto fin0;
	}

	/* check device */
	err = BGAinit();
	if (err < ER_OK) goto fin0;

	/* set Vinf */
	Vinf.reqmode = Vinf.curmode = VIDEOMODE;
	// Vinf.framebuf_addr is already set
	// Vinf.f_addr is already set
	strncpy(Vinf.chipinf, "Bochs Graphics Adapter", L_CHIPINF);
	// Vinf.framebuf_total is already set
	Vinf.attr |= (LINEAR_FRAMEBUF | NEED_FINPROC);
	Vinf.attr &= ~(BPP_24 | USE_VVRAM);
	Vinf.fn_setcmap = BGAsetcmap;
	Vinf.fn_setmode = BGAsetmode;
	Vinf.modemap = SUPPORT_MODEMAP;

	/* these values are temporally, fix them at BGAsetmode() */
	Vinf.fb_width = Vinf.framebuf_rowb = VideoHsize(Vinf.reqmode);
	Vinf.fb_height = VideoVsize(Vinf.reqmode);
	Vinf.framebuf_rowb *= (VideoPixBits(Vinf.reqmode) >> 11) & 0x1f;

	err = 1;
	goto fin0;

fin0:
	return err;
}
