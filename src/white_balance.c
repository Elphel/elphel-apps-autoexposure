/*!***************************************************************************
*! FILE NAME  : white_balance.c
*! DESCRIPTION: White balance part of the autoexposure/white balance/hdr daemon
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
*!  $Log: white_balance.c,v $
*!  Revision 1.6  2008/12/10 02:12:54  elphel
*!  Added threshold with no-correction zone, similar to autoexposure.
*!  Individual, per-color integration: whenintegrated error is less than half threshold - no correction will take place,, from 0.5 to 1.5 threshold - correction will be scaled, above 1.5 - full correction applied.
*!  In the case correction is scaled, only portion of integrated error is removed after correction. If full correction applied - integrated error is reset.
*!
*!  Revision 1.5  2008/12/02 19:12:13  elphel
*!  made it check that there is enough pixels to work with (before it got 0 if daemon started to early on all colors but one were zeroed out)
*!
*!  Revision 1.4  2008/12/02 00:28:13  elphel
*!  multiple bugfixes, making white balance to work and work with camvc
*!
*!  Revision 1.3  2008/12/01 02:32:48  elphel
*!  Updated white balance to use new gains
*!
*!  Revision 1.2  2008/11/30 05:34:12  elphel
*!  Updated for WB_EN/WB_MASK split (still not updated for the new gain control - not operational)
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.5  2008/11/18 19:30:16  elphel
*!  Added initialization of the "next" frame - otherwise it wait _very_ long if the camera frame number is reset
*!
*!  Revision 1.4  2008/11/18 07:36:55  elphel
*!  added TODO
*!
*!  Revision 1.3  2008/11/17 06:49:48  elphel
*!  synchronized greens analog gains when color HDR is not used
*!
*!  Revision 1.2  2008/11/16 17:35:06  elphel
*!  improved control of analog gains
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/ 

#include "autoexposure.h"
void initWhiteBalanceCorr(void) {
  GLOBALPARS_SNGL(G_NEXT_WB_FRAME)=0;
}
#define MIN_LEVEL_TO_ADJUST 0x1000 /// 1/16 of the full scale 
#define MIN_PIXELS_TO_ADJUST 0x100 /// one 32x32 block, 1 color
/**
 * @brief Single white balance correction step (through scaling "gamma" curves). 
 * white balance parameters are used for the target fram, not for the currenly processed
 * @param frame "current" frame, histograms are ready for frame-1
 * @param target_frame frame, to which new exposure should be applied (that frame picture will change  brightness)
 *                     caller should take care of not bumping into HDR frame
 * @return <0 - error, 0 - no correction was made, 1 - correction to [P_VEXPOS] was made
 */
/// TODO: at startup or long pause, set wb after aexp settles to within thershold
/// TODO: SupportT P_WB_MASK - now it is just ON/OFF. NOTE:When the bit is off it should be scaled with the G1 color!

