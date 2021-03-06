/*
 *  Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *   + Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  
 *   + Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  
 *   + Neither the name of mtbaldy.us nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

/*
 *  A Hobby Boards Inc. (HBI) H3-R1 Humidity Sensor is a DS18S20, a DS2438, and
 *  a Honeywell HIH 4000 series humidity sensor.
 *
 *  The sensor relative humidity (RH) is
 *
 *      sensor RH = (( Vout / Vsupply ) - (0.8 / Vsupply)) / 0.0062 at 25C
 *
 *  The temperature correction is
 *
 *      true RH = sensor RH / ( 1.0305 + 0.000044 T - 0.0000011 * T^2 )
 *
 *  where T is the sensor temperature measured in degrees Celsius.
 *
 *  Note HIH-4000 series spec sheets show 0.16 in place of (0.8V / Vsupply).
 *  That is because the spec sheets assume Vsupply = 5.0V which leads to
 *  0.16.
 *
 *  See Dan Awtrey's article entitled "A 1-Wire Humidity Sensor" from Sensors,
 *  The Journal of Applied Sensing Technology.
 */

/*
 *  Note: this code is quite similar to tai_8540.c as the AAG TAI-8540 is
 *  similar to the HBI H3-R1.  The primary difference between the two is
 *  the addition of a "high" precision thermometer (DS18S20) to the HBI
 *  unit.  So, like the EDS HMP2001, the current temperature is taken from
 *  the auxillary devices (DS18S20) rather than the device built into the
 *  DS2438(Z).
 *
 *  Also, the HBI unit uses a HIH-4000 serices humidity sensor; whereas,
 *  the AAG unit uses a HIH-3600 series sensor.  Different temperature
 *  compensations algorithms apply to those two series of devices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "err.h"
#include "os.h"
#include "device.h"
#include "ha7net.h"
#include "device_hbi_h3r1.h"

typedef struct {
     device_t      *ds18s20;
     unsigned char state[28];
} h3r1_t;

static const char *h3r1_rhrh_name  = "h3r1_rh";
static const char *h3r1_rhrh_prec  = "%0.2f";

static const char *h3r1_temp_name  = "h3r1_temp";
static const char *h3r1_temp_prec  = "%0.1f";

/*
 *  Device commands
 */
#define CONVERT_TEMP    "44"
#define CONVERT_VOLT    "B4"
#define READ_SCRATCHPAD "BE"
#define RECALL_MEMORY   "B8"

/*
 *  Device A/D channels
 */
#define CHANNEL_VDD     0x00
#define CHANNEL_VAD     0x01
#define CHANNEL_VSENSE  0x02

/*
 *  Device flags
 */
#define FLAG_IAD 0x01  /* Current A/D and ICA control bit         */
#define FLAG_CA  0x02  /* Current accumulator bit                 */
#define FLAG_EE  0x04  /* Current accumulator shadow selector bit */
#define FLAG_AD  0x08  /* A/D input select bit (1->VDD; 0->VAD)   */
#define FLAG_TB  0x10  /* Temperature conversion indicator        */
#define FLAG_NVB 0x20  /* NVRAM in use indicator                  */
#define FLAG_ADB 0x40  /* A/D converter in use indicator          */

static int
check(const char *func, ha7net_t *ctx, device_t *dev, size_t page, size_t dlen,
      size_t line)
{
     if (!ctx || !dev || page > 7 || dlen > 8)
     {
          dev_debug("%s(%u): Invalid call arguments supplied",
		    func ? func : "?", line);
          return(ERR_BADARGS);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_2438)
     {
          dev_debug("%s(%u): The device dev=%p with family code 0x%02x does "
		    "not appear to be a DS2438 (0x%02x); the device appears "
		    "to be a %s", func ? func : "?", line,
		    dev, dev_fcode(dev), OWIRE_DEV_2438,
		    dev_strfcode(dev_fcode(dev)));
          return(ERR_NO);
     }
     else
	  return(ERR_OK);
}


