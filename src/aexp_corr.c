/*!***************************************************************************
*! FILE NAME  : aexp_corr.c
*! DESCRIPTION: Autoexposure part of the autoexposure/white balance/hdr daemon
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
*!  $Log: aexp_corr.c,v $
*!  Revision 1.9  2010/12/16 16:52:15  elphel
*!  working on HDR autoexposure
*!
*!  Revision 1.8  2010/12/15 16:46:44  elphel
*!  autoexposure correction: hdr synchronization
*!
*!  Revision 1.7  2009/03/14 05:49:41  elphel
*!  Fixed autoexposure getting stuck at minimal (one scan line)
*!
*!  Revision 1.6  2008/12/14 07:32:54  elphel
*!  Monitoring image width (VIRT_WIDTH) and making autoexposure to use absolute time units (usec) instead of native lines when the width changes
*!
*!  Revision 1.5  2008/12/10 02:10:48  elphel
*!  Added no-correction zone,
*!  so integrated error less than half threshold will not cause any correction, from 0.5 to 1.5 threshold - correction will be scaled, above 1.5 - full correction applied.
*!  In the case correction is scaled, only portion of integrated error is removed after correction.
*!
*!  Revision 1.4  2008/12/04 02:24:08  elphel
*!  Added maximal exposure time conversion from usec to lines
*!
*!  Revision 1.3  2008/12/02 00:28:13  elphel
*!  multiple bugfixes, making white balance to work and work with camvc
*!
*!  Revision 1.2  2008/12/01 02:32:48  elphel
*!  Updated white balance to use new gains
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
void initAexpCorr(void) {
  GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=0; ///NOTE - autoexposure was stuck for a hours when I tried resetting frame number...
}
/**
 * @brief Single autoexposure correction step 
 * autoexposure parameters are used for the target frame, not for the currenly processed
 * @param color color number to process - normally COLOR_Y_NUMBER
 * @param frame "current" frame, histograms are ready for frame-1
 * @param target_frame frame, to which new exposure should be applied (that frame picture will change  brightness)
 *                     caller should take care of not bumping into HDR frame
 * @return <0 - error, 0 - no correction was made, 1 - correction to [P_VEXPOS] was made
 */
///P_AUTOEXP_OVEREXP_MAX - temporary

/// TODO: if there were no new histograms - just skip to the next frame
/// TODO: Add errors until they are above threshold (simplified logariphm)?
/// TODO: Add non-linear function to expand near-saturation area (and then experiment with such a function)
/// TODO: Don't correct colors when large exposure changes were made
/// TODO: fix exposure when the green is not the highest. Or is it a problem with white balance, not the AE? AE - when gain as above ~0x430/0x400
/// TODO: Maybe switch autoexposure to "color mode" if green is not highest?
//   return gammaReverse (perc_frac);

