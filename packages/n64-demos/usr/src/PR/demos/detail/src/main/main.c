/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright  law.  They  may not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 *************************************************************************/

/*---------------------------------------------------------------------*
        Copyright (C) 1997,1998 Nintendo. (Originated by SGI)
        
        $RCSfile: main.c,v $
        $Revision: 1.20 $
        $Date: 1999/04/16 07:06:31 $
 *---------------------------------------------------------------------*/

/*
 * File:	onetri.c
 * Create Date:	Mon Apr 17 11:45:57 PDT 1995
 *
 * VERY simple app, draws a couple triangles spinning.
 *
 */

#include <ultra64.h>
#include <PR/ramrom.h>	/* needed for argument passing into the app */
#include <assert.h>

#include "app.h"
#include "walk_around.h"

/*
 * Symbol genererated by "makerom" to indicate the end of the code segment
 * in virtual (and physical) memory
 */
extern char _codeSegmentBssEnd[];

/*
 * Symbols generated by "makerom" to tell us where the static segment is
 * in ROM.
 */
extern char _staticSegmentRomStart[], _staticSegmentRomEnd[];

/*
 * Stacks for the threads as well as message queues for synchronization
 * This stack is ridiculously large, and could also be reclaimed once
 * the main thread is started.
 */
u64	bootStack[STACKSIZE/sizeof(u64)];

static void	idle(void *);
static void	mainproc(void *);

static OSThread	idleThread;
static u64	idleThreadStack[STACKSIZE/sizeof(u64)];

static OSThread	mainThread;
static u64	mainThreadStack[STACKSIZE/sizeof(u64)];

/* this number (the depth of the message queue) needs to be equal
 * to the maximum number of possible overlapping PI requests.
 * For this app, 1 or 2 is probably plenty, other apps might
 * require a lot more.
 */
#define NUM_PI_MSGS     8

static OSMesg PiMessages[NUM_PI_MSGS];
static OSMesgQueue PiMessageQ;

OSMesgQueue	dmaMessageQ, rdpMessageQ, retraceMessageQ;
OSMesg		dmaMessageBuf, rdpMessageBuf, retraceMessageBuf;
OSIoMesg	dmaIOMessageBuf;	/* see man page to understand this */

/*
 * Dynamic data.
 */
Dynamic dynamic;

/*
 * Task descriptor.
 */
OSTask	tlist =
{
    M_GFXTASK,			/* task type */
    OS_TASK_DP_WAIT,		/* task flags */
    NULL,			/* boot ucode pointer (fill in later) */
    0,				/* boot ucode size (fill in later) */
    NULL,			/* task ucode pointer (fill in later) */
    SP_UCODE_SIZE,		/* task ucode size */
    NULL,			/* task ucode data pointer (fill in later) */
    SP_UCODE_DATA_SIZE,		/* task ucode data size */
    &dram_stack[0],		/* task dram stack pointer */
    SP_DRAM_STACK_SIZE8,	/* task dram stack size */
    &rdp_output[0],		/* task fifo buffer start ptr */
    &rdp_output[0]+RDP_OUTPUT_LEN, /* task fifo buffer end ptr */
    NULL,			/* task data pointer (fill in later) */
    0,				/* task data size (fill in later) */
    NULL,			/* task yield buffer ptr (not used here) */
    0				/* task yield buffer size (not used here) */
};

Gfx		*glistp;	/* global for test case procs */
    
/*
 * global variables
 */
static float	theta = 0.0;
static int	rdp_flag = 0;
static int      draw_buffer = 0;

OSPiHandle	*handler;

void
boot(void)
{
    /* notice that you can't call osSyncPrintf() until you set
     * up an idle thread.
     */
    
    osInitialize();

    handler = osCartRomInit();

    osCreateThread(&idleThread, 1, idle, (void *)0,
		   idleThreadStack+STACKSIZE/sizeof(u64), 10);
    osStartThread(&idleThread);

    /* never reached */
}

