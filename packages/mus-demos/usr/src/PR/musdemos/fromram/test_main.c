
/*************************************************************

  test_main.c : Nintendo 64 Music Tools Library Sample
  (c) Copyright 1998, Software Creations (Holdings) Ltd.

  Version 3.14

  FROMRAM demo main source file. This demo illustrates how to
  use the library with samples based in RAM as opposed to ROM.

**************************************************************/

/* include system files */
#ifndef F3DEX_GBI
#define F3DEX_GBI
#endif

#include <ultra64.h>
#include <sched.h>
#include <libmus.h>

/* include current header file */
#include "test_rom.h"

// prototypes...
void  StartThread (void *ignored);
void  GameThread  (void *ignored);
void  MainLoop    (void);


/* internal macros */
#define BOOTSTACK_SIZE     (STACKSIZE)
#define MAINSTACK_SIZE     (STACKSIZE)
#define DMA_QUEUE_SIZE     200
#define GEOM_ALL           G_SHADE|G_SHADING_SMOOTH|G_CULL_BOTH|G_FOG|G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_LOD
#define LOAD_PROJ          (G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH)
#define LOAD_MODEL         (G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH)
#define BGCOL              0


/* typedef - message */
typedef union 
{  
   short    type;  
   OSScMsg  app;    
} message_t;

/* typedef - graphics information */
typedef struct 
{
    OSScTask         task;
    message_t        mess;  
    unsigned short   *screen;
} info_t;

/* internal workspace */
OSThread       gameThread;
OSThread       mainThread;
u64            bootStack       [BOOTSTACK_SIZE/8];
u64            mainThreadStack [MAINSTACK_SIZE/8];
u64            scheduleStack   [OS_SC_STACKSIZE/8];
OSMesgQueue    pi_queue;
OSMesg         pi_messages	[DMA_QUEUE_SIZE];
OSMesgQueue    gfx_queue;
OSMesg         gfx_messages	[MAX_MESGS];
OSMesgQueue    dma_queue;
OSMesg         dma_messages	[MAX_MESGS];
info_t         info[2];
OSSched        sc;

/* entry point */
void boot(void *ignored)
{
   osInitialize();    
   osCreateThread(&mainThread, 1, StartThread, NULL, (void *)(mainThreadStack+MAINSTACK_SIZE/8), (OSPri)MAIN_PRIORITY);    
   osStartThread(&mainThread);
}

/* start up thread */
void StartThread(void *ignored) 
{
   /* create PI manager for ROM access */
   osCreatePiManager((OSPri) OS_PRIORITY_PIMGR, &pi_queue, pi_messages, DMA_QUEUE_SIZE);   
   /* create main thread and start it */
   osCreateThread(&gameThread, 6, GameThread, NULL, bootStack+BOOTSTACK_SIZE/8, (OSPri)GAME_PRIORITY);    
   osStartThread(&gameThread);   
   /* become the idle thread */
   osSetThreadPri(0, 0);	
   for(;;);
}

/* main thread */
void GameThread(void *ignored)
{
   OSScClient	client;
   int mode;

   /* create DMA message queue */
   osCreateMesgQueue(&dma_queue, dma_messages, MAX_MESGS);

   /* initialize default scheduler using "osTvType" */
   if (osTvType==0)
      mode = OS_VI_PAL_LAN1;      /* --- PROGRAMMING CAUTION --- */
   else if (osTvType==1)          /* Applications must not use "ovTvType" to switch the TV   */
      mode = OS_VI_NTSC_LAN1;     /* system from NTSC to PAL or PAL to NTSC. A Game Pak must */
   else                           /* not be compatible with both NTSC and PAL formats.       */
      mode = OS_VI_MPAL_LAN1;     
   osCreateScheduler(&sc, (void *)(scheduleStack+OS_SC_STACKSIZE/8), SCHEDULER_PRIORITY, mode, 1);

   /* initialise double buffered info */
   info[0].mess.type = OS_SC_DONE_MSG;
   info[0].screen    = cfb_A;
   info[1].mess.type = OS_SC_DONE_MSG;
   info[1].screen    = cfb_B;
    
   osCreateMesgQueue(&gfx_queue, gfx_messages, MAX_MESGS);
   osScAddClient(&sc, &client, &gfx_queue);
   
   /* call main loop (never returns from this) */
   MainLoop();
}


// workspace for MainLoop...

u64         glist[2][100];
Vp          vp0 = 
{
        (SCREEN_XSIZE/2)*4, (SCREEN_YSIZE/2)*4, G_MAXZ/2, 0,  /* scale */
        (SCREEN_XSIZE/2)*4, (SCREEN_YSIZE/2)*4, G_MAXZ/2, 0,  /* translate */
};
Vtx         tri[]=
{
  { -64, 64,-5,0,0,0,0,0xff,0,0xff},
  {  64, 64,-5,0,0,0,0,0,0,0xff},
  {  64,-64,-5,0,0,0,0xff,0,0,0xff},
};

//==========================================================================
//
// Main Loop
//
//==========================================================================