int aexpCorr(int color, int frame, int target_frame) {
//  int target_frame=frame+EXPOS_AHEAD;
  int rslt;
//  unsigned long write_data[6]; 
  unsigned long write_data[4]; 
  int frac; //,frac_recover;
  int level,level_gamma; //,num_pix;
  int perc;
  int old_vexpos;
  int old_expos; /// old_expos is used when image width is modified
  int new_vexpos;
  float fvexpos;
  int error_thresh= framePars[target_frame & PARS_FRAMES_MASK].pars[P_AE_THRESH];/// Threshold for integrated error - when errors are smaller - correction is scaled proportionally
  int max_vexpos;
  int dim;
  int diff;
//  int adiff,large_diff;
  int target_frame8=target_frame & PARS_FRAMES_MASK;
  int ae_period_change=framePars[target_frame8].pars[P_AE_PERIOD] & 0xff; /// lower byte
  int ae_period_nochange=(framePars[target_frame8].pars[P_AE_PERIOD] >> 8 ) & 0xff; /// next byte
  int ae_dont_sync=(framePars[target_frame8].pars[P_AE_PERIOD] & 0x10000); /// don't try to synchronize to availble histograms
  int i;
  int aerr=0; // just to keep compiler happy
  int * ae_err= (int *) &(GLOBALPARS_SNGL(G_AE_INTEGERR)); /// so it will be signed
  if (!ae_period_change) ae_period_change=DEFAULT_AE_PERIOD_CHANGE;
  if (!ae_period_nochange) ae_period_nochange=DEFAULT_AE_PERIOD_NOCHANGE;
//  unsigned long overexp_scale;
  if (!framePars[target_frame & PARS_FRAMES_MASK].pars[P_AUTOEXP_ON]) {
    GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+ae_period_change;
    return 0; /// autoexposure is turned off
  }
  if (GLOBALPARS_SNGL(G_NEXT_AE_FRAME)>frame) return 0; /// too early to bother

  MDF3(fprintf(stderr,"Accumulated error *ae_err=%d\n",*ae_err)); ///======= 0 here

  frac=framePars[target_frame & PARS_FRAMES_MASK].pars[P_AEXP_FRACPIX];
//  level=framePars[target_frame & PARS_FRAMES_MASK].pars[P_AEXP_LEVEL];
/// get (approximate if not updated) percentile for dim (1 scanline exposure) image -to use as a zero point exposure
  dim=(GLOBALPARS_SNGL((color>1)?G_HIST_DIM_23:G_HIST_DIM_01) >> ((color & 1)? 16 : 0)) & 0xffff;
/// measure
  perc=getPercentile(frame-1,color, frac, 1 << color); ///sets global hist_index, gamma_index
//      MDF2(fprintf(stderr,"got histogram for frame: 0x%lx,  NOW: 0x%lx\n",histogram_cache[hist_index].frame, GLOBALPARS_SNGL(G_THIS_FRAME)));
  if (histogram_cache[hist_index].frame < (frame-1)) { /// histogram is too old - try again
    GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+1;
    if (ae_dont_sync) return 0; /// will request histogram for this frame
/// repeat up to 8 times trying to get a fresh histogram
    for (i=0; i<8; i++) {
      frame++;
      MDF3(fprintf(stderr,"Skipping frame trying to synchronize, frame will be 0x%x\n",frame));
      lseek(fd_fparmsall, frame +LSEEK_FRAME_WAIT_ABS, SEEK_END);
      this_frame=frame;
      perc=getPercentile(frame-1,color, frac, 1 << color); ///sets global hist_index, gamma_index
      if (histogram_cache[hist_index].frame == (frame-1)) break;
    }
    if (histogram_cache[hist_index].frame < (frame-1)) { /// histogram is too old - try again
      GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+1;
      return 0; /// will request histogram for this frame
    }
  }
  level_gamma=framePars[target_frame & PARS_FRAMES_MASK].pars[P_AEXP_LEVEL];
/// calculate sensor level from gamma_level using gamma table with hash32 saved at hist_index
  level=gammaReverse (level_gamma);
  MDF3(fprintf(stderr,"->>> frame=0x%x, target_frame=0x%x,dim=0x%04x, frac=0x%04x, level=0x%x,level_gamma=0x%x, perc=0x%04x\n",frame,target_frame,dim,frac,level,level_gamma,perc));
  if (perc <0) {
     GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+ae_period_change;
     return -1; ///getPercentile() failed
  }
///
/// overexposure recovery. Triggered if the percentile is above middle point between the set level and 0xffff
///
//  if (perc > ((level+0xffff)>>1)) {
///  if ((perc > ((level_gamma+0x1ffff)>>2))&& (perc>0xf000) ) {
///  if ((perc > ((level_gamma+0x2ffff)>>2))&& (perc>0xf000) ) {
  if ((perc > ((level_gamma+0xffff)>>1))&& (perc>0xf000) ) { // more that a middle between level and upper limit of 0xfff
     aex_recover_cntr++; /// global counter
///NOTE: Maybe just dividing exposure by 2 /counter is enough?

     MDF1(fprintf(stderr,"--- Triggered overexposure recovery:perc=0x%x, level_gamma=0x%x,aex_recover_cntr=0x%x\n",perc,level_gamma,aex_recover_cntr));
  } else { /// no overexposition
    aex_recover_cntr=0;
  }

  old_vexpos= histogram_cache[hist_index].vexpos;
  old_expos=  histogram_cache[hist_index].expos;
  if (old_vexpos==1) {
    fvexpos=old_vexpos;
    new_vexpos= (fvexpos*level)/perc;
  } else {
    fvexpos=(old_vexpos-1);
    if (perc < (dim>>1)) { /// wrong/obsolete dim level - ignore it to avoid negatives)
      new_vexpos= (fvexpos*level)/perc;
    } else {
      new_vexpos= (fvexpos*(level-dim))/(perc-dim);
    }
    if (aex_recover_cntr > 3) { /// *** more aggressive
      MDF1(fprintf(stderr,">>>> Reducing new_vexpos twice\n"));
      new_vexpos=(new_vexpos>>1)+1;
    }
  }


  MDF3(fprintf(stderr,"old_vexpos=0x%x, new_vexpos=0x%x,\n",old_vexpos,new_vexpos));
/// Add some protection from autoexposure going crazy
  max_vexpos= framePars[target_frame & PARS_FRAMES_MASK].pars[P_AUTOEXP_EXP_MAX]; /// for now treat as number of lines TODO: change to usec?
//P_VIRT_WIDTH, P_CLK_SENSOR
  MDF3(fprintf(stderr,"max_vexpos=0x%x, [P_CLK_SENSOR]=0x%lx, [P_VIRT_WIDTH]=0x%lx\n", max_vexpos,framePars[target_frame & PARS_FRAMES_MASK].pars[P_CLK_SENSOR],framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH]));
  max_vexpos=(((long long) max_vexpos) * framePars[target_frame & PARS_FRAMES_MASK].pars[P_CLK_SENSOR])/
                                         framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH]/1000000;
  if (!max_vexpos) max_vexpos=0x10000; /// temporary limit
  if (new_vexpos < 1) new_vexpos=1;
  else {
    if (new_vexpos > max_vexpos) new_vexpos=max_vexpos;
    if (new_vexpos > (old_vexpos*4)) new_vexpos=(old_vexpos*4);
  }
  diff=poorLog(new_vexpos) - poorLog(old_vexpos);
  *ae_err+=diff;
