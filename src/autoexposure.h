/*!***************************************************************************
*! FILE NAME  : autoexposure.h
*! DESCRIPTION: 
*! Copyright (C) 2008 Elphel, Inc.
*! -----------------------------------------------------------------------------**
*!  This program is free software: you can redistribute it and/or modify
*!  it under the terms of the GNU General Public License as published by
*!  the Free Software Foundation, either version 3 of the License, or
*!  (at your option) any later version.
*!
*!  This program is distributed in the hope that it will be useful,
*!  but WITHOUT ANY WARRANTY; without even the implied warranty of
*!  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*!  GNU General Public License for more details.
*!
*!  You should have received a copy of the GNU General Public License
*!  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*! -----------------------------------------------------------------------------**
*!
*!  $Log: autoexposure.h,v $
*!  Revision 1.2  2009/02/25 17:47:51  spectr_rain
*!  removed deprecated dependency
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*/ 
#ifndef __H_AUTOEXPOSURE__
#define __H_AUTOEXPOSURE__

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
//#include <ctype.h>
//#include <getopt.h>
#include <time.h>
#include <string.h>
#include <syslog.h>

#include <netinet/in.h>
#include <sys/mman.h>      /* mmap */
#include <sys/ioctl.h>

#include <elphel/c313a.h>
#include <elphel/x393_devices.h>
//#include <elphel/exifa.h>
#include <asm/byteorder.h>

#include "aexp_utils.h"
#include "aexp_corr.h"
#include "white_balance.h"
#include "hdr_control.h"
#include "globalsinit.h"

//#if ELPHEL_DEBUG
 #define THIS_DEBUG 1
//#else
// #define THIS_DEBUG 0
//#endif
//#define ELP_FERR(x) printf(stderr,"%s:%d:%s: ERROR ",__FILE__,__LINE__,__FUNCTION__); x
#if THIS_DEBUG
  #define MDF0(x) { if (autoexposure_debug & (1 << 0)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF1(x) { if (autoexposure_debug & (1 << 1)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF2(x) { if (autoexposure_debug & (1 << 2)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//  #define MDF2(x)
  #define MDF3(x) { if (autoexposure_debug & (1 << 3)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//  #define MDF3(x)
  #define MDF4(x) { if (autoexposure_debug & (1 << 4)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF5(x) { if (autoexposure_debug & (1 << 5)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF6(x) { if (autoexposure_debug & (1 << 6)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF7(x) { if (autoexposure_debug & (1 << 7)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//moved from MDF3 - all what is active when autoexposure is off
  #define MDF8(x) { if (autoexposure_debug & (1 << 8)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//moved from MDF3 - all what is active when white balance is off
  #define MDF9(x) { if (autoexposure_debug & (1 << 9)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
#else
  #define MDF0(x)
  #define MDF1(x)
  #define MDF2(x)
  #define MDF3(x)
  #define MDF4(x)
  #define MDF5(x)
  #define MDF6(x)
  #define MDF7(x)
#endif

#define START_SKIP_FRAMES 8 // don't try autoexposure until this number of frames after startup
#define get_imageParamsThis(x) ( framePars[this_frame & PARS_FRAMES_MASK].pars[x])
#define min(x,y) ((x<y)?x:y)

//#define get_imageParamsThis(x) ( framePars[this_frame & PARS_FRAMES_MASK].pars[x])
#define RECALIBRATE_AHEAD           3 // Darken frame 3 ahead of current to measure histograms
#define EXPOS_AHEAD                 3 // minimal frames ahead of the current to set VEXPOS
#define DEFAULT_AE_PERIOD_CHANGE    4
#define DEFAULT_AE_PERIOD_NOCHANGE  1
#define DEFAULT_WB_PERIOD_CHANGE   16
#define DEFAULT_WB_PERIOD_NOCHANGE  4
#define RECALIBRATE_AFTER           2 // number of dim frames
#define DEFAULT_BLACK_CALIB     0xa00
#define HDR_PATTERN1_8          0xaa
#define HDR_PATTERN2_8          0x66
#define HDR_PATTERN1  (HDR_PATTERN1_8 | (HDR_PATTERN1_8 << 8) | (HDR_PATTERN1_8 << 16) | (HDR_PATTERN1_8 << 24))
#define HDR_PATTERN2  (HDR_PATTERN2_8 | (HDR_PATTERN2_8 << 8) | (HDR_PATTERN2_8 << 16) | (HDR_PATTERN2_8 << 24))
#define DEFAULT_WB_WHITELEV   0xfae1 // 98%
#define DEFAULT_WB_WHITEFRAC  0x028f // 1%
#define PERCENTILE_SHIFT 16
#define PERCENTILE_1 (1 << PERCENTILE_SHIFT)

#endif
