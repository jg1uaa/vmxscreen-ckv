#ifdef USE_DEVICE_VIDEOMODE_H
#include <device/pcat/videomode.h>

#else
#define	MAX_VIDEO_MODE		1
#define	VALID_VIDEO_MODE(mode)	0

#define	VideoHsize(mode) ((Vinf.attr & USE_VVRAM) ? 3840 : \
			  (Vinf.framebuf_total >= 16777216) ? 2560 : 1920)
#define	VideoVsize(mode) ((Vinf.attr & USE_VVRAM) ? 1920 : \
			  (Vinf.framebuf_total >= 16777216) ? 1600 : 1080)

#if defined(COLOR_CMAP256)
#define	VideoPixBits(mode)	0x0808
#define	VideoCmapEnt(mode)	256
#define	VideoRedInf(mode)	0
#define	VideoGreenInf(mode)	0
#define	VideoBlueInf(mode)	0

#elif defined(COLOR_RGB565)
#define	VideoPixBits(mode)	0x1010
#define	VideoCmapEnt(mode)	0
#define	VideoRedInf(mode)	0x0b05
#define	VideoGreenInf(mode)	0x0506
#define	VideoBlueInf(mode)	0x0005

#else
#define	VideoPixBits(mode)	0x2018
#define	VideoCmapEnt(mode)	0
#define	VideoRedInf(mode)	0x1008
#define	VideoGreenInf(mode)	0x0808
#define	VideoBlueInf(mode)	0x0008

#endif

#endif
