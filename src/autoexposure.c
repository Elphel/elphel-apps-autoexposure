/*!***************************************************************************
*! FILE NAME  : autoexposure.c
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
*!  $Log: autoexposure.c,v $
*!  Revision 1.8  2012/04/08 04:10:36  elphel
*!  rev. 8.2.2 - added temperature measuring daemon, related parameters for logging SFE and system temperatures
*!
*!  Revision 1.7  2010/12/16 16:52:15  elphel
*!  working on HDR autoexposure
*!
*!  Revision 1.6  2010/12/15 16:46:44  elphel
*!  autoexposure correction: hdr synchronization
*!
*!  Revision 1.5  2010/12/14 23:14:18  dzhimiev
*!  1. fixed waiting time for daemons to die
*!  2. + comments
*!
*!  Revision 1.4  2008/12/11 17:10:56  elphel
*!  Skipping one extra frame at startup of autoexposure
*!
*!  Revision 1.3  2008/12/10 02:07:53  elphel
*!  removed obsolete debug output
*!
*!  Revision 1.2  2008/12/02 19:13:10  elphel
*!  It now waits for frame #9 before starting. TODO: make other detection of good frames
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.16  2008/11/18 19:30:16  elphel
*!  Added initialization of the "next" frame - otherwise it wait _very_ long if the camera frame number is reset
*!
*!  Revision 1.15  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!  Revision 1.14  2008/11/15 07:05:38  elphel
*!  implemented analog gain control (in addition to gammas) while while balancing
*!
*!  Revision 1.13  2008/11/15 03:10:13  elphel
*!  Some parameters renamed, reassigned.
*!
*!  Revision 1.12  2008/11/14 07:13:32  elphel
*!  Added gammaReverse() and gammaDirect() functions, TODO lists, bug fixes
*!
*!  Revision 1.11  2008/11/14 01:01:59  elphel
*!  cleared debug output
*!
*!  Revision 1.10  2008/11/13 05:40:45  elphel
*!  8.0.alpha16 - modified histogram storage, profiling
*!
*!  Revision 1.9  2008/11/08 05:52:21  elphel
*!  debug feature
*!
*!  Revision 1.8  2008/11/04 17:40:34  elphel
*!  comment typos
*!
*!  Revision 1.7  2008/11/02 00:32:35  elphel
*!  added TODO
*!
*!  Revision 1.6  2008/10/31 18:26:32  elphel
*!  Adding support for constants like SENSOR_REGS32 (defined constant plus 32 to simplify referencing sensor registers from PHP
*!
*!  Revision 1.5  2008/10/29 04:18:28  elphel
*!  v.8.0.alpha10 made a separate structure for global parameters (not related to particular frames in a frame queue)
*!
*!  Revision 1.4  2008/10/28 07:05:12  elphel
*!  implemented white balance, HDR mode (1/1 and 2/2)
*!
*!  Revision 1.3  2008/10/26 05:55:38  elphel
*!  snapshot
*!
*!  Revision 1.2  2008/10/25 19:49:15  elphel
*!  8.0.alpha8 - added autoexposure to the installation
*!
*!  Revision 1.1  2008/10/24 00:34:12  elphel
*!  initial release
*!
*/ 
#include "autoexposure.h"