static int
check2(const char *func, ha7net_t *ctx, device_t *dev, size_t page,
       size_t dlen, h3r1_t **devx, size_t line)
{
     int istat;

     if (ERR_OK != (istat = check(func, ctx, dev, page, dlen, line)))
	  return(istat);

     /*
      *  Have to ensure that dev != NULL before this next bit
      */
     if (!devx)
	  return(ERR_OK);
     
     *devx = (h3r1_t *)dev_private(dev);
     if (*devx)
	  return(ERR_OK);

     dev_debug("%s(%u): " "h3r1_dev_init() has not yet been called for this "
	       "device", func ? func : "?", line);
     return(ERR_NO);
}


static int
ds2438_readpage(ha7net_t *ctx, device_t *dev, size_t page, unsigned char *data,
		size_t *dlen)
{
     char cmd[32];
     ha7net_crc_t crc = HA7NET_CRC8(2, 8, 0);
     unsigned char data2[11];
     int istat;

     /*
      *  Sanity checks
      */
     if (ERR_OK != (istat= check("ds2438_readpage", ctx, dev, page, 0,
				 __LINE__)))
	  return(istat);

     /*
      *  Initializations
      */
     if (dlen)
	  *dlen = 0;

     /*
      *  Recall memory to the scratch pad
      *  0xB8 <page>
      */
     strcpy(cmd, RECALL_MEMORY "00");
     cmd[3] = '0' + page;

     /*
      *  Note that ha7net_writeblock() will reset the bus and select
      *  the device for us (since we're supplying non-NULL for the
      *  second argument).
      */
     istat = ha7net_writeblock(ctx, dev, NULL, NULL, cmd, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_readpage(%d): Unable to copy page %u of device "
		    "memory to the devices's scratch pad; ha7net_writeblock() "
		    "returned %d; %s",
		    __LINE__, page, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  No need to reset the bus and reselect the device...
      *  ha7net_writeblock() will do that for us.
      */

     /*
      *  Now read the scratch pad
      *  0xBE <page> 0xFF (x 9)
      */
     strcpy(cmd, READ_SCRATCHPAD "00FFFFFFFFFFFFFFFFFF");
     cmd[3] = '0' + page;
     istat = ha7net_writeblock_ex(ctx, dev, data2, 11, cmd, &crc, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_readpage(%d): Unable to read the device's scratch "
		    "pad; ha7net_writeblock() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  We ignore the leading two bytes ("BE00") and any trailing
      *  byte (CRCs and dribble).
      */
     if (data)
	  memcpy(data, data2 + 2, 8);
     if (dlen)
	  *dlen = 8;

     /*
      *  All done
      */
     return(ERR_OK);
}


static int
ds2438_writepage(ha7net_t *ctx, device_t *dev, size_t page,
		 unsigned char *data, size_t dlen)
{
     char cmd[32], *ptr;
     size_t i;
     int istat;
     static const char *hex = "0123456789ABCDEF";

     /*
      *  Sanity checks
      */
     if (ERR_OK != (istat= check("ds2438_writepage", ctx, dev, page, dlen,
				 __LINE__)))
	  return(istat);

     /*
      *  Write 8 bytes of data to the scratchpad
      *  0x4E <page> <data[0]> ... <data[7]>
      *          " a b 1 2 3 4 5 6 7 8"
      */
     strcpy(cmd, "4E000000000000000000");
     cmd[3] = '0' + page;
     if (dlen > 8)
	  dlen = 8;
     ptr = cmd + 4;
     for (i = 0; i < dlen; i++)
     {
	  *ptr++ = hex[(data[i] & 0xf0) >> 4];
	  *ptr++ = hex[data[i] & 0x0f];
     }
     *ptr = '\0';

     /*
      *  Note that ha7net_writeblock() will reset the bus and select
      *  the device for us (since we're supplying non-NULL for the
      *  second argument).
      */
     istat = ha7net_writeblock(ctx, dev, NULL, NULL, cmd, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_writepage(%d): Unable to write data to the "
		    "device's scratch pad; ha7net_writeblock() returned %d; "
		    "%s", __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  No need to reset the bus and reselect the device...
      *  ha7net_writeblock() will do that for us.
      */

     /*
      *  Move the scratch pad to device memory
      *  0x48 <page>
      */
     cmd[0] = '4';
     cmd[1] = '8';
     cmd[2] = '0';
     cmd[3] = '0' + page;
     cmd[4] = '\0';
     istat = ha7net_writeblock(ctx, dev, NULL, NULL, cmd, 0);
     if (istat == ERR_OK)
	  return(ERR_OK);

     dev_debug("ds2438_writepage(%d): Unable to copy the device's scratch pad "
	       "to page %u of the device's memory; ha7net_writeblock() "
	       "returned %d; %s", __LINE__, page, istat, err_strerror(istat));

     return(istat);
}


static int
ds2438_flag_test(ha7net_t *ctx, device_t *dev, unsigned char flag,
		 int *fvalue)
{
     unsigned char data[9];
     size_t dlen;
     int istat;

     /*
      *  Leave sanity checks up to ds2438_readpage()
      */

     /*
      *  Read page 0 from the device
      */
     dlen = 0;
     istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_flag_test(%d): Unable to read device flag "
		    "0x%02x; ds2438_readpage() returned %d; %s",
		    __LINE__, flag, istat, err_strerror(istat));
	  return(istat);
     }
     else if (!dlen)
     {
	  dev_debug("ds2438_flag_test(%d): Unable to read device flag "
		    "0x%02x; ds2438_readpage() returned to no data",
		    __LINE__, flag);
	  return(ERR_NO);
     }

     /*
      *  Return the result
      */
     if (fvalue)
	  *fvalue = (data[0] & flag) ? -1 : 0;

     return(ERR_OK);
}


static int
ds2438_flag_set(ha7net_t *ctx, device_t *dev, unsigned char flag, int fvalue,
		unsigned char flags2, int fvalue2)
{
     int attempts, istat;
     unsigned char data[9];
     size_t dlen;

     /*
      *  Leave sanity checks up to ds2438_readpage()
      */

     /*
      *  Obtain page 0 from the device
      */
     dlen = 0;
     istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_flag_set(%d): Unable to read device flag "
		    "0x%02x; ds2438_readpage() returned %d; %s",
		    __LINE__, flag, istat, err_strerror(istat));
	  return(istat);
     }
     else if (!dlen)
     {
	  dev_debug("ds2438_flag_set(%d): Unable to read device flag "
		    "0x%02x; ds2438_readpage() returned to no data",
		    __LINE__, flag);
	  return(ERR_NO);
     }

     /*
      *  Set or clear the flag in page 0
      */
     attempts = 1;
