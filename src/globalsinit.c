/*!***************************************************************************
*! FILE NAME  : globalsinit.c
*! DESCRIPTION: Opens files, mmap-s data structures, initializes variables
*!              For the autoexposure daemon
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
*!  $Log: globalsinit.c,v $
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




  int fd_fparmsall, fd_histogram_cache, fd_gamma_cache;
  struct framepars_all_t    *frameParsAll;
  struct framepars_t        *framePars;
  unsigned long             *globalPars;
  struct framepars_past_t   *pastPars;
  struct histogram_stuct_t  * histogram_cache; /// array of histograms
/// gamma cache is needed to re-linearize the data
  struct gamma_stuct_t  * gamma_cache; /// array of gamma structures
  int hist_index; /// to preserve it between calls
  int gamma_index; /// to preserve it between calls
  int aex_recover_cntr;
  unsigned long this_frame;

  int      autoexposure_debug;


/**
 * @brief open required files, mmap theurt data structures
 * uses global variables for files and mmap-ed data so they are accessible everywhere
 * @return 0 - OK, <0 - problems opening/mma-ing
 */
int initFilesMmap(int sensor_port, int sensor_subchannel) {
    const char *framepars_dev_names[SENSOR_PORTS] = {
            DEV393_PATH(DEV393_FRAMEPARS0),
            DEV393_PATH(DEV393_FRAMEPARS1),
            DEV393_PATH(DEV393_FRAMEPARS2),
            DEV393_PATH(DEV393_FRAMEPARS3)};

  const char *framepars_driver_name=framepars_dev_names[sensor_port];
  const char histogram_driver_name[]=DEV393_PATH(DEV393_HISTOGRAM);
  const char gamma_driver_name[]=    DEV393_PATH(DEV393_GAMMA);
///Frame parameters file open/mmap (read/write)
  fd_fparmsall= open(framepars_driver_name, O_RDWR);
  if (fd_fparmsall <0) {
     ELP_FERR(fprintf(stderr, "Open failed: (%s)\r\n", framepars_driver_name));
     return -1;
  }
  frameParsAll = (struct framepars_all_t *) mmap(0, sizeof (struct framepars_all_t) * HISTOGRAM_CACHE_NUMBER , PROT_READ | PROT_WRITE, MAP_SHARED, fd_fparmsall, 0);
  if((int) frameParsAll == -1) {
     ELP_FERR(fprintf(stderr, "problems with mmap: %s\n", framepars_driver_name));
     close (fd_fparmsall);
     return -1;
  }
  framePars =   frameParsAll->framePars;
  pastPars =    frameParsAll->pastPars;
  globalPars=   frameParsAll->globalPars;

///Histogrames file open/mmap (readonly)
  fd_histogram_cache= open(histogram_driver_name, O_RDONLY);
  if (fd_histogram_cache <0) {
     ELP_FERR(fprintf(stderr, "Open failed: (%s)\r\n", histogram_driver_name));
     close (fd_fparmsall);
     return -1;
  }
  // Select port and subchannel for histograms
  lseek(fd_histogram_cache, LSEEK_HIST_SET_CHN + (4 * sensor_port) + sensor_subchannel, SEEK_END); /// specify port/sub-channel is needed

  histogram_cache = (struct histogram_stuct_t *) mmap(0, sizeof (struct histogram_stuct_t) * HISTOGRAM_CACHE_NUMBER , PROT_READ, MAP_SHARED, fd_histogram_cache, 0);
  if((int) histogram_cache == -1) {
     ELP_FERR(fprintf(stderr, "problems with mmap: %s\n", histogram_driver_name));
     close (fd_fparmsall);
     close (fd_histogram_cache);
     return -1;
  }

///Gamma tables file open/mmap (readonly)
  fd_gamma_cache= open(gamma_driver_name, O_RDWR);
  if (fd_gamma_cache <0) {
     ELP_FERR(fprintf(stderr, "Open failed: (%s)\r\n", gamma_driver_name));
     close (fd_fparmsall);
     close (fd_histogram_cache);
     return -1;
  }
  gamma_cache = (struct gamma_stuct_t *) mmap(0, sizeof (struct gamma_stuct_t) * GAMMA_CACHE_NUMBER , PROT_READ, MAP_SHARED, fd_gamma_cache, 0);
  if((int) gamma_cache == -1) {
     ELP_FERR(fprintf(stderr, "problems with mmap: %s\n", gamma_driver_name));
     close (fd_fparmsall);
     close (fd_histogram_cache);
     close (fd_gamma_cache);
     return -1;
  }
  return 0; /// All initialized
}
/**
 * @brief give a chance for other applications to initialize P_* parameters related to autoexposure functionality
 * so the daemon itself can be started early
 * @return just 0 for now
 */
int initParams(int daemon_bit) {
    aex_recover_cntr=0;
    lseek(fd_histogram_cache, LSEEK_DAEMON_HIST_Y+daemon_bit, SEEK_END);   /// wait for autoexposure daemon to be enabled
    GLOBALPARS_SNGL(G_AE_INTEGERR)=0; /// reset running error
    GLOBALPARS_SNGL(G_WB_INTEGERR)=0; /// reset running error
    this_frame=GLOBALPARS_SNGL(G_THIS_FRAME);  /// set global frame number
    if (GLOBALPARS_SNGL(G_HIST_DIM_01)==0) {
      GLOBALPARS_SNGL(G_HIST_DIM_01)=DEFAULT_BLACK_CALIB | (DEFAULT_BLACK_CALIB <<16);
      GLOBALPARS_SNGL(G_HIST_DIM_23)=DEFAULT_BLACK_CALIB | (DEFAULT_BLACK_CALIB <<16);
      return 1 ; /// used default, no real calibration
    }
//#define P_AUTOEXP_EXP_MAX 81 //unsigned long exp_max;		/* 100 usec == 1 etc... */
    initAexpCorr();
    initWhiteBalanceCorr();
    return 0;
}


