/*!***************************************************************************
*! FILE NAME  : hdr_control.c
*! DESCRIPTION: HDR part of the autoexposure/white balance/hdr daemon
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
*!  $Log: hdr_control.c,v $
*!  Revision 1.4  2011/05/20 21:42:04  elphel
*!  different debug levels
*!
*!  Revision 1.3  2010/12/16 16:52:15  elphel
*!  working on HDR autoexposure
*!
*!  Revision 1.2  2010/12/15 16:46:44  elphel
*!  autoexposure correction: hdr synchronization
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/ 

#include "autoexposure.h"
//#define P_HDR_DUR        168 // 0 - HDR 0ff, >1 - duration of same exposure (currently 1 or 2 - for free running)
//#define P_HDR_VEXPOS     169 // if less than 0x10000 - number of lines of exposure, >=10000 - relative to "normal" exposure
int exposureHDR (int frame, int target_frame) {
   unsigned long write_data[4]; 
   int target_frame8=target_frame & PARS_FRAMES_MASK;
   int HDR_mode;
   int vexpos, vexpos_HDR;
   int rslt;
   if (!((HDR_mode=framePars[target_frame8].pars[P_HDR_DUR])))  return 0; /// HDR mode is disabled
   MDF5(fprintf(stderr,"frame=0x%x target_frame=0x%x GLOBALPARS_SNGL(G_THIS_FRAME)=0x%x\n",(int) frame, (int) target_frame, (int)GLOBALPARS_SNGL(G_THIS_FRAME)));

   if ((HDR_mode<1) || (HDR_mode>2)) {
     ELP_FERR(fprintf(stderr, "Wrong HDR mode %d (only 1 or 2 are supported)\n",HDR_mode));
     return -1;
   }
 /// maybe expos (not vexpos!) was just written by autoexposure, not yet recalculated from expos to vexpos

   switch (HDR_mode) {
     case 1:
       lseek(fd_fparmsall, frame+1 +LSEEK_FRAME_WAIT_ABS, SEEK_END);
       break;
     case 2:
       lseek(fd_fparmsall, frame+2 +LSEEK_FRAME_WAIT_ABS, SEEK_END);
       break;
   }

   vexpos=    framePars[target_frame8].pars[P_VEXPOS];
   vexpos_HDR=framePars[target_frame8].pars[P_HDR_VEXPOS];
   if (vexpos_HDR>=0x10000) vexpos_HDR= (((long long) vexpos_HDR) * vexpos) >> 16;
   write_data[0]= FRAMEPARS_SETFRAME;
   write_data[2]= P_VEXPOS; // | FRAMEPAIR_FORCE_NEWPROC ?
/// program next two frames after target_frame
   MDF1(fprintf(stderr,">>>>>HDR_mode=%d, write: %08lx  %08lx %08lx %08lx\n",HDR_mode,write_data[0],write_data[1],write_data[2],write_data[3]));

   switch (HDR_mode) {
     case 1:
       write_data[1]= target_frame+1;
       write_data[3]= vexpos_HDR;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
      MDF1(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
       if (rslt < sizeof(write_data)) return -errno;
       write_data[1]= target_frame+2;
       write_data[3]= vexpos;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
       if (rslt < sizeof(write_data)) return -errno;
      MDF5(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
     break;
     case 2:
       write_data[1]= target_frame+2;
       write_data[3]= vexpos_HDR;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
       if (rslt < sizeof(write_data)) return -errno;
      MDF1(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
     break;
   }
/// skip 2 frames from 'frame' (use absolute to add tolerance against being too late)
   lseek(fd_fparmsall, frame + 2+LSEEK_FRAME_WAIT_ABS, SEEK_END);
   switch (HDR_mode) {
     case 1:
       write_data[1]= target_frame+3;
       write_data[3]= vexpos_HDR;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
       if (rslt < sizeof(write_data)) return -errno;
      MDF1(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
       write_data[1]= target_frame+4;
       write_data[3]= vexpos;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
       if (rslt < sizeof(write_data)) return -errno;
      MDF1(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
     break;
     case 2:
       write_data[1]= target_frame+4;
       write_data[3]= vexpos;
       rslt=write(fd_fparmsall, write_data, sizeof(write_data));
       if (rslt < sizeof(write_data)) return -errno;
      MDF1(fprintf(stderr,"write: %08lx  %08lx %08lx %08lx -> %d\n",write_data[0],write_data[1],write_data[2],write_data[3],rslt));
     break;
   }
   return 1;
}



/**
 * @brief in HDR mode - wait until we get a histogram from a normal frame (only needed to re-sync after long delays
 * uses global variables for files and mmap-ed data so they are accessible everywhere
 * @return 0 - OK, <0 - problems opening/mma-ing
 * BUG: Does not seem to work at all !
 */
int skipHDR(int mode, unsigned long frame) {
  int patt;
  int skip;
  MDF5(fprintf(stderr,"mode=%d frame=0x%x GLOBALPARS_SNGL(G_THIS_FRAME)=0x%x\n",(int) mode, (int) frame, (int)GLOBALPARS_SNGL(G_THIS_FRAME)));

  switch (mode) {
   case 0: return 0;
   case 1: patt=HDR_PATTERN1;
           break;
   case 2: patt=HDR_PATTERN2;
           break;
   default: return -1;
  }
  patt >>= (frame & PARS_FRAMES_MASK);
  if (patt & 1) return 0;
  skip=0;
  while (patt && !(patt & 1)) { 
    patt >>= 1;
    skip++;
  }
  lseek(fd_histogram_cache, LSEEK_HIST_WAIT_Y,     SEEK_END); /// wait for Y only
  lseek(fd_histogram_cache, LSEEK_HIST_NEEDED + 0, SEEK_END); /// nothing needed, just waiting for some frames to be skipped
//  lseek(fd_histogram_cache, skip,                  SEEK_CUR); /// just wait until the Y histogram will available for non-HDR frame
//  lseek(fd_histogram_cache, skip-1,                SEEK_CUR); /// just wait until the Y histogram will available for non-HDR frame
  lseek(fd_histogram_cache, skip,                SEEK_CUR); /// just wait until the Y histogram will available for non-HDR frame
  MDF5(fprintf(stderr,"mode=%d frame=0x%x skip=%d GLOBALPARS_SNGL(G_THIS_FRAME)=0x%x\n",(int) mode, (int) frame, (int) skip, (int)GLOBALPARS_SNGL(G_THIS_FRAME)));
  return 1;
}
