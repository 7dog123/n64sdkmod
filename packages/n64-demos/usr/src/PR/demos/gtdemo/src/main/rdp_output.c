
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

/*
 * The "output_buff" field of the RCP task structure is set to this address.
 * It is placed in its own .c, and thus its own .o, since the linker aligns
 * individual relocatables to data cache line size (16 byte) boundaries.
 *
 * This avoids the problem where the output buffer data is accidentally
 * scribbled over during a writeback for data sharing the same line.
 */

#include <ultra64.h>
#include "gtdemo.h"

u64 rdp_output[0x400] __attribute__((aligned (16)));	/* RSP writes back RDP data */