int whiteBalanceCorr(int frame, int target_frame, int ae_rslt) {
   MDF9(fprintf(stderr,"frame=0x%x, target_frame=0x%x G_WB_INTEGERR=0x%08lx\n",frame,target_frame,GLOBALPARS_SNGL(G_WB_INTEGERR))); ///======= 0 here
   int rslt;
   int colors;
   unsigned long write_data[18];
///   int  gamma_index; - global
///   int hist_index; - global
///   int error_thresh;
   int color;
   int wb_whitelev,wb_whitefrac,wb_whitepix, wb_whiteindex,wb_maxwhite;
   int brightest_color=-1; /// brightest one of the enabled colors (including COLOR_Y_NUMBER)
   int max_white_pixels;   /// maximal (among colors) number of "white" (>wb_whitelev) pixels
   int min_white_pixels;   ///to detect overexposure for all colors
   int num_pixels,white_pixels,nonwhite_frac,nonwhite_pixels;
   int perc,delta,perc_frac;
   unsigned long * hist_cumul;      /// 256 of cumulated histogram values (in pixels) for selected color
   unsigned char * hist_percentile; /// 256 of rounded percentiles (1 byte) - used as a starting point for linear interpolation
   int levels[4];      /// 16-bit levels for "white" for different colors (output levels), 
   int corr[4];        /// 16.16 additional manual color correction coefficients
   int gains[4];       /// full gain (analog+digital) per channel
   int new_gains[4];
   int target_frame8=target_frame & PARS_FRAMES_MASK;
   int i;
   int error_thresh= framePars[target_frame8].pars[P_WB_THRESH];/// Threshold for integrated error - when errors are smaller - correction is scaled proportionally
   int wb_period_change=   framePars[target_frame8].pars[P_WB_PERIOD] & 0xff; /// lower byte
   int wb_period_nochange=(framePars[target_frame8].pars[P_WB_PERIOD] >> 8 ) & 0xff; /// next byte
   int wb_dont_sync=      (framePars[target_frame8].pars[P_WB_PERIOD] & 0x10000); /// don't try to synchronize to availble histograms
   int aerr=0; // just to keep compiler happy
//   int * wb_err= (int *) &(GLOBALPARS_SNGL(G_WB_INTEGERR)); /// so it will be signed
   static int wb_err[4]; /// individual per-color. Maybe make visible outside? Or not?

   if (!wb_period_change)   wb_period_change=  DEFAULT_WB_PERIOD_CHANGE;
   if (!wb_period_nochange) wb_period_nochange=DEFAULT_WB_PERIOD_NOCHANGE;


   if (!((colors=framePars[target_frame8].pars[P_WB_CTRL] & 0x0f)) || !(framePars[target_frame8].pars[P_WB_CTRL] & (1<<WB_CTRL_BIT_EN))) {
     GLOBALPARS_SNGL(G_NEXT_WB_FRAME)=frame+wb_period_change;
     return 0; /// white balance is off (mask==0)
   }
   if (GLOBALPARS_SNGL(G_NEXT_WB_FRAME) > frame) return 0; /// too early to bother
   MDF3(fprintf(stderr,"G_WB_INTEGERR=0x%08lx\n",GLOBALPARS_SNGL(G_WB_INTEGERR))); ///======= 0 here
   colors |= (1 << COLOR_Y_NUMBER);
/// Get the histogram (including percentiles)
   lseek(fd_histogram_cache, LSEEK_HIST_WAIT_C, SEEK_END); /// wait for all histograms, not just Y (G1)
   lseek(fd_histogram_cache, LSEEK_HIST_NEEDED + (colors << 8), SEEK_END);      /// specify that reverse histogram(s) are needed. Are they?
   hist_index=lseek(fd_histogram_cache, frame-1, SEEK_SET);                     /// request histograms for the previous frame
   if(hist_index <0) {
     ELP_FERR(fprintf(stderr, "Requested histograms for frame %d (0x%x) are not available\n",frame-1,frame-1));
     return -1;
   }


   if (histogram_cache[hist_index].frame < (frame-1)) { /// histogram is too old - try again
     GLOBALPARS_SNGL(G_NEXT_WB_FRAME)=frame+1;
     if (wb_dont_sync) return 0; /// will request histogram for this frame
/// repeat up to 8 times trying to get a fresh histogram
     for (i=0; i<8; i++) {
       frame++;
       MDF3(fprintf(stderr,"Skipping frame trying to synchronize, frame will be 0x%x\n",frame));
       lseek(fd_fparmsall, frame +LSEEK_FRAME_WAIT_ABS, SEEK_END);
       this_frame=frame;
       hist_index=lseek(fd_histogram_cache, frame-1, SEEK_SET);                     /// request histograms for the previous frame
       if (histogram_cache[hist_index].frame == (frame-1)) break;
     }
     if (histogram_cache[hist_index].frame < (frame-1)) { /// histogram is too old - try again
       GLOBALPARS_SNGL(G_NEXT_WB_FRAME)=frame+1;
       return 0; /// will request histogram for this frame
     }
   }

   wb_whitelev=framePars[target_frame8].pars[P_WB_WHITELEV];
   wb_whitefrac=framePars[target_frame8].pars[P_WB_WHITEFRAC];
   wb_maxwhite=framePars[target_frame8].pars[P_WB_MAXWHITE];

   if (!wb_whitelev  || (wb_whitelev  > 0xffff))  wb_whitelev =DEFAULT_WB_WHITELEV;  ///  0xfae1 // 98%
   if (!wb_whitefrac || (wb_whitefrac > 0xffff)) wb_whitefrac=DEFAULT_WB_WHITEFRAC; ///  0x028f // 1%
   if (!wb_maxwhite  || (wb_maxwhite  > 0xffff)) wb_maxwhite= wb_whitefrac<<2; /// 48 
   MDF4(fprintf(stderr,"colors=0x%x wb_whitelev=0x%x, wb_whitefrac=0x%x\n",colors,wb_whitelev,wb_whitefrac));
   corr[COLOR_RED]=   framePars[target_frame8].pars[P_WB_SCALE_R];
   corr[COLOR_GREEN1]=0x10000;
   corr[COLOR_GREEN2]=framePars[target_frame8].pars[P_WB_SCALE_GB];
   corr[COLOR_BLUE]=  framePars[target_frame8].pars[P_WB_SCALE_B];

   for (color=0;color<4;color++) if (!corr[color]) corr[color]=0x10000;
   MDF4(fprintf(stderr,"corr[]= 0x%04x 0x%04x 0x%04x 0x%04x\n",corr[0],corr[1],corr[2],corr[3]));
/// Find the "brightest" color (maximal number of pixels above wb_whitelev ).  wb_whitelev is currently rounded to 8 bits
   max_white_pixels=0;
   num_pixels=histogram_cache[hist_index].cumul_hist[255+ (COLOR_Y_NUMBER<<8)]; /// this one is definitely calculated
   if (num_pixels < MIN_PIXELS_TO_ADJUST) {
     ELP_FERR(fprintf(stderr, "Few pixels to process, giving up. num_pixels=%d, frame=%d (0x%x)\n", num_pixels, frame,frame));
     return -1;
   }
   min_white_pixels=num_pixels;
   wb_whitepix= (((long long) num_pixels) * wb_whitefrac)>>16;
   wb_whiteindex=(wb_whitelev>>8) & 0xff;
   wb_maxwhite  =(((long long) num_pixels) * wb_maxwhite)>>16;
   MDF4(fprintf(stderr,"num_pixels=0x%04x wb_whitepix=0x%04x wb_whiteindex=0x%04x\n",num_pixels,wb_whitepix,wb_whiteindex));
/// Find the "brightest" color (maximal number of pixels above wb_whitelev ).  wb_whitelev is currently rounded to 8 bits
   for (color=0;color<4;color++) if (colors & (1 << color)) {
     hist_cumul=     &(histogram_cache[hist_index].cumul_hist[(color << 8)]);
     if ( ((white_pixels= (num_pixels-hist_cumul[wb_whiteindex])))> max_white_pixels) {
       max_white_pixels=white_pixels;
       brightest_color=color;
     }
     if (min_white_pixels> white_pixels) min_white_pixels = white_pixels;
   }
   MDF4(fprintf(stderr,"brightest_color=%d, max_white_pixels= 0x%x, min_white_pixels=0x%x wb_whiteindex=0x%x, wb_maxwhite=0x%x\n", brightest_color, max_white_pixels, min_white_pixels, wb_whiteindex,wb_maxwhite));
///#define P_WB_MAXWHITE    175 /// Maximal allowed "white" pixels fraction (16.16) to have level above [P_WB_WHITELEV] for the darkest color
                             /// if this limit is exceeded there will be no correction (waiting for autoexposure to decrease overall brightness)
   if (min_white_pixels > wb_maxwhite) { /// everexposure - wait for the next chance
     MDF4(fprintf(stderr,"Overexposure - giving up: min_white_pixels=0x%x wb_maxwhite=0x%x\n",min_white_pixels, wb_maxwhite));
     return 0; /// wait for the next frame (skip normal interval here?)
   }
   while (max_white_pixels < wb_whitepix)  { /// No, wb_whitelev has to be lowered and brightest color is unknown (maybe max_white_pixels==0)
     for (brightest_color=0;brightest_color<4;brightest_color++) if (colors & (1 << brightest_color)) {
       if ( ((max_white_pixels= (num_pixels-histogram_cache[hist_index].cumul_hist[(brightest_color << 8) +wb_whiteindex])))>=wb_whitepix) break;
     }
     wb_whiteindex--;
   }
   MDF4(fprintf(stderr,"brightest_color=%d, max_white_pixels= 0x%04x, wb_whiteindex=0x%x\n",brightest_color,max_white_pixels,wb_whiteindex));
   hist_cumul=&(histogram_cache[hist_index].cumul_hist[brightest_color << 8]);
/// See if enough pixels are above wb_whitelev
   if (max_white_pixels < wb_whitepix) { /// No, wb_whitelev has to be lowered
     while (wb_whiteindex && (hist_cumul[wb_whiteindex] > (num_pixels-wb_whitepix))) wb_whiteindex--;
   }
   wb_whitelev=wb_whiteindex<<8;
/// Find levels for other (than brightest) colors so the same number of pixels is below it. First - output range, then correct by gamma-tables to input range
   nonwhite_pixels=hist_cumul[wb_whiteindex];  /// number of non-white (less than level) pixels for the brightest color
   if (nonwhite_pixels<0) nonwhite_pixels=0; /// still bad
   nonwhite_frac= (((long long) nonwhite_pixels)<<16)/num_pixels;
   white_pixels=num_pixels-nonwhite_pixels;

   levels[brightest_color]=wb_whitelev;
   MDF4(fprintf(stderr,"wb_whitelev=0x%x white_pixels=0x%04x, nonwhite_pixels=0x%04x, nonwhite_frac= 0x%04x\n",wb_whitelev, white_pixels,nonwhite_pixels,nonwhite_frac));
   for (color=0;color<4;color++) if (colors & (1 << color)) {
    if (color != brightest_color){
/// nonwhite_frac-> levels[color]
       hist_cumul=     &(histogram_cache[hist_index].cumul_hist[color<<8]);
       hist_percentile=&(histogram_cache[hist_index].percentile[color<<8]);
       perc=hist_percentile[nonwhite_frac >> (PERCENTILE_SHIFT-8)]; /// 8 MSBs used as index
       MDF4(fprintf(stderr,"perc=0x%04x nonwhite_frac >> (PERCENTILE_SHIFT-8)= 0x%04x\n",perc, nonwhite_frac >> (PERCENTILE_SHIFT-8)));
       if (perc>0) perc--;                                              /// seems hist_percentile[perc] rounds up, not down
       while ((perc>0) &&  (hist_cumul[perc] > nonwhite_pixels)) perc--;    /// adjust down  (is that needed at all?)
       perc++;
       while ((perc<255) &&  (hist_cumul[perc] <= nonwhite_pixels)) perc++; /// adjust up  (is that needed at all?)
       perc--;
       delta=hist_cumul[perc+1] - hist_cumul[perc];
       MDF4(fprintf(stderr,"perc=0x%04x delta= 0x%04x\n",perc, delta));
       if (delta) {
         perc_frac=((nonwhite_pixels-hist_cumul[perc]) << (PERCENTILE_SHIFT-8))/delta;
       } else perc_frac=0;
       levels[color]=(perc << (PERCENTILE_SHIFT-8)) + perc_frac;
       MDF4(fprintf(stderr,"hist_cumul[0x%x]=0x%lx hist_cumul[0x%x]=0x%lx, delta=0x%x, levels[%d]=0x%x\n",perc,hist_cumul[perc],perc+1,hist_cumul[perc+1],delta,color,levels[color]));
     }
/// Now - "ungamma" levels to map them to the sensor input data
/// sets global gamma_index, uses global hist_index;
     if (!setGammaIndex(color))  return -1; /// could not get gamma table
     levels[color]= gammaReverse (levels[color]) - get_imageParamsThat (P_SCALE_ZERO_OUT, frame-1); /// Could use "this", not "that" - unlikely to change
     gains[color] = histogram_cache[hist_index].gains[color&3];
     new_gains[color] =gains[color];

//     MDF4(fprintf(stderr,"levels[%d]=0x%x\n",color,levels[color]));
   }
   MDF4(fprintf(stderr,"levels[0]=0x%x levels[1]=0x%x levels[2]=0x%x levels[3]=0x%x\n",levels[0],levels[1],levels[2],levels[3]));
   MDF4(fprintf(stderr,"gains[0]=0x%x gains[1]=0x%x gains[2]=0x%x gains[3]=0x%x\n",gains[0],gains[1],gains[2],gains[3]));
/// Now levels are (0..0xffff)-fatzero (so can be negative?) and the gains should be calculated for R,G2,B - don't touch G here
///corr[0]

   if (levels[COLOR_Y_NUMBER] < MIN_LEVEL_TO_ADJUST) { /// green level is too low to adjust
//       return -1; /// give up?
     levels[COLOR_Y_NUMBER]= MIN_LEVEL_TO_ADJUST; /// still try
   }

//  int target_frame8=target_frame & PARS_FRAMES_MASK;
//  int ae_period_change=framePars[target_frame8].pars[P_AE_PERIOD] & 0xff; /// lower byte
/// It is important to use the new value of Green gain - it (an all the rest gains) could change if application had written
/// green gain and driver adjusted the rest of them. It would be safer to recalculate all the gains from the taget frame green gain 
/// and then write all in one atomic write, including the same green. That writing green back will help in the cases when write
/// occured after we read green gain but before had written back. So we are risking missing the gain chnage, otherwise we'll have
/// the gains mismatch. But tghat can be chnaged by define
#define BETTER_MISS_GAIN_CHANGE 1
   for (color=0;color<4;color++) if ((colors & (1 << color)) && (color != COLOR_Y_NUMBER)) {
/// gain[i]=gain[i]*level[green]/level[i]*corr[i]>>16
     if (levels[color]<MIN_LEVEL_TO_ADJUST) levels[color] =MIN_LEVEL_TO_ADJUST;
     new_gains[color]= (((long long) gains[color]) * levels[COLOR_Y_NUMBER] / levels[color] * corr[color]) >> 16;
///per-color damping is here
     if (error_thresh>0) { /// if threshold ==0 - just apply what is calculated
       wb_err[color]+=(new_gains[color]>>4); /// Too high precision - scale to have reasonable threshold ~500, same as for the autoexposure
       wb_err[color]-=(gains[color]>>4);
       aerr=abs(wb_err[color])- (error_thresh >> 1); /// TRYING: <1/2 of error_thresh - no correction at all, (0.5..1.5)*error_thresh - scale
       if (aerr>0) {
         if (aerr > error_thresh) aerr=error_thresh;
         new_gains[color]=(gains[color]*(error_thresh-aerr)+new_gains[color]*aerr)/error_thresh;
         if (aerr < error_thresh) wb_err[color]=   (wb_err[color]*(error_thresh-aerr))/error_thresh; /// reduce residual error
         else                     wb_err[color]= 0;                                                  /// reset residual  error (save on division)
       } else new_gains[color]=gains[color];
     }

   }
   MDF5(fprintf(stderr,"wb_err[0]=%d,  wb_err[1]=%d, wb_err[2]=%d, wb_err[3]=%d\n",wb_err[0],wb_err[1],wb_err[2],wb_err[3]));
   MDF5(fprintf(stderr,"new_gains[0]=0x%x new_gains[1]=0x%x new_gains[2]=0x%x new_gains[3]=0x%x\n",new_gains[0],new_gains[1],new_gains[2],new_gains[3]));
   unsigned long targetGain=framePars[target_frame & PARS_FRAMES_MASK].pars[P_GAINR+COLOR_Y_NUMBER];
   if (targetGain!=gains[COLOR_Y_NUMBER]) { /// need to correct gains to the green gain at target frame modified by externally
     for (color=0;color<4;color++) if ((colors & (1 << color)) && (color != COLOR_Y_NUMBER)) {
/// gain[i]*=targetGain/gain[COLOR_Y_NUMBER]
 /// could take division out of cycle, but that's not that often to get here
       new_gains[color]= (((long long) new_gains[color]) * targetGain)/ gains[COLOR_Y_NUMBER];
     }
     new_gains[COLOR_Y_NUMBER]=targetGain;
     MDF4(fprintf(stderr,"Corrected for green gain changed: new_gains[0]=0x%x new_gains[1]=0x%x new_gains[2]=0x%x new_gains[3]=0x%x\n",new_gains[0],new_gains[1],new_gains[2],new_gains[3]));
   }

/// Now apply modified scales
  i=0;
  write_data[i++]= FRAMEPARS_SETFRAME;
  write_data[i++]= target_frame;
  for (color=0;color<4;color++) if ((colors & (1 << color)) && (BETTER_MISS_GAIN_CHANGE ||(color != COLOR_Y_NUMBER))){
    write_data[i++]= P_GAINR+color;
    write_data[i++]= new_gains[color];
  }
  rslt=write(fd_fparmsall, write_data, (i << 2));
  if (rslt < (i << 2)) return -errno;
  GLOBALPARS_SNGL(G_NEXT_WB_FRAME)=frame+framePars[target_frame8].pars[P_WB_PERIOD]; /// modify overall next_frame
  return 1;
/// TODO: add error integration, don't rush for instant changes?

}
