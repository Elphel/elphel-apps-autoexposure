/*!***************************************************************************
*! FILE NAME  : hdr_control.h
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
*!  $Log: hdr_control.h,v $
*!  Revision 1.1.1.1  2008/11/27 20:04:01  elphel
*!
*!
*!  Revision 1.1  2008/11/15 23:08:24  elphel
*!  8.0.alpha17 - split autoexposure source file
*!
*!
*/
#ifndef __H_AUTOEXPOSURE_HDR_CONTROL__
#define __H_AUTOEXPOSURE_HDR_CONTROL__

int skipHDR(int mode, unsigned long frame);
int exposureHDR (int frame, int target_frame);

#endif
