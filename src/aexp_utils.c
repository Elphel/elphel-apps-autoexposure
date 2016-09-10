/**
 * \file  aexp_utils.c
 * \brief Daemon to adjust camera exposure and white balance - utility
 *         functions 
 * \date 2008
 */
/*!***************************************************************************
*! FILE NAME  : aexp_utils.c
*! DESCRIPTION: Daemon to adjust camera exposure and white balance
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
*!  $Log: aexp_utils.c,v $
*!  Revision 1.4  2008/12/01 02:32:48  elphel
*!  Updated white balance to use new gains
*!
*!  Revision 1.3  2008/11/30 05:04:45  elphel
*!  added Doxygen file data
*!
*!  Revision 1.2  2008/11/28 08:17:09  elphel
*!  keeping Doxygen a little happier
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.2  2008/11/18 19:30:16  elphel
*!  Added initialization of the "next" frame - otherwise it wait _very_ long if the camera frame number is reset
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/ 

#include "autoexposure.h"

int poorLog(int x) {
/*!
   0->   0
   32-> 512
   64-> 768
 128 ->1024
 256 ->1280
0x10000000 -> 0x1900
0x80000000 -> 0x1c00
*/
  if (x<0) return 0;
  int y=5;
  if ((x & ~0x1ff) ==0  ) { /// 0..511 gets here
    while (((x & 0x100) ==0) && (y>1)) {
      y--;
      x <<=1;
    }
    if (y>1) x+=(y<<8);
    return x; // 0..511
  }
  while (x & ~0x1ff) { /// first time always
    x >>= 1;
    y++;
  }
  return x+(y<<8);
}

int poorExp(int x) {
  int  y=x & 0xff;
  x >>= 8;
  if      (x <  1) return y >> 4;
  else if (x <= 5) return y >> (5-x);
  else             return y << (x-5);
}

int   waitRequstPrevHist(unsigned long next_frame) {
  unsigned long write_data[4]; 
  this_frame=GLOBALPARS_SNGL(G_THIS_FRAME);
  MDF2(fprintf(stderr,"next_frame=0x%08lx, this_frame=0x%08lx\n",next_frame,this_frame)); ///======= 0 here

  if (next_frame <= this_frame) return 0; /// too_late
  else if (next_frame > (this_frame+5) ) { /// wait heer as it is too early to schedule histograms
    lseek(fd_fparmsall, next_frame-5+LSEEK_FRAME_WAIT_ABS, SEEK_END);
    this_frame=GLOBALPARS_SNGL(G_THIS_FRAME);
  }
/// schedule all histograms for next_frame-1
  write_data[0]= FRAMEPARS_SETFRAME;
  write_data[1]= next_frame-1;
  write_data[2]= P_HISTRQ_YC; /// will request once all the histogtram for that frame
  write_data[3]= 3; /// both
  int rslt=write(fd_fparmsall, write_data, sizeof(write_data));
  if (rslt < sizeof(write_data)) return -errno;
/// now wait for that next frame (if needed)
  if (next_frame > this_frame ) {
    lseek(fd_fparmsall, next_frame+LSEEK_FRAME_WAIT_ABS, SEEK_END);
    this_frame=GLOBALPARS_SNGL(G_THIS_FRAME);
    return 1;
  }
  return 0;
}

/**
 * @brief calculate pixel value corresponding so that specified fraction of all pixels have value less or equal than it
 * both result and input fraction are 16.16 format, so 0x10000 is 1.0
 * @param frame    - absolute frame number for which percentile is requested
 * @param color    - color number (0 - R, 1 - G1(G), 2 - G2(GB), 3 - B). G1 is also used for autoexposure
 * @param fraction - fraction of all pixels that should have value less or equal result 16.16 format - 0x10000 == 1.0 == all pixels
 * @param request_colors - if 0 - use the same histogram index as already calculated, otherwise it is a bit mask of colors to request,
 *                         i.e. (1<<color) - request only the specified color, 0xf - request all colors
 * @return <0 for error, or interpalated pixel "input" value in 0..0x10000 scale
 */