try:
     if (fvalue)
	  data[0] |= flag;
     else
	  data[0] &= ~flag;

     if (flags2)
     {
	  if (fvalue2)
	       data[0] |= flags2;
	  else
	       data[0] &= ~flags2;
     }

     /*
      *  Now write page 0 back to the device
      */
     dev->lastcmd.tv_sec  = 0;
     dev->lastcmd.tv_usec = 0;
     istat = ds2438_writepage(ctx, dev, 0, data, dlen);
     if (istat != ERR_OK)
     {
	  dev_debug("ds2438_flag_set(%d): Unable to set device flag "
		    "0x%02x; ds2438_writepage() returned %d; %s",
		    __LINE__, flag, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Pause
      */
     if (dev->lastcmd.tv_sec != 0 ||
	 dev->lastcmd.tv_usec != 0)
     {
	  unsigned int dt;
	  struct timeval tv;

	  gettimeofday(&tv, NULL);

	  /* Delta in milliseconds */
	  dt = (tv.tv_sec - dev->lastcmd.tv_sec) * 1000 +
	       (tv.tv_usec - dev->lastcmd.tv_usec) / 1000;

	  if (dt < 10)
	       os_sleep(10 - dt);
     }
     else
	  os_sleep(10);

     /*
      *  Read the page back
      */
     dlen = 0;
     istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
     if (istat != ERR_OK || !dlen)
     {
	  dev_debug("ds2438_flag_set(%d): Attempt to read flag page after "
		    "writing device flag 0x%02x failed; ROM id %d; "
		    "ds2438_readpage() returned %d; %s",
		    __LINE__, flag, dev->romid, istat, err_strerror(istat));
	  return(ERR_OK);
     }

     /*
      *  See if the flag got set
      */
     if (fvalue)
     {
	  if (data[0] & flag)
	       return(ERR_OK);
     }
     else
     {
	  if (!(data[0] & flag))
	       return(ERR_OK);
     }

     attempts++;
     if (attempts <= 2)
     {
	  dev_debug("ds2438_flag_set(%d): Attempt to set flag page 0x%02x "
		    "failed; ROM id %s; will try again",
		    __LINE__, flag, dev->romid);
	  goto try;
     }

     dev_debug("ds2438_flag_set(%d): Attempt to set flag page 0x%02x "
	       "failed twice; failing for device with ROM id %s",
	       __LINE__, flag, dev->romid);

     return(ERR_NO);
}


static int
ds2438_temp_convert(ha7net_t *ctx, device_t *dev)
{
     unsigned char data[9];
     size_t dlen;
     h3r1_t *devx;
     int istat;

     /*
      *  Sanity checks
      */
     if (ERR_OK != (istat = check2("ds2438_temp_convert", ctx, dev, 0, 0,
				   &devx, __LINE__)))
	  return(istat);

     /*
      *  Initiate a temperature conversion
      */
     dev->lastcmd.tv_sec  = 0;
     dev->lastcmd.tv_usec = 0;
     istat = ha7net_writeblock(ctx, dev, NULL, NULL, CONVERT_TEMP, 0);
     if (istat != ERR_OK)
	  goto done_bad;

     /*
      *  Sleep for 10 milliseconds
      */
     if (dev->lastcmd.tv_sec != 0 ||
	 dev->lastcmd.tv_usec != 0)
     {
	  unsigned int dt;
	  struct timeval tv;

	  gettimeofday(&tv, NULL);

	  /* Delta in milliseconds */
	  dt = (tv.tv_sec - dev->lastcmd.tv_sec) * 1000 +
	       (tv.tv_usec - dev->lastcmd.tv_usec) / 1000;

	  if (dt < 10)
	       os_sleep(10 - dt);
     }
     else
	  os_sleep(10);

     /*
      *  Now read the result
      */
     dlen = 0;
     istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
     if (istat != ERR_OK || !dlen)
	  goto done_bad;

     devx->state[2] = data[2];
     devx->state[1] = data[1];

     return(ERR_OK);

done_bad:
     /*  Note, we can end up here with istat == ERR_OK *but* dlen == 0 */
     dev_debug("ds2438_temp_convert(%d): An error occurred while initiating "
	       "an temperature conversion; %d; %s",
	       __LINE__, istat, err_strerror(istat));
     return(istat);
}


static int
ds2438_temp_get(ha7net_t *ctx, device_t *dev, float *temp)
{
     h3r1_t *devx;
     int istat;
     short itemp;
     float tempc;

     /*
      *  Sanity checks
      */
     if (ERR_OK != (istat = check2("ds2438_temp_get", ctx, dev, 0, 0,
				   &devx, __LINE__)))
	  return(istat);

     if (0x80 & devx->state[2])
	  itemp = 0xE000 | ((short)devx->state[2] << 5)
	       | ((short)devx->state[1] >> 3);
     else
	  itemp = ((short)devx->state[2] << 5) | ((short)devx->state[1] >> 3);
     tempc = (float)itemp * 0.03125;

     if (temp)
	  *temp = tempc;

     return(ERR_OK);
}


static int
ds2438_ad_convert(ha7net_t *ctx, device_t *dev, int channel)
{
     int attempts, istat;
     unsigned char data[9];
     h3r1_t *devx;
     size_t dlen;

     if (ERR_OK != (istat = check2("ds2438_ad_convert", ctx, dev, 0, 0,
				   &devx, __LINE__)))
	  return(istat);

     if (channel == CHANNEL_VSENSE)
     {
	  if (!(devx->state[0] & FLAG_IAD))
	  {
	       istat = ds2438_flag_set(ctx, dev, FLAG_IAD, 1, 0, 0);
	       if (istat != ERR_OK)
			   goto done_bad;
	       devx->state[0] |= FLAG_IAD;
	       
	       /*
		*  Need to sleep for ~27.6 milliseconds -- call it 30 ms
		*/
	       os_sleep(30);
	  }
	  dlen = 0;
	  istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
	  if (istat != ERR_OK || !dlen)
	       goto done_bad;
	  devx->state[5] = data[5];
	  devx->state[6] = data[6];
     }
     else if (channel == CHANNEL_VDD || channel == CHANNEL_VAD)
     {
	  istat = ds2438_flag_set(ctx, dev, FLAG_AD,
				  (channel == CHANNEL_VDD) ? 1 : 0,
				  (FLAG_CA | FLAG_IAD), 0);
	  if (istat != ERR_OK)
	       goto done_bad;

	  /*
	   *  Now initiate a voltage conversion
	   */
	  dev->lastcmd.tv_sec  = 0;
	  dev->lastcmd.tv_usec = 0;
	  ha7net_writeblock(ctx, dev, NULL, NULL, CONVERT_VOLT, 0);

	  /*
	   *  Sleep for 10 milliseconds
	   */
	  if (dev->lastcmd.tv_sec != 0 ||
	      dev->lastcmd.tv_usec != 0)
	  {
	       unsigned int dt;
	       struct timeval tv;

	       gettimeofday(&tv, NULL);

	       /* Delta in milliseconds */
	       dt = (tv.tv_sec - dev->lastcmd.tv_sec) * 1000 +
		    (tv.tv_usec - dev->lastcmd.tv_usec) / 1000;

	       if (dt < 10)
		    os_sleep(10 - dt);
	  }
	  else
	       os_sleep(10);

	  attempts = 1;
     read_loop:
	  dlen = 0;
	  istat = ds2438_readpage(ctx, dev, 0, data, &dlen);
	  if (istat != ERR_OK || !dlen)
	       goto done_bad;

	  /*
	   *  See if the A/D conversion had finished
	   */
	  if (data[0] & FLAG_ADB)
	  {
	       /*
		*  ADB flag is still set in the status/config register....
		*/
	       if (++attempts <= 2)
	       {
		    dev_debug("ds2438_ad_convert(%d): A/D conversion not yet "
			      "complete for the device with ROM id \"%s\"; "
			      "will attempt a second read",
			      __LINE__, dev->romid);
		    os_sleep(10);
		    goto read_loop;
	       }
	       dev_debug("ds2438_ad_convert(%d): A/D conversion not yet "
			 "complete for the device with ROM id \"%s\"; "
			 "giving up", __LINE__, dev->romid);
	       return(ERR_CRC);
	  }

	  /*
	   *  Save the voltage info
	   */
	  devx->state[24 + channel*2    ] = data[4];
	  devx->state[24 + channel*2 + 1] = data[3];

	  /*
	   *  Update state info with this data...
	   */
	  memcpy(devx->state, data, dlen);
     }
     else
     {
	  dev_debug("ds2438_ad_convert(%d): Invalid call arguments supplied; "
		    "unrecognized channel value %d specified",
		    __LINE__, channel);
	  return(ERR_BADARGS);
     }

     return(ERR_OK);

done_bad:
     /*  Note, we can end up here with istat == ERR_OK *but* dlen == 0 */
     dev_debug("ds2438_ad_convert(%d): An error occurred while initiating an "
	       "A/D voltage conversion; %d; %s",
	       __LINE__, istat, err_strerror(istat));
     return(istat);
}


static int
ds2438_ad_get(ha7net_t *ctx, device_t *dev, int channel, float *value)
{
     h3r1_t *devx;
     int istat;
     float voltage;

     /*
      *  Sanity checks
      */
     if (ERR_OK != (istat = check2("ds2438_ad_get", ctx, dev, 0, 0, &devx,
				   __LINE__)))
	  return(istat);

     if (channel == CHANNEL_VSENSE)
	  voltage = (float)(((unsigned short)devx->state[6] << 8) |
			   (unsigned short)devx->state[5]) / 4096.0;
     else if (channel == CHANNEL_VDD || channel == CHANNEL_VAD)
	  voltage =
	       (float)((((unsigned short)devx->state[24 + channel*2] << 8)
			& 0x300) |
		       (unsigned short)devx->state[24 + channel*2 + 1]) /100.0;
     else
     {
	  dev_debug("ds2438_ad_convert(%d): Invalid call arguments supplied; "
		    "unrecognized channel value %d specified",
		    __LINE__, channel);
	  return(ERR_BADARGS);
     }

     if (value)
	  *value = voltage;

     return(ERR_OK);
}


static int
h3r1_rh_convert(ha7net_t *ctx, device_t *dev, int got_temp)
{
     int istat;

     /*
      *  We'll let the subroutines do the sanity checks
      */
     if (!got_temp)
     {
	  istat = ds2438_temp_convert(ctx, dev);
	  if (istat != ERR_OK)
	       goto done_bad;
     }

     istat = ds2438_ad_convert(ctx, dev, CHANNEL_VDD);
     if (istat != ERR_OK)
	  goto done_bad;

     istat = ds2438_ad_convert(ctx, dev, CHANNEL_VAD);
     if (istat == ERR_OK)
	  return(ERR_OK);

done_bad:
     dev_debug("h3r1_rh_convert(%d): An error occurred while initiating a "
	       "humidity conversion; %d; %s",
	       __LINE__, istat, err_strerror(istat));
     return(istat);
}

static int
h3r1_rh_get(ha7net_t *ctx, device_t *dev, int got_temp, float tempc, float *rh)
{
     float humidity, vad, vdd;
     int istat;
     char msg[128];

     /*
      *  Initialize the error message buffer
      */
     msg[0] = '\0';

     /*
      *  Let the subroutines do the sanity checks
      */

     if (!got_temp)
     {
	  istat = ds2438_temp_get(ctx, dev, &tempc);
	  if (istat != ERR_OK)
	       goto done_bad;
     }

     istat = ds2438_ad_get(ctx, dev, CHANNEL_VDD, &vdd);
     if (istat != ERR_OK)
	  goto done_bad;

     istat = ds2438_ad_get(ctx, dev, CHANNEL_VAD, &vad);
     if (istat != ERR_OK)
	  goto done_bad;

     if (vdd != 0)
     {
	  humidity = ((vad / vdd) - (0.8 / vdd))
	      / (0.0062 * (1.0305 + tempc * (0.000044 - 0.0000011 * tempc)));
	  humidity = dev->gain * humidity + dev->offset;
     }
     else
     {
	  snprintf(msg, sizeof(msg), "cannot compute the humidity as vdd=0; ");
	  istat = ERR_RANGE;
	  goto done_bad;
     }

//     dev_debug("*** Read humidity of %5.2f; vad=%10.6f; vdd=%10.6f; "
//	       "tempc=%10.6f; gain=%10.6f; offset=%10.6f",
//	       humidity, vad, vdd, tempc, dev->gain, dev->offset);
     if (humidity < -20.0 || humidity > 120.0)
     {
	  /*
	   *  Assume a device read error not caught by the CRCs
	   */
	  snprintf(msg, sizeof(msg), "humidity of %f seems odd; ",
		   humidity);
	  istat = ERR_RANGE;
	  goto done_bad;
     }
     else if (humidity < 0.0)
	  humidity = 0.0;
     else if (humidity > 100.0)
	  humidity = 100.0;

     if (rh)
	  *rh = humidity;

     return(ERR_OK);

done_bad:
     dev_debug("h3r1_rh_get(%d): An error occurred while calculating the "
	       "relative humidity; %s%d; %s",
	       __LINE__, msg, istat, err_strerror(istat));
     return(istat);
}


int
h3r1_done(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     h3r1_t *devx;

     /*
      *  Bozo check
      */
     if (!dev)
     {
	  dev_debug("h3r1_done(%d): Invalid call arguments; dev=NULL",
		    __LINE__);
	  return(ERR_BADARGS);
     }

     devx = (h3r1_t *)dev_private(dev);
     dev_private_set(dev, NULL);
     if (devx)
	  free(devx);

     return(ERR_OK);     
}


int
h3r1_init(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     h3r1_t *devx;
     device_t *ds18s20;
     size_t ref;

     /*
      *  Bozo check
      */
     if (!ctx || !dev)
     {
	  dev_debug("h3r1_init(%d): Invalid call "
		    "arguments; ctx=%p, dev=%p; none of the preceding call "
		    "arguments should be NULL", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_2438)
     {
	  dev_debug("h3r1_init(%d): The device dev=%p with family "
		    "code 0x%02x does not appear to be a DS2438 device "
		    "(0x%02x); the device appears to be a %s",
		    __LINE__, dev, dev_fcode(dev), OWIRE_DEV_2438,
		    dev_strfcode(dev_fcode(dev)));
	  return(ERR_NO);
     }

     /*
      *  Locate our DS18S20 device
      */
     ds18s20 = NULL;
     ref = dev_gref(dev);
     if (!ref)
     {
	  dev_debug("h3r1_init(%d): The configuration does not appear to "
		    "associate a DS18S20 temperature probe device with this "
		    "HBI H3-R1 Humidity Sensor; will use the temperature sensor "
		    "within the onboard DS2438", __LINE__);
     }
     else
     {
	  ds18s20 = devices;
	  while (!dev_flag_test(ds18s20, DEV_FLAGS_END))
	  {
	       if ((dev_gref(ds18s20) == ref) &&
		   (dev_fcode(ds18s20) == OWIRE_DEV_18S20))
		    break;
	       ds18s20++;
	  }
	  if (dev_flag_test(ds18s20, DEV_FLAGS_END))
	  {
	       dev_debug("h3r1_init(%d): The configuration does not appear to "
			 "associate a DS18S20 temperature probe device with "
			 "this HBI H3-R1 Humidity Sensor; will use the "
			 "temperature sensor within the onboard DS2438",
			 __LINE__);
	       ds18s20 = NULL;
	  }
     }

     devx = (h3r1_t *)calloc(1, sizeof(h3r1_t));
     if (!devx)
     {
	  dev_debug("h3r1_init(%d): Insufficient virtual memory", __LINE__);
	  return(ERR_NOMEM);
     }

     /*
      *  Lock down the data structure while we make changes to it
      */
     dev_lock(dev);

     /*
      *  Tie this device specific data into the device's descriptor
      */
     dev_private_set(dev, devx);

     /*
      *  Note who our DS18S20 is
      */
     devx->ds18s20 = ds18s20;

     /*
      *  Data field info
      */
     dev->data.fld_used[0]   = DEV_FLD_USED;
     dev->data.fld_dtype[0]  = DEV_DTYPE_TEMP;
     dev->data.fld_format[0] = h3r1_temp_prec;
     dev->data.fld_units[0]  = DEV_UNIT_C;

     dev->data.fld_used[1]   = DEV_FLD_USED;
     dev->data.fld_dtype[1]  = DEV_DTYPE_RH;
     dev->data.fld_format[1] = h3r1_rhrh_prec;
     dev->data.fld_units[1]  = DEV_UNIT_RH;

     /*
      *  Group the devices together
      */
     if (ds18s20)
	  dev_group("hbi_h3r1", dev, ds18s20, NULL);

     /*
      *  Unlock the structure
      */
     dev_unlock(dev);

     /*
      *  All done
      */
     return(ERR_OK);
}


int
h3r1_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     h3r1_t *devx;
     int got_temp, istat;
     float rh, tempc;
     time_t t0, t1;

     if (!ctx || !dev)
     {
	  dev_debug("h3r1_read(%d): Invalid call arguments supplied; "
		    "ctx=%p, dev=%p", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     devx = (h3r1_t *)dev_private(dev);
     if (!devx || dev_fcode(dev) != OWIRE_DEV_2438)
     {
	  dev_debug("h3r1_read(%d): The device dev=%p with family code 0x%02x "
		    "does not appear to be an HBI H3-R1 Humidity Sensor or "
		    "h3r1_init() has not yet been called for this "
		    "device", __LINE__, dev, dev_fcode(dev));
	  return(ERR_NO);
     }

     t0 = time(NULL);
     got_temp = 0;
     if (devx->ds18s20)
     {
	  /*
	   *  Get a current temperature read for the probe's associated DS18S20
	   *  Use flags of zero so that we don't release the bus lock which
	   *  ha7net_gettemp() will obtain
	   */
	  istat = dev_read(ctx, devx->ds18s20, 0);
	  if (istat == ERR_OK)
	  {
	       /*
		*  Lock down the structure while we alter field values
		*/
	       dev_lock(devx->ds18s20);
	       tempc = devx->ds18s20->data.val[0][devx->ds18s20->data.n_current];
	       dev_unlock(devx->ds18s20);
	       got_temp = 1;
	  }
	  else
	  {
	       dev_debug("h3r1_read(%d): Unable to perform a temperature "
			 "measurement with the HBI Humidity Sensor's associated "
			 "DS18S20 (ROM id \"%s\"); dev_read() returned "
			 "the error %d; %s",
			 __LINE__, dev_romid(devx->ds18s20), istat,
			 err_strerror(istat));
	  }
     }

     /*
      *  Initiate a humidity conversion
      */
     istat = h3r1_rh_convert(ctx, dev, got_temp);
     t1 = time(NULL);
     if (istat != ERR_OK)
	  goto done_bad;

     /*
      *  Now compute the requested quantities
      */
     istat = h3r1_rh_get(ctx, dev, got_temp, tempc, &rh);
     if (istat != ERR_OK)
	  goto done_bad;

     if (!got_temp)
     {
	  istat = ds2438_temp_get(ctx, dev, &tempc);
	  if (istat != ERR_OK)
	       goto done_bad;
     }

     /*
      *  And return the results
      */
     dev_lock(dev);
     dev->data.val[0][dev->data.n_current] = tempc;
     dev->data.val[1][dev->data.n_current] = rh;
     dev->data.time[dev->data.n_current]   = t0 + (int)(difftime(t1, t0)/2.0);
     dev_unlock(dev);

     return(ERR_OK);

done_bad:

     /*
      *  Assume previous values
      */
     dev_lock(dev);
     dev->data.val[0][dev->data.n_current] =
	  dev->data.val[0][dev->data.n_previous];
     dev->data.val[1][dev->data.n_current] =
	  dev->data.val[1][dev->data.n_previous];
     dev->data.time[dev->data.n_current]   = t0 + (int)(difftime(t1, t0)/2.0);
     dev_unlock(dev);

     dev_debug("h3r1_read(%d): Unable to read the device's temperature "
	       "and relative humidity; %d; %s",
	       __LINE__, istat, err_strerror(istat));

     return(istat);
}