void MainLoop(void)
{
   u64 *glistp, *glist_start;
   int i, frame, drawing_flag;
   float angle;
   u32 work;
   Mtx projection_2d, mtxwork, model_2d[2];
   OSScTask    *task;
   message_t   *end_msg;
 
   frame = drawing_flag = 0;
   angle = 0.0;

   /* initialise for 2D */
   guOrtho(&projection_2d, 0.0, 320.0, 240.0, 0.0, -200.0, 200.0, 1.0);

   /* initialise music player */
   InitMusicDriver();
   /* start song */
   MusStartSong(tune_buf);

   /* loop for ever */
   while (1)
   {
      frame ^= 1;
      /* rotating triangle */
      guTranslate(&model_2d[frame], 160.0, 120.0, 0.0);
      guRotate(&mtxwork, angle, 0.0F,0.0F,1.0F);    
      guMtxCatL(&mtxwork, &model_2d[frame], &model_2d[frame]);
      angle += 1.0;
      if (angle>=360.0)
        angle-=360.0; 
      glist_start = glistp = &glist[frame][0];

      /* special video tackle */
      osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON);
      osViSetSpecialFeatures(OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_OFF);
      /* setup segments */
      gSPSegment(glistp++, 0, 0);
      gSPSegment(glistp++,  CFB_SEG, OS_K0_TO_PHYSICAL(cfb_A) );
      /* set defaults */
      gSPViewport(glistp++, &vp0);
      gSPClearGeometryMode (glistp++,GEOM_ALL);
      gSPTexture           (glistp++,0, 0, 0, 0, G_OFF);
      gSPSetGeometryMode(glistp++,G_SHADE | G_SHADING_SMOOTH);

      gDPSetCycleType      (glistp++,G_CYC_1CYCLE);
      gDPPipelineMode      (glistp++,G_PM_1PRIMITIVE);
      gDPSetScissor        (glistp++,G_SC_NON_INTERLACE, 0, 0, 320, 240);
      gDPSetTextureLOD     (glistp++,G_TL_TILE);
      gDPSetTextureLUT     (glistp++,G_TT_NONE);
      gDPSetTextureDetail  (glistp++,G_TD_CLAMP);
      gDPSetTexturePersp   (glistp++,G_TP_PERSP);
      gDPSetTextureFilter  (glistp++,G_TF_BILERP);
      gDPSetTextureConvert (glistp++,G_TC_FILT);
      gDPSetCombineMode    (glistp++,G_CC_SHADE, G_CC_SHADE);
      gDPSetCombineKey     (glistp++,G_CK_NONE);
      gDPSetAlphaCompare   (glistp++,G_AC_NONE);
      gDPSetColorDither    (glistp++,G_CD_DISABLE);

      /* set colour buffer address */
      gDPSetColorImage(glistp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, osVirtualToPhysical(frame ? cfb_B : cfb_A));
      gDPPipeSync          (glistp++);

      /* clear screen */
      work = GPACK_RGBA5551(BGCOL, BGCOL, BGCOL,1);
      gDPSetCycleType      (glistp++,G_CYC_FILL);
      gDPSetFillColor(glistp++, (work<<16) | work);
      gDPFillRectangle     (glistp++,0, 0, 320-1, 240-1);
      gDPPipeSync(glistp++);

      /* now setup to draw something */
      gSPMatrix(glistp++, OS_K0_TO_PHYSICAL(&projection_2d), LOAD_PROJ);
      gSPMatrix(glistp++, OS_K0_TO_PHYSICAL(&model_2d[frame]), LOAD_MODEL);
      gDPSetCycleType(glistp++, G_CYC_1CYCLE);
      gDPSetRenderMode(glistp++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
 
      /* draw simple triangle */
      gDPPipeSync(glistp++);      
      gSPVertex(glistp++,  OS_K0_TO_PHYSICAL(&tri[0]),3,0);
      gSP1Triangle(glistp++, 0, 1, 2, 0);
      gDPFullSync(glistp++);
      gSPEndDisplayList(glistp++);

      /* make sure previous draw list is complete */
      if (drawing_flag)
      {
         do
         {
            osRecvMesg(&gfx_queue, (OSMesg *)&end_msg, OS_MESG_BLOCK);
         } while (end_msg->type!=OS_SC_DONE_MSG);
      }
      drawing_flag = 1;

      /* create graphic task */
      task = &info[frame].task;
      task->list.t.data_ptr = (u64 *) glist_start;
      task->list.t.data_size = glistp-glist_start;
      task->list.t.type = M_GFXTASK;
      task->list.t.flags = 0x0;
      task->list.t.ucode_boot = (u64 *)rspbootTextStart;
      task->list.t.ucode_boot_size = ((int) rspbootTextEnd - (int) rspbootTextStart);
      task->list.t.ucode = (u64 *) gspF3DEX_fifoTextStart;
      task->list.t.ucode_data = (u64 *) gspF3DEX_fifoDataStart;
      task->list.t.ucode_size      = 4096;
      task->list.t.ucode_data_size = 2048;
      task->list.t.dram_stack = NULL;
      task->list.t.dram_stack_size = 0;
      task->list.t.output_buff = fifo_buffer;
      task->list.t.output_buff_size = &fifo_buffer[FIFO_DATA_SIZE/sizeof(u64)];
      task->list.t.yield_data_ptr = yield_buffer;
      task->list.t.yield_data_size = OS_YIELD_DATA_SIZE;
      task->next     = 0;
      task->flags = OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP | OS_SC_LAST_TASK | OS_SC_SWAPBUFFER;
      task->msgQ     = &gfx_queue;
      task->msg      = (OSMesg)&info[frame].mess;
      task->framebuffer = (void *)info[frame].screen;

      // start task...
      osWritebackDCacheAll();
      osSendMesg(osScGetCmdQ(&sc), (OSMesg)task, OS_MESG_BLOCK);
    }
}

/* end of file */