long getPercentile(unsigned long frame,int color, unsigned long fraction, int request_colors) {
   unsigned long * hist_cumul;      /// 256 of cumulated histogram values (in pixels)
   unsigned char * hist_percentile; /// 256 of rounded percentiles (1 byte) - used as a starting point for linear interpolation
   int total_pixels;
   int perc;
   int perc_frac=0;
   int frac_pixels; /// number of pixels corresponding to a specified fraction (rounded down)
   int delta;
   MDF2(fprintf(stderr,"percentile for frame=0x%lx, color=%x, fraction=0x%lx, request_colors=0x%x\n",frame, color, fraction, request_colors));
/// Get the histogram (including percentiles)
   if (request_colors) { /// at minimum for the new frame request_colors= 1<<color
     if (request_colors & ~ (1<<COLOR_Y_NUMBER)) lseek(fd_histogram_cache, LSEEK_HIST_WAIT_C, SEEK_END); /// wait for all histograms, not just Y (G1)
     else                                        lseek(fd_histogram_cache, LSEEK_HIST_WAIT_Y, SEEK_END); /// wait for just Y (G1)
    MDF2(fprintf(stderr,"this_frame: 0x%lx,  NOW: 0x%lx\n",this_frame, GLOBALPARS_SNGL(G_THIS_FRAME)));
     lseek(fd_histogram_cache, LSEEK_HIST_NEEDED + (request_colors << 8), SEEK_END);      /// specify that reverse histogram(s) are needed
     hist_index=lseek(fd_histogram_cache, frame, SEEK_SET);                             /// request histograms for the specified frame
    MDF2(fprintf(stderr,"got histogram for frame: 0x%lx,  NOW: 0x%lx\n",histogram_cache[hist_index].frame, GLOBALPARS_SNGL(G_THIS_FRAME)));
/// histograms for frame will be available 1 frame later
     if(hist_index <0) {
       ELP_FERR(fprintf(stderr, "Requested histograms for frame %ld (0x%lx) are not available. this_frame=0x%lx, now=0x%lx\n",frame,frame,this_frame,GLOBALPARS_SNGL(G_THIS_FRAME) ));
       return -1;
     }
   }
   hist_cumul=     &(histogram_cache[hist_index].cumul_hist[color<<8]);
   hist_percentile=&(histogram_cache[hist_index].percentile[color<<8]);
   if (fraction > (PERCENTILE_1-1)) fraction = PERCENTILE_1-1;
   total_pixels=   hist_cumul[255];
   frac_pixels=(((long long) total_pixels) * fraction) >> PERCENTILE_SHIFT;
   perc=hist_percentile[fraction >> (PERCENTILE_SHIFT-8)]; /// 8 MSBs used as index
   MDF2(fprintf(stderr,"total_pixels=0x%x, frac_pixels=0x%x, perc=0x%x\n",total_pixels, frac_pixels, perc));

   if (perc>0) perc--;                                              /// seems hist_percentile[perc] rounds up, not down
   while ((perc>0) &&  (hist_cumul[perc] > frac_pixels)) perc--;    /// adjust down  (is that needed at all?)
   perc++;
   while ((perc<255) &&  (hist_cumul[perc] <= frac_pixels)) perc++; /// adjust up  (is that needed at all?)
   perc--;
   delta=hist_cumul[perc+1] - hist_cumul[perc];
   if (delta) {
     perc_frac=((frac_pixels-hist_cumul[perc]) << (PERCENTILE_SHIFT-8))/delta;
   }
   perc_frac += perc << (PERCENTILE_SHIFT-8);
   MDF2(fprintf(stderr,"hist_cumul[0x%x]=0x%lx hist_cumul[0x%x]=0x%lx, delta=0x%x, perc_frac=0x%x\n",perc,hist_cumul[perc],perc+1,hist_cumul[perc+1],delta,perc_frac));

/// here perc_frac is the intermediate result, so that (1<<PERCENTILE_SHIFT) corresponds to full scale of after-gamma data
/// Now we need to "un-gamma" it to reference to the (supposingly linear) input range that is proportional to exposure
   if (setGammaIndex(color)) /// sets global gamma_index, uses global hist_index;
     return gammaReverse (perc_frac);
   return -1; /// error
}

/**
 * @brief Sets global variable gamma_index (needed to "ungamma" fractions) using current global his_index
 * @param color - color number for which gamma_index is needed
 * @return gamma_index>0. 0 if not availble
 */
int setGammaIndex(int color) {
   unsigned long  write_data[2];
   unsigned long hash32= histogram_cache[hist_index].gtab[color&3];
//   MDF2(fprintf(stderr,"frame=0x%lx hist.frame=0x%lx  hist_index=%d hash32=0x%lx\n",frame, histogram_cache[hist_index].frame, hist_index, hash32));
//   MDF7(fprintf(stderr,"frame=0x%lx hist.frame=0x%lx  hist_index=%d hash32=0x%lx\n",frame, histogram_cache[hist_index].frame, hist_index, hash32));
   write_data[0]=hash32;
   write_data[1]=GAMMA_MODE_NEED_REVERSE;
   int rslt=write(fd_gamma_cache, write_data, 6);
   if (rslt<= 0) {
       ELP_FERR(fprintf(stderr,"Request for gamma returned %d, hash32=0x%lx (gamma=%f, black=%d, scale=%f)\n",rslt,hash32, 0.01*((hash32>>16) & 0xff),(int)(hash32>>24), (hash32 & 0xffff)/1024.0));
       return 0;
   }
   gamma_index=lseek(fd_gamma_cache, 0, SEEK_CUR);
   MDF2(fprintf(stderr,"gamma_index=0x%x, rslt=0x%x\n",gamma_index,rslt));
  if (gamma_index <= 0) {
       ELP_FERR(fprintf(stderr,"request for gamma table color=%d  failed\n",color));
       return 0;
   }
   MDF2(fprintf(stderr,"gamma_index=0x%x\n",gamma_index));
  return gamma_index;
}


