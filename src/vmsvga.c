/*
	vmsvga.c	screen driver
        video board / chip dependent processing: VMware SVGA II

	Copyright 2015-2017 by SASANO Takayoshi
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

struct _vmxinf {
	FastLock	lock;
	UH		ioaddr;
	UW		id;
	void		*fifobase;
	UW		fifosize;
	W		fifoentry;
	_UW		*fifomem;
};

LOCAL	struct _vmxinf	VMXinf;

#define	VENDOR_VMWARE	0x15ad
#define	DEVICE_SVGA2	0x0405

#define	regID		0
#define	regENABLE	1
#define	regWIDTH	2
#define	regHEIGHT	3
#define	regBPP		7
#define	regPITCH	12
#define	regVRAMSIZE	15
#define	regCAP		17
#define	regPALETTE0	17
#define	regFIFOSIZE	19
#define	regCONFIG	20
#define	regSYNC		21
#define	regBUSY		22
#define	regIRQMASK	33
#define	regPALETTE	1024

#define	regID_MAGIC(x)	(0x90000000 | ((x) & 0xff))
#define	regCAP_IRQMASK	(1 << 18)

#define	fifoMIN		0
#define	fifoMAX		1
#define	fifoNEXT	2
#define	fifoSTOP	3

/*
 * VMware SVGA Device Developer Kit sets 0x48c to fifoMIN (SVGA_FIFO_MIN)
 * register, this means the last index of FIFO's register area
 * (SVGA_FIFO_NUM_REGS = 291) * sizeof(uint32). We use larger value.
 */
#define	fifoMIN_MIN	0x00001000
#define	regFIFOSIZE_MIN	0x00010000	// QEMU returns 0x00010000, use this

struct _fifocmd {
	UW	cmd;
	UW	x;
	UW	y;
	UW	width;
	UW	height;
} __attribute__((packed));

#define	fifoCMD_UPDATE	1
#define	CMD_ENTRY_MIN	2	/* minimal value */

Inline	void	WriteSVGA(UW index, UW value)
{
	out_w(VMXinf.ioaddr + 0, index);
	out_w(VMXinf.ioaddr + 1, value);
	return;
}

Inline	UW	ReadSVGA(UW index)
{
	out_w(VMXinf.ioaddr + 0, index);
	return in_w(VMXinf.ioaddr + 1);
}

LOCAL	void	VMSVGAsync(void)
{
	WriteSVGA(regSYNC, 1);
	while (ReadSVGA(regBUSY));
	return;
}

LOCAL	ERR	VMSVGAinit(void)
{
	ERR	err;
	W	i, n, v[L_DEVCONF_VAL];

	/* create lock */
	err = CreateLockWN(&VMXinf.lock, "vmsc");
	if (err < ER_OK) goto fin0;

	/* enable FrameBuffer and I/O register */
	outPciConfH(Vinf.pciaddr, PCR_COMMAND,
		    inPciConfH(Vinf.pciaddr, PCR_COMMAND) | 0x0007);

	/* map I/O region */
	VMXinf.ioaddr = inPciConfW(Vinf.pciaddr, PCR_BASEADDR_0) & ~0x03;

	/* VMware SVGA II protocol negotiation */
	for (i = 2; i >= 0; i--) {
		n = regID_MAGIC(i);
		WriteSVGA(regID, n);
		if (ReadSVGA(regID) == n) {
			VMXinf.id = n;
			break;
		}
	}
	if (i < 0) {
		err = ER_OBJ;
		goto fin1;
	}

	/* map FrameBuffer */
	Vinf.framebuf_addr =
		(void *)(inPciConfW(Vinf.pciaddr, PCR_BASEADDR_1) & ~0x0f);
	Vinf.framebuf_total = ReadSVGA(regVRAMSIZE);
	err = mapFrameBuf(Vinf.framebuf_addr, Vinf.framebuf_total,
			 &Vinf.f_addr);
	if (err < ER_OK) goto fin1;

	/* extended stuff (FIFO, interrupt) */
	VMXinf.fifosize = 0;
	if (VMXinf.id != regID_MAGIC(0)) {
		/* regFIFOSIZE is untrusted, do sanity check */
		n = ReadSVGA(regFIFOSIZE);
		if (n >= regFIFOSIZE_MIN) {
			VMXinf.fifosize = n;
			VMXinf.fifobase =
				(void *)(inPciConfW(Vinf.pciaddr,
						    PCR_BASEADDR_2) & ~0x0f);
		}

		/* disable interrupt */
		if (ReadSVGA(regCAP) & regCAP_IRQMASK) {
			WriteSVGA(regIRQMASK, 0);
		}
	}

	/* get FIFO entry size (default:maximum, 0 disables FIFO) */
	VMXinf.fifoentry = (GetDevConf("VMSVGACMDENTRY", v) > 0) ? v[0] : -1;

	/* USE_VVRAM is controlled by VMSVGACMDENTRY */
	if (!VMXinf.fifosize || !VMXinf.fifoentry) {
		VMXinf.fifosize = 0;
		Vinf.attr &= ~USE_VVRAM;
	} else {
		Vinf.attr |= USE_VVRAM;
	}

	/* map FIFO */
	err = VMXinf.fifosize ?
		MapMemory(VMXinf.fifobase, VMXinf.fifosize,
			  MM_SYSTEM | MM_READ | MM_WRITE | MM_CDIS,
			  (void *)&VMXinf.fifomem) : ER_OK;
	if (err < ER_OK) {
		VMXinf.fifosize = 0;
		Vinf.attr &= ~USE_VVRAM;
	}

	err = ER_OK;
	goto fin0;

fin1:
	DeleteLock(&VMXinf.lock);
fin0:
	return err;
}

