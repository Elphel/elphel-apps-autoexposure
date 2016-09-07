/*!***************************************************************************
*! FILE NAME  : temperature.c
*! DESCRIPTION: Daemon to measure sensor (100338 rev E) and/or system temperature
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
*!  $Log: temperature.c,v $
*!  Revision 1.2  2012/04/16 00:23:53  elphel
*!  temperature daemon waits for the daemon enable bit to be set and skips one frame before probing devices
*!
*!  Revision 1.1  2012/04/08 04:10:36  elphel
*!  rev. 8.2.2 - added temperature measuring daemon, related parameters for logging SFE and system temperatures
*!
*!
*/ 
#include "temperature.h"

  int fd_fparmsall;
  struct framepars_all_t    *frameParsAll;
  struct framepars_t        *framePars;
  unsigned long             *globalPars;
//  struct framepars_past_t   *pastPars;
  unsigned long this_frame;
  int      temperature_debug;

  int  slaves[]=  {0x90, 0x30, 0x34, 0x38, 0x3c};
  char bus0slow[]={11,11, 1, 1, 7, 7};
  char bus0dflt[]={ 2, 2, 1, 1, 7, 7}; // will be updated
  int  tempIndices[]={0,1,1,2,3}; // where to write result
  short * tempData;
  long long due_time;
  long long due_times[5];
  long long now;
  unsigned long thisFrame;
  long long thatTime;
  unsigned long thatFrame;

  long long period_us;
  long long individual_period_us;


  const char SA56004X_LTM=0x0;
  const char SA56004X_LTL=0x22;
  const char MCP98242_LT=0x5;
  const char MCP98242_RESOL=0x8;

int initFrameParsMmap(void);
long long getNowTime(void);
int readTemperature(int indx);
int setResolution(int indx);