int main (int argc, char *argv[]) {
  int daemon_bit=0; ///TODO: - make it an argument
//  int perc;
  int rslt, ae_rslt;
  int hdr_mode; /// 0 - off, 1 each other frame, 2 - 2 on, 2 - off 
  unsigned long vexp , old_vexp, old_that_vexpos;
  int exp_ahead; 
  unsigned long next_frame;

  const char usage[]=   "Usage:\n%s [-b <daemon_bit_number> [-d <debug_bits>]]\n\n"
                        "Start autoexposure daemon, bind it to bit <daemon_bit_number> (0..31) in P_DAEMON_EN (ELPHEL_DAEMON_EN in PHP)\n"
                        "Optional debug_bits (hex number) enable different groups of debug messages (1 group per bit) to stderr\n\n";

  if (argc < 2) {
     daemon_bit=DAEMON_BIT_AUTOEXPOSURE;
  } else if ((argc < 3) || (strcasecmp(argv[1], "-b"))) {
     printf (usage,argv[0]);
     return 0;
  }
  if ((argc >=5) && (strcasecmp(argv[3], "-d")==0)) {
    autoexposure_debug=strtol(argv[4], NULL, 16);
  } else autoexposure_debug=1;

  daemon_bit=strtol(argv[2], NULL, 10);
  if ((daemon_bit<0) || (daemon_bit>31)) {printf ("Invalid bit number %d (should be 0..31)\n", daemon_bit); exit (1);}
  fprintf(stderr,"autoexposure started, daemon_bit=0x%x, debug=0x%x\n",daemon_bit,autoexposure_debug);
//  MDF1(fprintf(stderr,"\n"));
  if (initFilesMmap()<0) exit (1); /// initialization errors
  if (autoexposure_debug <0) { /// tempoorary hack for testing
    GLOBALPARS(G_DEBUG)=0;
    exit (0);
  }
  MDF0(fprintf(stderr,"autoexposure started, daemon_bit=0x%x, debug=0x%x\n",daemon_bit,autoexposure_debug));
/// Next function call will wait until the daemon_bit will be enabled in [P_DAEMON_EN] giving a chance to other applications to initialize
/// TODO: Find why earlier frames have bad histograms - frame 6 - 0 pixels, frame 7 - 0x3fff pixels and only 8-th has total_pixels=0x4c920
/// For now - just wait for frame 9 (it will use histogram from frame 8)
//  lseek(fd_fparmsall,9+LSEEK_FRAME_WAIT_ABS, SEEK_END); /// skip 2 frames (first got 0 pixels, 2- 0x3fff)
  lseek(fd_fparmsall,10+LSEEK_FRAME_WAIT_ABS, SEEK_END); /// skip 3 frames (first got 0 pixels, 2- 0x3fff) - one extra, sometimes it is needed

  while (1) { /// restart loop
    if (initParams(daemon_bit)<0) exit (1); /// initialization errors
/// Main loop

  while (1) {
      this_frame=GLOBALPARS(G_THIS_FRAME);
      MDF6(fprintf(stderr,"Waiting for autoexposure daemon to be enabled\n"));
      lseek(fd_histogram_cache, LSEEK_DAEMON_HIST_Y+daemon_bit, SEEK_END);   /// wait for autoexposure daemon to be enabled (let it sleep if not)
      if (GLOBALPARS(G_THIS_FRAME) != this_frame) {
///TODO: Make it possible for this_frame to lag slightly (1 frame) to compensate for CPU being busy with other tasks?
/// Need to re-initialize after long sleep 
       if (initParams(daemon_bit)<0) exit (1); /// initialization errors
      }
/// Is exposure black level calibration requested (will produce 2 (or 1, depending on trigger mode?) dark frames
      if (GLOBALPARS(G_HIST_DIM_01)==0xffffffff)  {
        rslt=recalibrateDim();
        MDF1(fprintf(stderr,"G_HIST_DIM_01: 0x%08lx, G_HIST_DIM_23: 0x%08lx, recalibrateDim()->%d\n",GLOBALPARS(G_HIST_DIM_01),GLOBALPARS(G_HIST_DIM_23),rslt));
        this_frame=GLOBALPARS(G_THIS_FRAME);
      }
/// In HDR mode make sure we skip those different frames;
      hdr_mode=get_imageParamsThis(P_HDR_DUR);
      if (hdr_mode>2) hdr_mode=2;
      if (hdr_mode>0) {
        skipHDR(hdr_mode,this_frame);
        this_frame=GLOBALPARS(G_THIS_FRAME);
      }
      exp_ahead=get_imageParamsThis(P_EXP_AHEAD);
      if (!exp_ahead) exp_ahead = 3;
      int hdr_target_frame=this_frame+exp_ahead;
/// debugging
   old_vexp=framePars[(this_frame+exp_ahead)  & PARS_FRAMES_MASK].pars[P_VEXPOS];
   old_that_vexpos= get_imageParamsThatValid(P_VEXPOS, this_frame-1);
MDF3(fprintf(stderr, "this_frame= 0x%x, this_frame+exp_ahead= 0x%x, old_vexp= 0x%x, old_that_vexpos = 0x%x\n",  (int) this_frame,(int) (this_frame+exp_ahead), (int) old_vexp, (int) old_that_vexpos));
      if (hdr_mode>0) { /// align target autoexposure frame to hdr mode
         if (hdr_target_frame & 1) hdr_target_frame++;
         if ((hdr_mode>1) && (hdr_target_frame & 2)) hdr_target_frame+=2;
/// if it is too far ahead, wait some frames
         if ((hdr_target_frame-this_frame)>(exp_ahead+1)) {
            lseek(fd_fparmsall, (hdr_target_frame-exp_ahead-1)+LSEEK_FRAME_WAIT_ABS, SEEK_END);
            this_frame=GLOBALPARS(G_THIS_FRAME);
         }
         exp_ahead=hdr_target_frame-this_frame;
      }
   old_vexp=framePars[(this_frame+exp_ahead)  & PARS_FRAMES_MASK].pars[P_VEXPOS];
   old_that_vexpos= get_imageParamsThatValid(P_VEXPOS, this_frame-1);
MDF3(fprintf(stderr, "this_frame= 0x%x, this_frame+exp_ahead= 0x%x, old_vexp= 0x%x, old_that_vexpos = 0x%x\n",  (int) this_frame,(int) (this_frame+exp_ahead), (int) old_vexp, (int) old_that_vexpos));
      if (((ae_rslt=aexpCorr(COLOR_Y_NUMBER, this_frame, this_frame+exp_ahead )))<0) break; /// restart on errors

//GLOBALPARS(G_AE_INTEGERR)
      vexp= framePars[(this_frame+exp_ahead)  & PARS_FRAMES_MASK].pars[P_VEXPOS];
      next_frame=GLOBALPARS(G_NEXT_AE_FRAME);
      if (ae_rslt>0) MDF1(fprintf(stderr,"aexpCorr(0x%x, 0x%lx, 0%lx) -> %d, VEXPOS will be 0x%lx (0x%lx , 0x%lx), next_frame=0x%lx\n", COLOR_Y_NUMBER,this_frame,this_frame+exp_ahead,ae_rslt,vexp,old_vexp, old_that_vexpos,next_frame));
///  WB processing
      rslt=whiteBalanceCorr(this_frame, this_frame+exp_ahead, ae_rslt );
      if (rslt>0) MDF2(fprintf(stderr,"whiteBalanceCorr(0x%lx, 0%lx) -> %d\n",this_frame,this_frame+exp_ahead,rslt));
      if (rslt<0) break;  /// restart on errors 

/// TODO:Add HDR here
//      int hdr_target_frame;
      if (hdr_mode>0) {
         rslt=exposureHDR(this_frame, this_frame+exp_ahead);
         MDF3(fprintf(stderr,"exposureHDR(0x%lx, 0%x) -> %d\n",this_frame,hdr_target_frame,rslt));
         if (rslt<0) break;  /// restart on errors 
//         if (rslt>0) next_frame=this_frame+4;
         next_frame=hdr_target_frame;

      }

/// need to wait for the next_frame here, requesting histogram(s) for it.
/// if it is far in the future - wait so we can schedule histograms ahead of time - both?
      MDF6(fprintf(stderr,"Waiting for the next frame = 0x%x\n", (int) next_frame));
      waitRequstPrevHist(next_frame);



/// Will process WB after exposure, don't chnage next_frame to next_frame_wb
///    histograms are only availble for the previous frame, so this_frame-1
/*
        perc=getPercentile(next_frame-1,COLOR_Y_NUMBER, framePars[next_frame & PARS_FRAMES_MASK].pars[P_AEXP_FRACPIX], 1 << COLOR_Y_NUMBER);
        MDF6(fprintf(stderr,"FRAME: 0x%lx, COLOR: %d, FRACTION: 0x%04lx RESULT:0x%04x, NOW: 0x%lx\n",next_frame-1,COLOR_Y_NUMBER,framePars[next_frame & PARS_FRAMES_MASK].pars[P_AEXP_FRACPIX],perc,GLOBALPARS(G_THIS_FRAME)));
*/
    }
    ELP_FERR(fprintf (stderr,"Restarting autoexposure due to errors, skipping a frame\n"));
    lseek(fd_fparmsall, GLOBALPARS(G_THIS_FRAME) + 1+LSEEK_FRAME_WAIT_ABS, SEEK_END);

  }
  return 0;
}