/**
 * @brief calculates direct gamma conversion using gamma tables indexed by global gamma_index
 * @param x - argument 0..0xffff (proportional to sensor data)
 * @return converted result, in the range 0..0xffff (proportional to 8-bit internal data to be compressed)
 */
unsigned long gammaDirect (unsigned long x) {
   if (x>0xffff) x=0xffff;
   unsigned long y= gamma_cache[gamma_index].direct[x >> 8]; /// truncating , no interpolation
   y+= ((gamma_cache[gamma_index].direct[(x >> 8)+1]-y)*(x & 0xff))>>8;
   if (y > 0xffff) y=0xffff;
   return y;
}

/**
 * @brief calculates reverse gamma conversion using gamma tables indexed by global gamma_index
 * @param x - argument 0..0xffff (proportional to 8-bit internal data to be compressed)
 * @return converted result, in the range 0..0xffff (proportional to sensor data)
 */
unsigned long gammaReverse (unsigned long x) {
   unsigned short * gamma_direct= gamma_cache[gamma_index].direct;  /// [257]  "Gamma" table, 16-bit for both non-scaled prototypes and scaled, 0..0xffff range (hardware will use less)
   unsigned char *  gamma_reverse=gamma_cache[gamma_index].reverse; /// [256] reverse table to speed-up reversing (still need interpolation).Index - most significant 8 bits, data - largest direct
   if (x>0xffff) x=0xffff;
   int y_frac=0;
   int y=gamma_reverse[x >> 8]; /// 8 MSBs used as index
   if (y>0) y--;                                   /// seems gamma_reverse[] rounds up, not down
   while ((y > 0) &&  (gamma_direct[y] >  x)) y--; /// adjust down (is that needed at all?)
   y++;
   while ((y<255) &&  (gamma_direct[y] <= x)) y++; /// adjust up (is that needed at all?)
   y--;
   int delta=gamma_direct[y+1] - gamma_direct[y];
   if (delta)  y_frac=((x - gamma_direct[y]) << 8)/delta;

   MDF2(fprintf(stderr,"y_frac=0x%x\n",y_frac));
   y_frac += y << 8;
   MDF2(fprintf(stderr,"gamma_direct[0x%x]=0x%x gamma_direct[0x%x]=0x%x, delta=0x%x, y_frac=0x%x\n",y, (int) gamma_direct[y],y+1,(int) gamma_direct[y+1],delta,y_frac));
/// limit just in case?
   if      (y_frac <      0) y_frac=0;
   else if (y_frac > 0xffff) y_frac=0xffff;
   return y_frac;
}



/**
 * @brief return value of parameter 'index' from frame 'frame' - use pastPars if too late for framePars
 * @param indx parameter indx
 * @param frame absolute frame number
 * @return parameter value (error will be 0xffffffff, but that could be a legitimate value too)
 */
unsigned long get_imageParamsThat     (int indx, unsigned long frame) {
   int frame_index=  frame & PARS_FRAMES_MASK;
   int past_index=   frame & PASTPARS_SAVE_ENTRIES_MASK;
   unsigned long value;
/// Locate frame info in framePars
   if (framePars[frame_index].pars[P_FRAME] != frame) {
///   too late, try pastPars
     if ((indx < PARS_SAVE_FROM) || (indx >= (PARS_SAVE_FROM+PARS_SAVE_NUM))) return 0xffffffff ; /// not saved
     value=pastPars[past_index].past_pars[indx - PARS_SAVE_FROM]; /// should be retrieved before checking frame (interrupts)
     if (pastPars[past_index].past_pars[P_FRAME-PARS_SAVE_FROM] != frame) { /// too late even for pastPars? Or a bug?
       ELP_FERR(fprintf (stderr,"Can not find frame 0x%x data neither in framePars[0x%x].pars[0x%x]=0x%x, nor in pastPars[0x%x].past_pars[0x%x]=0x%x\n",\
                     (int) frame, frame_index, (int) P_FRAME, (int) framePars[frame_index].pars[P_FRAME],\
                                  past_index,  (int) (P_FRAME-PARS_SAVE_FROM), (int) pastPars[past_index].past_pars[P_FRAME-PARS_SAVE_FROM]));
       return 0xffffffff;
     }
   } else {
     value=framePars[frame_index].pars[indx];
   }
   return value;
}