int main (int argc, char *argv[]) {
  int daemon_bit=DAEMON_BIT_TEMPERATURE;
//  int rslt;
//  unsigned long prev_seconds=0,prev_useconds=0;
//  int delta_msec;
//  unsigned long next_frame;


  const char usage[]=   "Usage:\n%s [-b <daemon_bit_number> | -nodaemon [-d <debug_bits>]]\n\n"
                        "Start temperature measurement daemon, bind it to bit <daemon_bit_number> (0..31, defaults to %d) in P_DAEMON_EN (ELPHEL_DAEMON_EN in PHP).\n"
                        "-nodaemon makes the program update temperatures once and exit.\n"
                        "Optional debug_bits (hex number) enable different groups of debug messages (1 group per bit) to stderr.\n\n";


  if (argc < 2) {
     daemon_bit=DAEMON_BIT_TEMPERATURE;

  } else if (strcasecmp(argv[1], "-nodaemon")==0) {
     daemon_bit=-1;
  } else if ((argc < 3) || ((strcasecmp(argv[1], "-b") && (strcasecmp(argv[1], "-d"))))) {
     printf (usage,argv[0],daemon_bit);
     return 0;
  } else {
    daemon_bit=strtol(argv[2], NULL, 10);
  }
  if ((argc >=2) && (strcasecmp(argv[argc-2], "-d")==0)) {
    temperature_debug=strtol(argv[argc-1], NULL, 16);
  } else temperature_debug=1;


 
  if (daemon_bit>31) {printf ("Invalid bit number %d (should be 0..31)\n", daemon_bit); exit (1);}
  if (daemon_bit>=0) fprintf(stderr,"Temperature monitor started, daemon_bit=0x%x\n",daemon_bit);
  else               fprintf(stderr,"Temperature monitor in no-daemon mode (single run)\n");
  if (initFrameParsMmap()<0) exit (1); /// initialization errors
//  lseek(fd_fparmsall,10+LSEEK_FRAME_WAIT_ABS, SEEK_END); /// skip 3 frames (first got 0 pixels, 2- 0x3fff) - one extra, sometimes it is needed
  tempData= (short *) &(GLOBALPARS(G_TEMPERATURE01)); // 2 32-bit words

  now=getNowTime();
  int numTemperatureDevices=0;
  int indx;
  period_us=((long long)1000)*((long long) get_imageParamsThis(P_TEMPERATURE_PERIOD));
  if (daemon_bit>=0){ // skip frame to make sure the 10359 is programmed
    lseek(fd_fparmsall, LSEEK_DAEMON_FRAME+daemon_bit, SEEK_END);   /// wait for temperature daemon to be enabled (let it sleep if not)
    lseek(fd_fparmsall, LSEEK_FRAME_WAIT_REL+1, SEEK_END);
  }
  for (indx=0;indx<5;indx++){
     int result=readTemperature(indx);
     if (result<0) slaves[indx]=-1;
     else  {
        tempData[tempIndices[indx]]=result;
        due_times[indx]=now+period_us;
        numTemperatureDevices++;
      MDF3(fprintf(stderr,"Due time %d %d ms\n",indx, (int)((due_times[indx]/((long long)1000)) & 0x7fffffff)));
      if (indx>0) setResolution(indx);
    }
  }


  if (numTemperatureDevices==0){
    fprintf(stderr,"No temperature measurement devices found, exiting.");
    return 0;
  }
  individual_period_us=period_us/numTemperatureDevices;
  due_time=now+individual_period_us;
  MDF3(fprintf(stderr,"Due time %d ms\n", (int)((due_time/((long long)1000)) & 0x7fffffff)));
  if (daemon_bit<0) return 0; // update once, exit
/// Main loop
  int i;
  thatTime=now;
  thatFrame=GLOBALPARS(G_THIS_FRAME);
// sleep one frame or until enabled
   lseek(fd_fparmsall, LSEEK_FRAME_WAIT_REL+1, SEEK_END);   /// skip 1 frame before returning (wait up to 255 frames with such command)

  while (1) {
      MDF6(fprintf(stderr,"Waiting for temperature daemon to be enabled, frame=%ld\n",GLOBALPARS(G_THIS_FRAME)));
      lseek(fd_fparmsall, LSEEK_DAEMON_FRAME+daemon_bit, SEEK_END);   /// wait for temperature daemon to be enabled (let it sleep if not)
      now=getNowTime();
      MDF3(fprintf(stderr,"Now is %d ms. due is %d ms \n",(int)((now/((long long)1000)) & 0x7fffffff),(int)((due_time/((long long)1000)) & 0x7fffffff)));
      thisFrame=GLOBALPARS(G_THIS_FRAME);
      if (now>=due_time){
        MDF3(fprintf(stderr,"  It is due!\n"));

        int numMeasured=0;
// update all temperatures that are already due;
        for (i=0;i<5;i++)  if ((slaves[i]>0) && (due_times[i]<due_time)){ // first service those that are due
             int temperature=readTemperature(i);
             if (temperature>=0) tempData[tempIndices[i]]=temperature; // >=0 is not >=0C :-)
             due_times[i]=now+period_us;
      MDF3(fprintf(stderr,"Due time %d is %d ms\n",i, (int)((due_times[i]/((long long)1000)) & 0x7fffffff)));
             numMeasured++;
           
        }
        MDF3(fprintf(stderr,"--- numMeasured=%d\n",numMeasured));
        if (numMeasured==0) { // none was due, measure the soonest to become due
          indx=-1;
          for (i=0;i<5;i++) if ((slaves[i]>0) && ((indx<0) || (due_times[i]<due_times[indx]))) indx=i;
          if (indx<0){
               fprintf(stderr,"BUG: earliest due indx<0");
               return -1;
          }
          int temperature=readTemperature(indx);
          if (temperature>=0) tempData[tempIndices[indx]]=temperature; // >=0 is not >=0C :-)
          due_times[indx]=now+period_us;
      MDF3(fprintf(stderr,"Updated earliest - due time %d is %d ms\n",indx, (int)((due_times[indx]/((long long)1000)) & 0x7fffffff)));
        }
        due_time=now+ individual_period_us;
        MDF3(fprintf(stderr,"Due time is  %d ms\n", (int)((due_time/((long long) 1000)) & 0x7fffffff)));
        for (i=0;i<5;i++) if ((slaves[i]>0) && (due_times[i]<due_time)) due_time=due_times[i];
        MDF3(fprintf(stderr,"Due time updated is  %d ms\n", (int)((due_time/((long long)1000)) & 0x7fffffff)));
      }
      // set due time as earliest of individual due times and now+ individual_period_us
      MDF3(fprintf(stderr,"Wait till next measurement for %d milliseconds\n",(int) ((due_time-now)/((long long)1000))));

// guess how many frames to wait
      int framePeriod=(now-thatTime)/(thisFrame-thatFrame);
      int numSkip=(due_time-now)/framePeriod;
      if (numSkip>255) numSkip=255;
      if (numSkip<1) numSkip=1;

      MDF3(fprintf(stderr,"Will skip  %d frames\n",numSkip));
      if (numSkip>0) lseek(fd_fparmsall, LSEEK_FRAME_WAIT_REL+numSkip, SEEK_END);   /// skip 1 frame before returning (wait up to 255 frames with such command)
    }
  }

  long long getNowTime(void){
      lseek(fd_fparmsall, LSEEK_GET_FPGA_TIME, SEEK_END);   /// get FPGA time
      long long result=GLOBALPARS(G_SECONDS);
      result*=1000000;
      result+=GLOBALPARS(G_MICROSECONDS);
      MDF4(fprintf(stderr,"getNowTime(): %ld.%06ld, result=%d, == %d ms\n",GLOBALPARS(G_SECONDS),GLOBALPARS(G_MICROSECONDS), (int) (result & 0x7fffffff),  (int) ((result/((long long) 1000)) & 0x7fffffff)));
      return result;
  }

  int readTemperature(int indx){
     MDF2(fprintf(stderr, "Measuring temperature data for index=%d, frame # %ld, slaves[index]=%d\n",indx,GLOBALPARS(G_THIS_FRAME),slaves[indx]));

    int result=-1;
    int devfd,ctlfd;
    unsigned char data8[2];
//    unsigned short *data16= (short *) data8;
    if (slaves[indx]<0) {MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -1;}
//    int err;

    if (indx==0) {
      devfd=open("/dev/xi2c8_aux", O_RDONLY);
      if (devfd<0) return -1;
      if (lseek(devfd,(slaves[indx]<<7)+(SA56004X_LTM),0)<0) { // 0x4800, byte mode
          close (devfd);
          return -1;
      }
      if (read(devfd,&data8[1],1)<1) {
          close (devfd);
          return -1;
      }
      if (lseek(devfd,(slaves[indx]<<7)+(SA56004X_LTL),0)<0) { // 0x4822
          close (devfd);
          return -1;
      }
      if (read(devfd,&data8[0],1)<1) {
          close (devfd);
        return -1;
      }

      result=((data8[1]<<4 ) | (data8[0]>>4)) & 0xfff;
//     MDF2(fprintf(stderr, "data8[0]=0x%x, data8[1]=0x%x, result=0x%04x\n",data8[0],data8[1],result));
      if (result & 0x800) {
          result&=0x7ff;
          result |=0x1000;
      }
    } else  { // bus 0 - need to slow down first
      ctlfd=open("/dev/xi2cctl", O_RDWR);
/**
      commented out so the communication speed will always be restored to fast (for the sensors)
      read(ctlfd,bus0dflt,6); // read and save old i2c bus0 timing parameters
      lseek(ctlfd,0,SEEK_SET); // rewind
*/
      write(ctlfd,bus0slow,6); // set slow i2c bus0 timing parameters
      devfd=open("/dev/xi2c16", O_RDONLY);
      if (devfd<0) return -1;
      if (lseek(devfd,((slaves[indx]<<7) +(MCP98242_LT))<<1,0)<0) { // 16-bit mode, 2 bytes/register
          close (devfd);
         // restore fast speed
         lseek(ctlfd,0,SEEK_SET); // rewind
         write(ctlfd,bus0dflt,6); // set slow i2c bus0 timing parameters // BUG: for some reason write returns here-1 after successful write
         close (ctlfd);
         return -1;
      }
      if (read(devfd,data8,2) <2) {
         close (ctlfd);
         // restore fast speed
         lseek(ctlfd,0,SEEK_SET); // rewind
         write(ctlfd,bus0dflt,6); // set slow i2c bus0 timing parameters
         close (ctlfd);
         return -1;
      } // read and save old i2c bus0 timing parameters
      //swap bytes
      result =((data8[0]<<8) | data8[1])&0x1fff; // sign+temperature as 8.4
// restore i2c bus 0 speed
      lseek(ctlfd,0,SEEK_SET); // rewind
      write(ctlfd,bus0dflt,6); // set slow i2c bus0 timing parameters
      close (ctlfd);
    }
    close (devfd);
    MDF1(fprintf(stderr,"Temperature data for index=%d is 0x%x, frame # %ld\n",indx, result, GLOBALPARS(G_THIS_FRAME)));
    return result;

/*
      if (ctlfd<0) {
         MDF2(fprintf(stderr, "Measuring temperature failed\n")); 
         return -1;
      }
      if (read(ctlfd,bus0dflt,6) <6) {close (ctlfd); MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -2;} // read and save old i2c bus0 timing parameters
      if (lseek(ctlfd,0,SEEK_SET)<0) {close (ctlfd); MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -2;} // rewind

      close (ctlfd);
      ctlfd=open("/dev/xi2cctl", O_RDWR);


//      if ((err=write(ctlfd,bus0slow,6))<6) {close (ctlfd); MDF2(fprintf(stderr, "Measuring temperature failed, err=%d\n",err)); return -2;} // set slow i2c bus0 timing parameters
      if ((err=write(ctlfd,bus0slow,6))<-6) {close (ctlfd); MDF2(fprintf(stderr, "Measuring temperature failed, err=%d\n",err)); return -2;} // set slow i2c bus0 timing parameters
      devfd=open("/dev/xi2c16", O_RDONLY);
      if (devfd<0)  {MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -1;}
      if (lseek(devfd,((slaves[indx]<<7) +(MCP98242_LT))<<1,0)<0) { // 16-but mode, 2 bytes/register
          close (devfd);
MDF2(fprintf(stderr, "Measuring temperature failed\n")); 
          return -1;
      }
      if (read(devfd,data16,2) <2) {close (ctlfd); MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -1;} // read and save old i2c bus0 timing parameters
      result =data16[0]&0x1fff; // sign+temperature as 8.4
// restore i2c bus 0 speed
      if (lseek(ctlfd,0,SEEK_SET)<0) {close (ctlfd);close (devfd); MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -2;} // rewind
      if (write(ctlfd,bus0dflt,6)<-6) {close (ctlfd);close (devfd); MDF2(fprintf(stderr, "Measuring temperature failed\n")); return -2;} // set slow i2c bus0 timing parameters
      close (ctlfd);
    }
    close (devfd);
     MDF2(fprintf(stderr,"Temperature data for index=%d is 0x%x, frame # %ld\n",indx, result, GLOBALPARS(G_THIS_FRAME)));

    return result;
*/
  }

  int setResolution(int indx){
    MDF2(fprintf(stderr, "Setting full temperature resolution for index=%d, frame # %ld, slaves[index]=%d\n",indx,GLOBALPARS(G_THIS_FRAME),slaves[indx]));
    int devfd,ctlfd;
    unsigned char data8[2];
    if (indx<1) return 0; // no resolution settings fro system temperature, only for SFE

      ctlfd=open("/dev/xi2cctl", O_RDWR);
/**
      commented out so the communication speed will always be restored to fast (for the sensors)
      read(ctlfd,bus0dflt,6); // read and save old i2c bus0 timing parameters
      lseek(ctlfd,0,SEEK_SET); // rewind
*/
      write(ctlfd,bus0slow,6); // set slow i2c bus0 timing parameters
      devfd=open("/dev/xi2c8", O_RDWR);
      if (devfd<0) return -1;
      if (lseek(devfd,(slaves[indx]<<7) +(MCP98242_RESOL),0)<0) { // 8-bit mode
          close (devfd);
         // restore fast speed
         lseek(ctlfd,0,SEEK_SET); // rewind
         write(ctlfd,bus0dflt,6); // set slow i2c bus0 timing parameters // BUG: for some reason write returns here-1 after successful write
         close (ctlfd);
         return -1;
      }
      data8[0]=3; // full resolution
      write(devfd,data8,1);
// restore i2c bus 0 speed
      lseek(ctlfd,0,SEEK_SET); // rewind
      write(ctlfd,bus0dflt,6); // set slow i2c bus0 timing parameters
      close (ctlfd);
    close (devfd);
    MDF1(fprintf(stderr,"Temperature sensor %d is set to maximal resolution\n",indx));
    return 0;




}

/**
 * @brief open required files, mmap theurt data structures
 * uses global variables for files and mmap-ed data so they are accessible everywhere
 * @return 0 - OK, <0 - problems opening/mma-ing
 */
int initFrameParsMmap(void) {
  const char framepars_driver_name[]="/dev/frameparsall";
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
//  pastPars =    frameParsAll->pastPars;
  globalPars=   frameParsAll->globalPars;

  return 0; /// All initialized
}

