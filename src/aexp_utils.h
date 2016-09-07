/*!***************************************************************************
*! FILE NAME  : aexp_utils.h
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
*!  $Log: aexp_utils.h,v $
*!  Revision 1.2  2008/12/01 02:32:48  elphel
*!  Updated white balance to use new gains
*!
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/ 
#ifndef __H_AUTOEXPOSURE_AEXP_UTILS__
#define __H_AUTOEXPOSURE_AEXP_UTILS__
unsigned long get_imageParamsThat     (int indx, unsigned long frame);
int           get_imageParamsThatValid(int indx, unsigned long frame);
int           setGammaIndex(int color);
long getPercentile(unsigned long frame,int color, unsigned long fraction,int request_colors);
int recalibrateDim(void);
unsigned long gammaDirect (unsigned long x);
unsigned long gammaReverse (unsigned long x);
int waitRequstPrevHist(unsigned long next_frame);
int poorLog(int x);
int poorExp(int x);

#define get_imageParamsThis(x) ( framePars[this_frame & PARS_FRAMES_MASK].pars[x])


#endif
