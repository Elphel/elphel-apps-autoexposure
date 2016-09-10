/*!***************************************************************************
*! FILE NAME  : temperature.h
*! DESCRIPTION: 
*! Copyright (C) 2012 Elphel, Inc.
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
*!  $Log: temperature.h,v $
*!  Revision 1.1  2012/04/08 04:10:36  elphel
*!  rev. 8.2.2 - added temperature measuring daemon, related parameters for logging SFE and system temperatures
*!
*/ 
#ifndef __H_TEMPERATURE__
#define __H_TEMPERATURE__

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

//#include <asm/elphel/exifa.h>
#include <asm/byteorder.h>

//#if ELPHEL_DEBUG
 #define THIS_DEBUG 1
//#else
// #define THIS_DEBUG 0
//#endif
//#define ELP_FERR(x) printf(stderr,"%s:%d:%s: ERROR ",__FILE__,__LINE__,__FUNCTION__); x
#if THIS_DEBUG
  #define MDF0(x) { if (temperature_debug & (1 << 0)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF1(x) { if (temperature_debug & (1 << 1)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF2(x) { if (temperature_debug & (1 << 2)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//  #define MDF2(x)
  #define MDF3(x) { if (temperature_debug & (1 << 3)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
//  #define MDF3(x)
  #define MDF4(x) { if (temperature_debug & (1 << 4)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF5(x) { if (temperature_debug & (1 << 5)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF6(x) { if (temperature_debug & (1 << 6)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
  #define MDF7(x) { if (temperature_debug & (1 << 7)) {fprintf(stderr,"%s:%d:%s: ",__FILE__,__LINE__,__FUNCTION__);x;} }
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

#define get_imageParamsThis(x) ( framePars[this_frame & PARS_FRAMES_MASK].pars[x])
#define min(x,y) ((x<y)?x:y)


#endif