//  adiff=abs(diff);
  MDF3(fprintf(stderr,"old_vexpos=0x%x, new_vexpos=0x%x, poorLog(new_vexpos)=0x%x,  poorLog(old_vexpos)=0x%x, diff=%d, *ae_err=%d\n", old_vexpos, new_vexpos, poorLog(new_vexpos), poorLog(old_vexpos), diff, *ae_err));
  error_thresh=framePars[target_frame & PARS_FRAMES_MASK].pars[P_AE_THRESH];/// Threshold for integrated error - when errors are smaller - correction is scaled proportionally
  if (error_thresh>0) {
//    aerr=abs (ae_err[0]);
//    aerr=abs(*ae_err) - (error_thresh>>2); /// TRYING: <1/4 of error_thresh - no correction at all, (0.25..1.25)*error_thresh - scale
    aerr=abs(*ae_err) - (error_thresh >> 1); /// TRYING: <1/4 of error_thresh - no correction at all, (0.25..1.25)*error_thresh - scale
    if (aerr>0) {
      if (aerr > error_thresh) aerr=error_thresh;
      new_vexpos=(old_vexpos*(error_thresh-aerr)+new_vexpos*aerr)/error_thresh;
      if (new_vexpos != old_vexpos) { /// fixing autoexposure getting stuck at new_vexpos == old_vexpos ==1
        if (aerr < error_thresh)  *ae_err=   (*ae_err * (error_thresh-aerr))/error_thresh; /// reduce residual error
        else                      *ae_err=0;                                               /// reset residual  error (save on division)
      }
    } else new_vexpos=old_vexpos;
  }
  MDF3(fprintf(stderr,"old_vexpos=0x%x, new_vexpos=0x%x, aerr=%d, ae_err=%d\n", old_vexpos, new_vexpos, aerr, *ae_err));
   if (new_vexpos==old_vexpos) {
     MDF3(fprintf(stderr,"No correction: thrshold=%d, aerr=%d\n",error_thresh,aerr));  ///======= 1 here
     GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+ae_period_nochange; /// try again next frame (extend in HDR)
     return 0; /// too little error - no change in exposure
  }
//  *ae_err=0; /// reset error accumulator TODO: If scaled - reduce resudual error, not reset it
/// Now apply that new exposure
/// And request 
/// First - see if image size was changed. If it was - use exposure (usec), not vexposure(lines)
  write_data[0]= FRAMEPARS_SETFRAME;
  write_data[1]= target_frame;
  int wasVirtWidth= (((long long) old_expos) * framePars[target_frame & PARS_FRAMES_MASK].pars[P_CLK_SENSOR]) / 1000000/ old_vexpos;
  int was_vexpos_from_expos= (((long long) old_expos) * framePars[target_frame & PARS_FRAMES_MASK].pars[P_CLK_SENSOR]) / 1000000/ framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH];
  int new_expos;
  MDF3(fprintf(stderr,"VIRT_WIDTH=0x%lx, wasVirtWidth=0x%x, was_vexpos_from_expos=0x%x\n", framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH], wasVirtWidth,was_vexpos_from_expos));
//  if ((framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH] > (wasVirtWidth+10)) || /// real is +/-1 - got 0x888/0x880
//      (framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH] < (wasVirtWidth-10))) { // changed, trigger exposure mode
  if ((was_vexpos_from_expos>(old_vexpos+1)) || (was_vexpos_from_expos<(old_vexpos-1))) {
    new_expos= (((long long) old_expos) * new_vexpos) / old_vexpos; 
    MDF1(fprintf(stderr,"Frame width changed, using absolute exposure new_expos %d (0x%x), old_expos=%d (0x%x) \n", new_expos, new_expos, old_expos,old_expos));
    MDF1(fprintf(stderr,"VIRT_WIDTH=0x%lx, wasVirtWidth=0x%x\n", framePars[target_frame & PARS_FRAMES_MASK].pars[P_VIRT_WIDTH], wasVirtWidth));
    write_data[2]= P_EXPOS; // | FRAMEPAIR_FORCE_NEWPROC ?
    write_data[3]= new_expos;
  } else {
    write_data[2]= P_VEXPOS; // | FRAMEPAIR_FORCE_NEWPROC ?
    write_data[3]= new_vexpos;
  }
  rslt=write(fd_fparmsall, write_data, sizeof(write_data));
  if (rslt < sizeof(write_data)) return -errno;
  GLOBALPARS_SNGL(G_NEXT_AE_FRAME)=frame+ae_period_change;
  return 1;
}