/**
 * @brief return value of parameter 'indx' from frame 'frame' - use pastPars if too late for framePars (31 bits only)
 * @param indx parameter index
 * @param frame absolute frame number
 * @return <0 - error, otherwise parameter lower 31 bits of the parameter value
 */
int           get_imageParamsThatValid(int indx, unsigned long frame) {
   int frame_index=  frame & PARS_FRAMES_MASK;
   int past_index=   frame & PASTPARS_SAVE_ENTRIES_MASK;
   int value;
/// Locate frame info in framePars
   if (framePars[frame_index].pars[P_FRAME] != frame) {
///   too late, try pastPars
     if ((indx < PARS_SAVE_FROM) || (indx >= (PARS_SAVE_FROM+PARS_SAVE_NUM))) return -2 ; /// not saved
     value=pastPars[past_index].past_pars[indx - PARS_SAVE_FROM] & 0x7fffffff;
     if (pastPars[past_index].past_pars[P_FRAME-PARS_SAVE_FROM] != frame) { /// too late even for pastPars? Or a bug?
       ELP_FERR(fprintf (stderr,"Can not find frame 0x%x data neither in framePars[0x%x].pars[0x%x]=0x%x, nor in pastPars[0x%x].past_pars[0x%x]=0x%x\n",\
                     (int) frame, frame_index, (int) P_FRAME, (int) framePars[frame_index].pars[P_FRAME],\
                                  past_index,  (int) (P_FRAME-PARS_SAVE_FROM), (int) pastPars[past_index].past_pars[P_FRAME-PARS_SAVE_FROM]));
       return -1;
     }
   } else {
     value=framePars[frame_index].pars[indx] & 0x7fffffff;
   }
   return value ;
}

/**
 * @brief Measure the percentile input (sensor) level for the specified by [P_AEXP_FRACPIX] fraction of all pixels
 * for VEXPOS = 1 line, use that as a zero level for exposure calculations
 * @return 0 - OK
 */
int recalibrateDim(void) {
  unsigned long vexpos_was, fraction, dims;
  unsigned long write_data[4];
  int rslt;
  this_frame=GLOBALPARS_SNGL(G_THIS_FRAME);
  MDF1(fprintf(stderr,"this_frame: 0x%lx\n",this_frame));
  unsigned long target_frame=this_frame+RECALIBRATE_AHEAD;
  vexpos_was= framePars[target_frame & PARS_FRAMES_MASK].pars[P_VEXPOS];
  fraction=   framePars[target_frame & PARS_FRAMES_MASK].pars[P_AEXP_FRACPIX];
  MDF1(fprintf(stderr,"this_frame: 0x%lx target_frame: 0x%lx\n",this_frame,target_frame));
  write_data[0]=FRAMEPARS_SETFRAME;
  write_data[1]= target_frame;
  write_data[2]= P_VEXPOS; // | FRAMEPAIR_FORCE_NEWPROC ?
  write_data[3]= 1;
  rslt=write(fd_fparmsall, write_data, sizeof(write_data));
  if (rslt < sizeof(write_data)) return -errno;
  write_data[1]= target_frame+RECALIBRATE_AFTER; /// or is +1 enough? 2 seems to be required for Micron free running mode, for async 1 is OK
  write_data[3]= vexpos_was;
  rslt=write(fd_fparmsall, write_data, sizeof(write_data));
  if (rslt < sizeof(write_data)) return -errno;
/// no error check here
  dims= getPercentile(target_frame,0, fraction, 0xf) & 0xffff; /// all colors are needed. Will skip frames
  dims|=getPercentile(target_frame,1, fraction, 0xf) << 16; 
  GLOBALPARS_SNGL(G_HIST_DIM_01)=dims;
  dims= getPercentile(target_frame,2, fraction, 0xf) & 0xffff; /// all colors are needed. Will skip frames
  dims|=getPercentile(target_frame,3, fraction, 0xf) << 16; 
  GLOBALPARS_SNGL(G_HIST_DIM_23)=dims;
  MDF1(fprintf(stderr,"dims: 0x%lx NOW: 0x%lx\n",dims, GLOBALPARS_SNGL(G_THIS_FRAME)));
  lseek(fd_histogram_cache, target_frame+RECALIBRATE_AFTER, SEEK_SET); ///
  return 0;
}