LOCAL	void	VMSVGAupdatecmd(W x, W y, W dx, W dy)
{
	W	min, max, next, stop, remain;
	struct _fifocmd	*cmd;

	/* FIFO disabled */
	if (!VMXinf.fifosize) goto fin0;

	/* gather current FIFO status */
	min = VMXinf.fifomem[fifoMIN];
	max = VMXinf.fifomem[fifoMAX];
	next = VMXinf.fifomem[fifoNEXT];
	stop = VMXinf.fifomem[fifoSTOP];

	/* put new command */
	cmd = ((void *)VMXinf.fifomem) + next;
	cmd->cmd = fifoCMD_UPDATE;
	cmd->x = x;
	cmd->y = y;
	cmd->width = dx;
	cmd->height = dy;

	/* calculate FIFO free area */
	next += sizeof(struct _fifocmd);
	remain = stop - next;
	if (remain < 0) remain += max - min;

	/* update next pointer */
	if (next >= max) next = min;
	VMXinf.fifomem[fifoNEXT] = next;

	/* no enough space for next time, flush now */
	if (remain <= sizeof(struct _fifocmd)) VMSVGAsync();

fin0:
	return;
}

/* update region */
LOCAL	void	VMSVGAupdate(W x, W y, W dx, W dy)
{
	Lock(&VMXinf.lock);
	VMSVGAupdatecmd(x, y, dx, dy);
	Unlock(&VMXinf.lock);
	return;
}

/* set color map */
LOCAL	void	VMSVGAsetcmap(COLOR *cmap, W index, W entries)
{
#if defined(COLOR_CMAP256)
	W	i, reg;

	reg = (VMXinf.id == regID_MAGIC(0)) ? regPALETTE0 : regPALETTE;

	Lock(&VMXinf.lock);
	for (i = index; i < index + entries; i++) {
		WriteSVGA(reg + i * 3 + 0, (*cmap >> (16)) & 0xff);
		WriteSVGA(reg + i * 3 + 1, (*cmap >> (8)) & 0xff);
		WriteSVGA(reg + i * 3 + 2, (*cmap >> (0)) & 0xff);
		cmap++;
	}
	Unlock(&VMXinf.lock);

#else
	/* no support, do nothing */

#endif
	return;
}

