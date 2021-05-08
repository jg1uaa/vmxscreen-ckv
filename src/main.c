/*
 *----------------------------------------------------------------------
 *    T-Kernel 2.0 Software Package
 *
 *    Copyright 2011 by Ken Sakamura.
 *    This software is distributed under the T-License 2.0.
 *----------------------------------------------------------------------
 *
 *    Released by T-Engine Forum(http://www.t-engine.org/) at 2011/05/17.
 *    Modified by SASANO Takayoshi at 2015/01/19.
 *
 *----------------------------------------------------------------------
 */

/*
        main.C          screen driver : main part
 *
 */
#include "screen.h"
#include <tstring.h>
#include <tcode.h>

LOCAL	BOOL	suspended;		/* suspended state     */
LOCAL	ID	PorID;
LOCAL	ID	TskID;

#define	TASK_EXINF	((void *)CH4toW('v', 'm', 's', 'c'))
#define	TASK_PRI	35
#define	TASK_STKSZ	4096

#define	Read	0x01
#define	Write	0x02
#define	R_OK	Read
#define	W_OK	Write
#define	RW_OK	(R_OK | W_OK)

/*
        check parameter & set up address space
		= ER_OK : OK (size == 0)
		> ER_OK : OK (size > 0)
                < ER_OK : error
*/
LOCAL	ERR	checkParam(W mode, W size, W dsz, W okpat)
{
	if (dsz <= 0) return ER_OBJ;

	if ((mode & okpat) == 0) return ER_PAR;

	if (size < 0 || (size > 0 && size < dsz)) return ER_PAR;

	return (size == 0) ? ER_OK : (ER_OK + 1);
}
/*
        perform I/O requests
*/
LOCAL	ERR	rwfn(W mode, W start, W size, void *buf, W *asize)
{
	ERR	err;
	W	dsz;
	BOOL	set = (mode == Write);

	switch (start) {
	case DN_SCRSPEC:
		dsz = sizeof(DEV_SPEC);
		if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
			err = getSCRSPEC((DEV_SPEC*)buf);
		break;
	case DN_SCRLIST:
		dsz = getSCRLIST(NULL);
		if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
			err = getSCRLIST((TC*)buf);
		break;
	case DN_SCRNO:
		dsz = sizeof(W);
		if ((err = checkParam(mode, size, dsz, RW_OK)) > ER_OK)
			err = getsetSCRNO((W*)buf, suspended, set);
		break;
	case DN_SCRCOLOR:
		dsz = getsetSCRCOLOR(NULL, FALSE);
		if ((err = checkParam(mode, size, dsz, RW_OK)) > ER_OK)
			err = getsetSCRCOLOR((COLOR*)buf, set);
		break;
	case DN_SCRBMP:
		dsz = sizeof(BMP);
		if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
			err = getSCRBMP((BMP*)buf);
		break;
	case DN_SCRBRIGHT:
		dsz = sizeof(W);
		if ((err = checkParam(mode, size, dsz, RW_OK)) > ER_OK)
			err = getsetSCRBRIGHT((W*)buf, set);
		break;
	case DN_SCRUPDFN:
		dsz = sizeof(FP);
		if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
			err = getSCRUPDFN((FP*)buf);
		break;
	case DN_SCRVFREQ:
		dsz = sizeof(W);
		if ((err = checkParam(mode, size, dsz, RW_OK)) > ER_OK)
			err = getsetSCRVFREQ((W*)buf, set);
		break;
	case DN_SCRADJUST:
		dsz = sizeof(ScrAdjust);
		if ((err = checkParam(mode, size, dsz, RW_OK)) > ER_OK)
			err = getsetSCRADJUST((ScrAdjust*)buf, set);
		break;
	case DN_SCRDEVINFO:
		dsz = sizeof(ScrDevInfo);
		if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
			err = getSCRDEVINFO((ScrDevInfo*)buf);
		break;
	case DN_SCRMEMCLK:
		dsz = 0;
		err = ER_NOSPT;
		break;
	case DN_SCRUPDRECT:
		dsz = sizeof(RECT);
		if ((err = checkParam(mode, size, dsz, W_OK)) > ER_OK)
			err = setSCRUPDRECT((RECT*)buf);
		break;
	case DN_SCRWRITE:
		dsz = size;
		if ((err = checkParam(mode, size, dsz, W_OK)) > ER_OK)
			err = setSCRWRITE(0, buf, dsz);
		break;
	default:
		if (start <= DN_SCRXSPEC(1) && start >= DN_SCRXSPEC(255)) {
			dsz = sizeof(DEV_SPEC);
			if ((err = checkParam(mode, size, dsz, R_OK)) > ER_OK)
				err = getSCRXSPEC((DEV_SPEC*)buf,
						 DN_SCRXSPEC(1) - start);
		} else {
			dsz = 0;
			err = ER_PAR;
		}
		break;
	}

	*asize = (err >= ER_OK) ? dsz : 0;
	return err;
}
/*
	check memory space
*/
LOCAL	ERR	checkTaskSpace(DevReq *q)
{
	ERR	err;

	/* no need to check */
	if (!q->cmd.adcnv || q->datacnt <= 0) {
		err = ER_OK;
		goto fin0;
	}

	/* set task space */
	err = SetTaskSpace(q->taskid);
	if (err < ER_OK) goto fin0;

	/* check address space */
	err = (q->cmd.cmd == DC_READ) ? CheckSpaceRW(q->memptr, q->datacnt) :
		CheckSpaceR(q->memptr, q->datacnt);
	if (err < ER_OK) goto fin0;

	err = ER_OK;
fin0:
	return err;
}
/*
	I/O request processing
*/
LOCAL	ERR	doRequest(DevReq *q, DevRsp *r)
{
	ERR	err;

	switch (q->cmd.cmd) {
	case	DC_READ:
		err = checkTaskSpace(q);
		err = (err < ER_OK) ? err : 
			rwfn(Read, q->datano, q->datacnt, q->memptr,
			     &r->datacnt);
		break;

	case	DC_WRITE:
		err = checkTaskSpace(q);
		err = (err < ER_OK) ? err : 
			rwfn(Write, q->datano, q->datacnt, q->memptr,
			     &r->datacnt);
		break;

	case	DC_OPEN:
	case	DC_CLOSE:
	case	DC_CLOSEALL:
		/* do nothing */
		err = ER_OK;
		break;

	case	DC_ABORT:
		/* do nothing */
		err = ER_OK;
		break;

	case	DC_SUSPEND:
		if (suspended == FALSE) {
			suspendSCREEN();
			suspended = TRUE;
		}
		err = ER_OK;
		break;

	case	DC_RESUME:
		if (suspended == TRUE) {
			resumeSCREEN();
			suspended = FALSE;
		}
		err = ER_OK;
		break;

	default:
		err = toERR(EC_PAR, ED_CMD);
	}

	return err;
}
/*
	main task
*/
LOCAL	void	mainTask(UW calptn)
{
	W	er, size;
	RNO	rno;
	DevReq	q;
	DevRsp	r;

	while (1) {
		er = acp_por(&rno, (void *)&q, &size, PorID, calptn);
		if (er < E_OK) goto fin0;

		if (size != sizeof(q)) continue;

		r.devid = q.devid;
		r.cmd = q.cmd;
		r.datano = q.datano;
		r.error.err = doRequest(&q, &r);

		rpl_rdv(rno, (void *)&r, sizeof(r));
	}

fin0:
	exd_tsk();
}
/*
	get task priority from argument
*/
LOCAL	PRI	getTaskPri(TC *arg)
{
	PRI	pri;

	pri = 0;
	for (; *arg != TK_NULL; arg++) {
		if (*arg == TK_EXCL) {
			pri = tc_strtol(arg + 1, NULL, 0);
			break;
		}
	}

	return (pri > 0) ? pri : TASK_PRI;
}
/*
	startup
*/
EXPORT	ERR	main(Bool start, TC *arg)
{
	ERR	err;
	T_CPOR	cpor = {
		.exinf = TASK_EXINF,
		.poratr = TA_NULL,
		.maxcmsz = sizeof(DevReq),
		.maxrmsz = sizeof(DevRsp),
	};
	T_CTSK	ctsk = {
		.exinf = TASK_EXINF,
		.task = mainTask,
		.itskpri = 0,
		.stksz = TASK_STKSZ,
		.tskatr = TA_HLNG | TA_RNG0,
	};
	LOCAL	const DevDef	def = {
		.attr = {
			.devinfo = 0,
			.devkind = DK_UNDEF,
			.reserved = 0,
			.openreq = 0,
			.lockreq = 0,
			.diskinfo = 0,
			.chardev = 1,
			.nowait = 0,
			.eject = 0,
		},
		.subunits = 0,
		.name = {TK_S, TK_C, TK_R, TK_E, TK_E, TK_N, TK_NULL},
		.portid = 0,
	};
	DevDef	ddef;

	/* epilogue */
	if (!start) {
		if (Vinf.attr & NEED_FINPROC) (*(Vinf.fn_setmode))(-1);
		err = ER_OK;
		goto fin4;
	}

	/* create rendezvous port */
	err = vcre_por(&cpor);
	if (err < E_OK) {
		err = toERR(EC_INNER, err);
		goto fin0;
	}
	PorID = (ID)err;

	/* create task and start */
	ctsk.itskpri = getTaskPri(arg);
	err = vcre_tsk(&ctsk);
	if (err < E_OK) {
		err = toERR(EC_INNER, err);
		goto fin1;
	}
	TskID = (ID)err;

	err = sta_tsk(TskID, D_NORM_PTN | D_ABORT_PTN);
	if (err < E_OK) {
		err = toERR(EC_INNER, err);
		goto fin2;
	}

	/* initialization */
	suspended = FALSE;

	/* device initialization processing */
	if ((err = initSCREEN()) < ER_OK) {
		goto fin3;
	}

	/* register device */
	ddef = def;
	ddef.portid = PorID;
	err = DefDevice(&ddef, NULL);
	if (err < E_OK) {
		finishSCREEN();
		goto fin3;
	}

	err = ER_OK;
	goto fin0;

fin4:
	ddef = def;
	ddef.portid = -1;
	DefDevice(&ddef, NULL);
fin3:
	ter_tsk(TskID);
fin2:
	del_tsk(TskID);
fin1:
	del_por(PorID);
fin0:
	return err;
}