static void
idle(void *arg)
{
    /* Initialize video */
    osCreateViManager(OS_PRIORITY_VIMGR);
    osViSetMode(&osViModeTable[OS_VI_NTSC_LAF1]);
    
    /*
     * Start PI Mgr for access to cartridge
     */
    osCreatePiManager((OSPri)OS_PRIORITY_PIMGR, &PiMessageQ, PiMessages, 
		      NUM_PI_MSGS);
    
    /*
     * Create main thread
     */
    osCreateThread(&mainThread, 3, mainproc, arg,
		   mainThreadStack+STACKSIZE/sizeof(u64), 10);
    
    osStartThread(&mainThread);

    /*
     * Become the idle thread
     */
    osSetThreadPri(0, 0);

    for (;;);
}

/*
 * This is the main routine of the app.
 */

static void
mainproc(void *arg)
{
    OSTask		*tlistp;
    Dynamic		*dynamicp;
    char		*staticSegment;
    float		fov = 40.0;
    float 		aspect = 1.333;
    float 		near_position = 50;
    float		far_position = 10000;
    u16			perspNorm;
    static int		flow = 0;
    static unsigned int	matrix = 2;
    static unsigned int	vidFilt = 1;
    int			up, down;

#ifdef DEBUG
    int i;
    char *ap;
    u32 *argp;
    u32 argbuf[16];

    argp = (u32 *)RAMROM_APP_WRITE_ADDR;
    for (i=0; i<sizeof(argbuf)/4; i++, argp++) {
	osEPiReadIo(handler, (u32)argp, &argbuf[i]); /* Assume no DMA */
    }
    /* Parse the options */
    ap = (char *)argbuf;
    while (*ap != '\0') {
	while (*ap == ' ')
	    ap++;
	if ( *ap == '-' && *(ap+1) == 'r') {
	    rdp_flag = 1;
	    ap += 2;
	}else{
	    ap++ ;
	}
    }
#endif
    
    /*
     * Setup the message queues
     */
    osCreateMesgQueue(&dmaMessageQ, &dmaMessageBuf, 1);
    
    osCreateMesgQueue(&rdpMessageQ, &rdpMessageBuf, 1);
    osSetEventMesg(OS_EVENT_DP, &rdpMessageQ, NULL);
    
    osCreateMesgQueue(&retraceMessageQ, &retraceMessageBuf, 1);
    osViSetEvent(&retraceMessageQ, NULL, 1);
    
    /*
     * Stick the static segment right after the code/data segment
     */
    staticSegment = _codeSegmentBssEnd;

    dmaIOMessageBuf.hdr.pri      = OS_MESG_PRI_NORMAL;
    dmaIOMessageBuf.hdr.retQueue = &dmaMessageQ;
    dmaIOMessageBuf.dramAddr     = staticSegment;
    dmaIOMessageBuf.devAddr      = (u32)_staticSegmentRomStart;
    dmaIOMessageBuf.size         = (u32)_staticSegmentRomEnd-(u32)_staticSegmentRomStart;

    osEPiStartDma(handler, &dmaIOMessageBuf, OS_READ);
    
    /*
     * Wait for DMA to finish
     */
    (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);

     if (walkAroundInit(-200, 100, -1000, -200, 100, 0) < 0) {
#ifdef DEBUG
       osSyncPrintf("Error: Unable to find controller\n");
#endif
     }

    /*
     * Main game loop
     */
    while (1) {

	/*
	 * pointers to build the display list.
	 */
	tlistp = &tlist;
	dynamicp = &dynamic;
	glistp = dynamicp->glist;

#ifdef NEVER
	guOrtho(&dynamicp->projection,
		-(float)SCREEN_WD/2.0F, (float)SCREEN_WD/2.0F,
		-(float)SCREEN_HT/2.0F, (float)SCREEN_HT/2.0F,
		1.0F, 10.0F, 1.0F);
#endif
	guPerspective(&dynamicp->projection, &perspNorm,
                fov, aspect, near_position, far_position, 1.0);
      

	/*
	 * Tell RCP where each segment is
	 */
	gSPSegment(glistp++, 0, 0x0);	/* Physical address segment */
	gSPSegment(glistp++, STATIC_SEGMENT, OS_K0_TO_PHYSICAL(staticSegment));
	gSPSegment(glistp++, CFB_SEGMENT, OS_K0_TO_PHYSICAL(cfb[draw_buffer]));
	gSPSegment(glistp++, CZB_SEGMENT, OS_K0_TO_PHYSICAL(czb));

	/*
	 * Initialize RDP state.
	 */
	gSPDisplayList(glistp++, rdpinit_dl);

	/*
	 * Initialize RSP state.
	 */
	gSPDisplayList(glistp++, rspinit_dl);

	/*
	 * Clear color framebuffer.
	 */
	gSPDisplayList(glistp++, clearczb_dl);
	gSPDisplayList(glistp++, clearcfb_dl);

	/*
	 * Main Display list 
   	 */
	walkAround(dynamicp, &up, &down);

	if (up & CONT_L) matrix = ((++matrix) % 4);
	switch (matrix) {
		case 0: gDPSetColorDither(glistp++, G_CD_NOISE);
		   	break;
		case 1: gDPSetColorDither(glistp++, G_CD_MAGICSQ);
                   	break;
		case 2: gDPSetColorDither(glistp++, G_CD_BAYER);
                   	break;
		case 3: gDPSetColorDither(glistp++, G_CD_DISABLE);
                   	break;
		default: break;
	}

	if (up & CONT_R) vidFilt = ~vidFilt;
	if (vidFilt == 1)
    		osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON);
	else 
		osViSetSpecialFeatures(OS_VI_DITHER_FILTER_OFF);

	gSPPerspNormalize(glistp++, perspNorm);
    	gSPDisplayList(glistp++, matrices_dl);
    	gSPDisplayList(glistp++, textri_dl);

	/* 
	 * Animate River Texture 
	 */
	flow++;
 	if (flow == 64) flow = 0;

	gDPSetTileSize(glistp++, 1, 
	      (0     << G_TEXTURE_IMAGE_FRAC), 
	      (flow  << (G_TEXTURE_IMAGE_FRAC-1)), /* to slow down the flow */
              (32-1) << G_TEXTURE_IMAGE_FRAC,
              (32-1) << G_TEXTURE_IMAGE_FRAC);
	gSPDisplayList(glistp++, river_dl);

	gDPFullSync(glistp++);
	gSPEndDisplayList(glistp++);

#ifdef DEBUG
#ifndef __MWERKS__
	assert((glistp-dynamicp->glist) < GLIST_LEN);
#endif
#endif

	/* 
	 * Build graphics task:
	 *
	 */
	tlistp->t.ucode_boot = (u64 *) rspbootTextStart;
	tlistp->t.ucode_boot_size = (u32)rspbootTextEnd - (u32)rspbootTextStart;

	/*
	 * choose which ucode to run:
	 */
	if (rdp_flag) {
	    /* RSP output over FIFO to RDP: */
	    tlistp->t.ucode = (u64 *) gspF3DEX2_fifoTextStart;
	    tlistp->t.ucode_data = (u64 *) gspF3DEX2_fifoDataStart; 
	} else {
	    /* RSP output over XBUS to RDP: */
	    tlistp->t.ucode = (u64 *) gspF3DEX2_xbusTextStart;
	    tlistp->t.ucode_data = (u64 *) gspF3DEX2_xbusDataStart;
	}
	
	/* initial display list: */
	tlistp->t.data_ptr = (u64 *) dynamicp->glist;
	tlistp->t.data_size = (u32)((glistp - dynamicp->glist) * sizeof(Gfx));

	/*
	 * Write back dirty cache lines that need to be read by the RCP.
	 */
	osWritebackDCache(&dynamic, sizeof(dynamic));
	
	/*
	 * start up the RSP task
	 */
	osSpTaskStart(tlistp);
	
	/* wait for RDP completion */
	(void)osRecvMesg(&rdpMessageQ, NULL, OS_MESG_BLOCK);

	/* setup to swap buffers */
	osViSwapBuffer(cfb[draw_buffer]);

	/* Make sure there isn't an old retrace in queue 
	 * (assumes queue has a depth of 1) 
	 */
	if (MQ_IS_FULL(&retraceMessageQ))
	    (void)osRecvMesg(&retraceMessageQ, NULL, OS_MESG_BLOCK);
	
	/* Wait for Vertical retrace to finish swap buffers */
	(void)osRecvMesg(&retraceMessageQ, NULL, OS_MESG_BLOCK);
	draw_buffer ^= 1;

    }
}