/* set display mode */
LOCAL	void	VMSVGAsetmode(W flg)
{
	W	max;

	/* exit VMware SVGA II mode, required for warm reboot */
	// XXX the last contents of VGA mode is redisplayed when exiting.
	if (flg < 0) {
		Lock(&VMXinf.lock);
		if (VMXinf.fifosize) {
			VMSVGAsync();
			WriteSVGA(regCONFIG, 0);
		}
		WriteSVGA(regENABLE, 0);
		VMXinf.fifosize = 0;
		Vinf.attr &= ~USE_VVRAM;
		Unlock(&VMXinf.lock);
		return;
	}

	/* initialize */
	WriteSVGA(regWIDTH, Vinf.act_width);
	WriteSVGA(regHEIGHT, Vinf.act_height);
	WriteSVGA(regBPP, Vinf.pixbits >> 8);

	if (VMXinf.fifosize) {
		max = (VMXinf.fifosize - fifoMIN_MIN) / sizeof(struct _fifocmd);
		if (VMXinf.fifoentry < 0 || VMXinf.fifoentry > max)
			VMXinf.fifoentry = max;
		if (VMXinf.fifoentry < CMD_ENTRY_MIN)
			VMXinf.fifoentry = CMD_ENTRY_MIN;

		VMXinf.fifomem[fifoMIN] =
			VMXinf.fifomem[fifoNEXT] = VMXinf.fifomem[fifoSTOP] =
			VMXinf.fifosize - sizeof(struct _fifocmd) *
			VMXinf.fifoentry;
		VMXinf.fifomem[fifoMAX] = VMXinf.fifosize;
		WriteSVGA(regCONFIG, 1);
	} else {
		WriteSVGA(regCONFIG, 0);
	}

	/* enter VMware SVGA II mode */
	/* QEMU requires regENABLE=1 after regCONFIG=1 */
	WriteSVGA(regENABLE, 1);

	/* fix display mode information */
	Vinf.width = Vinf.fb_width = Vinf.act_width;
	Vinf.height = Vinf.fb_height = Vinf.act_height;
	Vinf.rowbytes = Vinf.framebuf_rowb = ReadSVGA(regPITCH);
	Vinf.vramsz = Vinf.framebuf_rowb * Vinf.fb_height;

	return;
}

/* suspend / resume processing */
LOCAL	void	VMSVGAsuspend(BOOL suspend)
{
        /* in the case of suspend, clear Video-RAM content */
	if (suspend) {
		Lock(&VMXinf.lock);
//		memset(Vinf.f_addr, 0, Vinf.framebuf_total);
//		VMSVGAupdatecmd(0, 0, Vinf.width, Vinf.height);
		if (VMXinf.fifosize) VMSVGAsync();
	} else {
		Unlock(&VMXinf.lock);
	}

	return;
}

/* initialization processing */
EXPORT	W	VMSVGAInit(void)
{
	ERR	err;

	/* probe device */
	Vinf.pciaddr = searchPciDev(VENDOR_VMWARE, DEVICE_SVGA2);
	if (Vinf.pciaddr < 0) {
		err = 0;
		goto fin0;
	}

	/* check device */
	err = VMSVGAinit();
	if (err < ER_OK) goto fin0;

	/* set Vinf */
	Vinf.reqmode = Vinf.curmode = VIDEOMODE;
	// Vinf.framebuf_addr is already set
	// Vinf.f_addr is already set
	strncpy(Vinf.chipinf, "VMware SVGA II", L_CHIPINF);
	// Vinf.framebuf_total is already set
	Vinf.attr |= (LINEAR_FRAMEBUF | NEED_FINPROC | NEED_SUSRESPROC);
	Vinf.attr &= ~BPP_24;
	Vinf.fn_setcmap = VMSVGAsetcmap;
	Vinf.fn_setmode = VMSVGAsetmode;
	Vinf.fn_susres = VMSVGAsuspend;
	Vinf.modemap = SUPPORT_MODEMAP;

	if (Vinf.attr & USE_VVRAM) {
		Vinf.fn_updscr = VMSVGAupdate;
		Vinf.v_addr = Vinf.f_addr;
	}

	/* these values are temporally, fix them at VMSVGAsetmode() */
	Vinf.fb_width = Vinf.framebuf_rowb = VideoHsize(Vinf.reqmode);
	Vinf.fb_height = VideoVsize(Vinf.reqmode);
	Vinf.framebuf_rowb *= (VideoPixBits(Vinf.reqmode) >> 11) & 0x1f;

	err = 1;
	goto fin0;

fin0:
	return err;
}
