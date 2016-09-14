/*!***************************************************************************
*! FILE NAME  : globalsinit.h
*! DESCRIPTION: Daemon to adjust camera exposure and white balance
*! Copyright (C) 2008-2016 Elphel, Inc.
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
*!  $Log: globalsinit.h,v $
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/ 
#ifndef __H_AUTOEXPOSURE_GLOBALSINIT__
#define __H_AUTOEXPOSURE_GLOBALSINIT__

/// Global variables
  extern int                        fd_fparmsall;
  extern int                        fd_histogram_cache;
  extern int                        fd_gamma_cache;
  extern struct framepars_all_t   * frameParsAll;
  extern struct framepars_t       * framePars;
  extern unsigned long            * globalPars;
  extern struct framepars_past_t  * pastPars;
  extern struct histogram_stuct_t * histogram_cache; /// array of histograms
/// gamma cache is needed to re-linearize the data
  extern struct gamma_stuct_t     * gamma_cache; /// array of gamma structures
  extern int                        hist_index;        /// to preserve it between calls
  extern int                        gamma_index; /// to preserve it between calls
  extern int                        aex_recover_cntr;
  extern unsigned long              this_frame;
  extern int                        autoexposure_debug;

int initFilesMmap(int sensor_port, int sensor_subchannel);
int initParams(int daemon_bit);


#endif
