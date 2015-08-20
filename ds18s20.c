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
#include <time.h>
#include "os.h"
#include "device.h"
#include "ha7net.h"
#include "device_ds18s20.h"

static const char *ds18s20_prec  = "%0.1f";

int
ds18s20_init(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     if (!dev)
     {
	  dev_debug("ds18s20_init(%d): Invalid call arguments; dev=NULL",
		    __LINE__);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_18S20)
     {
	  dev_debug("ds18s20_init(%d): The device dev=%p with family code "
		    "0x%02x does not appear to be a thermometer (0x%02x); "
		    "the device appears to be a %s",
		    __LINE__, dev, dev_fcode(dev), OWIRE_DEV_18S20,
		    dev_strfcode(dev_fcode(dev)));
	  return(ERR_NO);
     }

     dev_lock(dev);
     dev->data.fld_used[0]   = DEV_FLD_USED;
     dev->data.fld_dtype[0]  = DEV_DTYPE_TEMP;
     dev->data.fld_format[0] = ds18s20_prec;
     dev->data.fld_units[0]  = DEV_UNIT_C;
     dev_unlock(dev);

     return(ERR_OK);
}


int
ds18s20_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     int attempts, count_per_c, count_remain, i, istat;
     unsigned char data[10];
     unsigned int delay;
     time_t t0, t1;
     float tempc;
     short temp_read;
     static ha7net_crc_t crc = HA7NET_CRC8(1, 8, 0);
  
     if (!ctx || !dev)
     {
	  dev_debug("ds18s20_read(%d): Invalid call arguments; ctx=%p, "
		    "dev=%p; neither may be non-zero", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_18S20)
     {
	  dev_debug("ds18s20_read(%d): The device dev=%p with family code "
		    "0x%02x does not appear to be a thermometer (0x%02x); "
		    "the device appears to be a %s",
		    __LINE__, dev, dev_fcode(dev), OWIRE_DEV_18S20,
		    dev_strfcode(dev_fcode(dev)));
	  return(ERR_NO);
     }

     /*
      *  Tell the DS18S20 to begin a temperature measurement
      */
     t0 = time(NULL);
     istat = ha7net_writeblock(ctx, dev, NULL, NULL, "44", 0);
     if (istat != ERR_OK)
     {
	  dev_debug("ds18s20_read(%d): Unable to initiate a temperature "
		    "conversion; ha7net_writeblock() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Need to wait for upwards of 750 milliseconds
      *
      *  Increased to 1250 milliseconds to try to prevent 85C condition
      */
     os_sleep(1250);

     /*
      *  Now read the scratchpad.  For example
      *     TX: BE FF FF FF FF FF FF FF FF FF
      *     RX: BE 30 00 4B 46 FF FF 10 10 4C
      *             0  1  2  3  4  5  6  7  8
      *  Byte 0 is the temperature LSB
      *  Byte 1 is the temperature MSB
      *  Byte 2 is T_H register (alarm trip high)
      *  Byte 3 is T_L register (alarm trip low)
      *  Byte 4 is reserved (0xFF)
      *  Byte 5 is reserved (0xFF)
      *  Byte 6 is COUNT REMAIN
      *  Byte 7 is COUNT PER DEGREE C (always 0x10)
      *  Byte 8 is 8bit CRC of Bytes 0 - 7
      *
      *  LSB maps as
      *
      *       bit   7   6   5   4   3   2   1    0
      *           2^6 2^5 2^4 2^3 2^2 2^1 2^0 2^-1
      *
      *  MSB maps as each bit indicating sign (i.e., MSB is either
      *  0x00 indicating positive sign or 0xFF indicating negative
      *  sign).
      *
      *      s = (MSB & 1) ? -1 : +1
      *
      *      TEMP_READ = s * (LSB >> 1)
      *
      *                                COUNT_PER_DEGREE_C - COUNT_REMAIN
      *      Temp = TEMP_READ - 0.25 + ---------------------------------
      *                                       COUNT_PER_DEGREE_C
      *                                
      */
     attempts = 1;
loop:
     istat = ha7net_writeblock_ex(ctx, dev, data, 10, "BEFFFFFFFFFFFFFFFFFF",
				  &crc, 0);
     if (attempts == 1)
	  t1 = time(NULL);
     if (istat != ERR_OK)
     {
	  dev_debug("ds18s20_read(%d): Unable to read the device's scratch "
		    "pad; ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Another error test is to see that data[2] == 0xff | data[2] == 0x00
      */
     if (data[2] == 0x00 || data[2] == 0xff)
	  goto data_okay;

     dev_detail("ds18s20_read(%d): Read of DS18S20 device with ROM id "
		"\"%s\" failed with bad MSB=0x%02x; will attempt "
		"another read", __LINE__, dev->romid, data[2]);
 
    if (++attempts <= 2)
	  goto loop;

     dev_detail("ds18s20_read(%d): Read of DS18S20 device with ROM id "
		"\"%s\" has failed", __LINE__, dev->romid);

     return(ERR_CRC);

     /*
      *  Now compute the temperature
      */
data_okay:
     temp_read = (data[2] << 8) | data[1];
     temp_read >>= 1;

     if (temp_read == 85)
     {
	  /*
	   *  Problem reading the thermometer -- we got the max value
	   *  This means either we didn't wait long enough or we have
	   *  a problem with the parasitic power.
	   */
	  if (++attempts <= 2)
	  {
	       dev_detail("ds18s20_read(%d): Received 85C temp from DS18S20 "
			  "with ROM id \"%s\"; will attempt another read",
			  __LINE__, dev->romid);
	       goto loop;
	  }
	  dev_detail("ds18s20_read(%d): Received 85C temp from DS18S20 "
		     "with ROM id \"%s\"; giving up for now",
		     __LINE__, dev->romid);
	  return(ERR_CRC);
     }

     count_remain = data[7];
     count_per_c  = data[8];
     tempc = (float)temp_read - 0.25 +
	          ((float)(count_per_c - count_remain) / (float)count_per_c);

     dev_lock(dev);
     dev->data.val[0][dev->data.n_current]  = tempc;
     dev->data.time[dev->data.n_current] = t0 + (int)(difftime(t1, t0)/2.0);
     dev_unlock(dev);

     return(ERR_OK);
}
