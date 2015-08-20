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
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "utils.h"
#include "os.h"
#include "device.h"
#include "ha7net.h"
#include "eds_aprobe.h"

static const char *eds_rhrh_name  = "eds_rhrh";
static const char *eds_rhrh_prec  = "%0.f";

static const char *eds_pres_name  = "eds_pres";
static const char *eds_pres_prec  = "%0.2f";

static const char *eds_temp_name  = "eds_temp";
static const char *eds_temp_prec  = "%0.1f";

static const char *eds_prob_name  = "eds_probe";
static const char *eds_prob_prec  = "%f";

static int parseAnalogData(unsigned char hb, unsigned char lb);
static float CDec(const char *str, size_t len);
static void ParseSerial(char *dst, unsigned char *src);
static void getEDSAnalogProbeDeviceType(char *dst, const unsigned char *src);

/*
 *  Transform a 12bit integer in ones compliment arithmetic into
 *  something that "computes"
 *
 *  hb = high byte
 *  lb = low byte
 */
static int
parseAnalogData(unsigned char hb, unsigned char lb)
{
     int i, retval;

     /*
      *  Negate ...
      */
     lb = ~lb;
     hb = ~hb;

     /*
      *  ... and flip
      */
     retval = 0;
     for (i = 0; i < 8; i++)
     {
	  retval= ( retval << 1 ) | ( hb & 0x01 );
	  hb >>= 1;
     }
     for (i = 0; i < 4; i++)
     {
	  retval = (retval << 1 ) | ( lb & 0x01 );
	  lb >>= 1;
     }

     /*
      *  All done
      */
     return(retval);
}


static float
CDec(const char *str, size_t len)
{
     unsigned char c;
     float dec, val;
     int dotseen;
     size_t i;
     const float dec2byte[256] = {
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

     val =  0.0;
     dec = 10.0;
     dotseen = 0;
     for (i = 0; i < len; i++)
     {
	  if ('.' != (c = (unsigned char)*str++))
	  {
	       if (!dotseen)
		    val = val*10 + dec2byte[c];
	       else
	       {
		    val += dec2byte[c] / dec;
		    dec *= 10.0;
	       }
	  }
	  else
	       dotseen = 1;
     }
     return(val);
}


static void
ParseSerial(char *dst, unsigned char *src)
{
     size_t i;
     const char byte2hex[] = "0123456789ABCDEF";

     for (i = 0; i < 8; i++)
     {
	  *dst++ = byte2hex[(*src) >> 4];
	  *dst++ = byte2hex[(*src++) & 0x0f];
     }
     *dst = '\0';
}


static void
getEDSAnalogProbeDeviceType(char *dst, const unsigned char *src)
{
     if (!memcmp(src + 10, "#M5Z", 4))
     {
	  memcpy(dst, src + 1, 4);
	  dst[4] = '\0';
     }
     else
	  dst[0] = '\0';
}


int
eds_aprobe_done(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     eds_aprobe_t *devx;

     /*
      *  Bozo check
      */
     if (!ctx || !dev)
     {
	  dev_debug("eds_aprobe_done(%d): Invalid call arguments; ctx=%p, "
		    "dev=%p; neither may be non-zero", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     devx = (eds_aprobe_t *)dev_private(dev);
     if (devx)
	  free(devx);
     dev_private_set(dev, NULL);
     dev_ungroup(dev);
     return(ERR_OK);
}


int
eds_aprobe_init(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     unsigned char data[128+1], *ptr;
     char device_ctype[5], serialno[16+1], temp_data[256], *tmp;
     int device_type, istat;
     eds_aprobe_t *devx;
     device_t *ds18s20;
     const char *gname;

     if (!ctx || !dev)
     {
	  dev_debug("eds_aprobe_init(%d): Invalid call "
		    "arguments; ctx=%p, dev=%p; none of the preceding call "
		    "arguments should be NULL", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_2406)
     {
	  dev_debug("eds_aprobe_init(%d): The device dev=%p with family "
		    "code 0x%02x does not appear to be a DS2406 device "
		    "(0x%02x); the device appears to be a %s",
		    __LINE__, dev, dev_fcode(dev), OWIRE_DEV_2406,
		    dev_strfcode(dev_fcode(dev)));
	  return(ERR_NO);
     }

     istat = ha7net_readpages_ex(ctx, dev, data, 4 * 32, 0, 4, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("eds_aprobe_init(%d): Unable to read the OTP data from "
		    "the DS2406 device with ROM id \"%s\"; "
		    "ha7net_readpages_ex() returned %d; %s",
		    __LINE__, dev_romid(dev), istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Get the probe type (e.g., "RHRH" = Relative Humidity)
      */
     getEDSAnalogProbeDeviceType(device_ctype, data + 32);
     if (!device_ctype[0])
     {
	  /*
	   *  This isn't an EDS Analog Probe
	   */
	  dev_debug("eds_aprobe_init(%d): The DS2406 device with ROM id "
		    "\"%s\" does not appear to be an EDS Analog Probe "
		    "device; it does not have the magic string \"#M5Z\" "
		    "starting at byte 32 of page 0 of its OTP data",
		    __LINE__, dev_romid(dev));
	  istat = ERR_EOM;
	  goto done;
     }
     if (!memcmp(device_ctype, "RHRH", 4))
	  device_type = EDS_RHRH;
     else if (!memcmp(device_ctype, "PRES", 4))
	  device_type = EDS_PRES;
     else if (!memcmp(device_ctype, "AOUT", 4))
	  device_type = EDS_AOUT;
     else
	  device_type = EDS_OTHER;

     /*
      *  Extract the device ID of the associated DS18S20
      */
     ParseSerial(serialno, data + 64);
     dev_romid_cannonical(serialno, sizeof(serialno),
			  serialno, strlen(serialno));

     /*
      *  Locate this device in the device list
      */
     ds18s20 = devices;
     while(!dev_flag_test(ds18s20, DEV_FLAGS_END))
     {
	  if (!strcmp(dev_romid(ds18s20), serialno))
	       break;
	  ds18s20++;
     }
     if (dev_flag_test(ds18s20, DEV_FLAGS_END))
     {
	  dev_debug("eds_aprobe_init(%d): The associated DS18S20 "
		    "temperature probe does not exist in the supplied "
		    "device list; if the list was generated with "
		    "ha7net_search() then perhaps devices with the family "
		    "code 0x%02x were excluded from the search?",
		    __LINE__, OWIRE_DEV_18S20);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Looks like this DS2406 is indeed an EDS Analog Probe device
      */

     /*
      *  Allocate storage for device specific data
      */
     devx = (eds_aprobe_t *)calloc(1, sizeof(eds_aprobe_t));
     if (!devx)
     {
	  dev_debug("eds_aprobe_init(%d): Insufficient virtual memory",
		    __LINE__);
	  istat = ERR_NOMEM;
	  goto done;
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
      *  EDS Analog Probe type information
      */
     devx->device_type = device_type;
     memcpy(devx->device_ctype, device_ctype, sizeof(device_ctype));

     /*
      *  Data field info
      */
     dev->data.fld_used[0]   = DEV_FLD_USED;
     dev->data.fld_used[1]   = DEV_FLD_USED;
     dev->data.fld_dtype[0]  = DEV_DTYPE_TEMP;
     dev->data.fld_format[0] = eds_temp_prec;
     dev->data.fld_units[0]  = DEV_UNIT_C;
     switch(device_type)
     {
     case EDS_RHRH :
	  gname                   = eds_rhrh_name;
	  dev->data.fld_dtype[1]  = DEV_DTYPE_RH;
	  dev->data.fld_format[1] = eds_rhrh_prec;
	  dev->data.fld_units[1]  = DEV_UNIT_RH;
	  break;

     case EDS_PRES :
	  gname                   = eds_pres_name;
	  dev->data.fld_dtype[1]  = DEV_DTYPE_PRES;
	  dev->data.fld_format[1] = eds_pres_prec;
	  dev->data.fld_units[1]  = DEV_UNIT_INHG;
	  break;

     default :
	  gname                   = eds_prob_name;
	  dev->data.fld_dtype[1]  = DEV_DTYPE_UNKNOWN;
	  dev->data.fld_format[1] = eds_prob_prec;
	  dev->data.fld_units[1]  = DEV_UNIT_UNKNOWN;
	  break;
     }

     /*
      *  Group the devices together if they are not already
      */
     dev_group(gname, dev, ds18s20, NULL);

     /*
      *  Extract the first calibration point
      */
     ptr = data + 79;

     /*
      *  Although not documented, it appears that
      *
      *  bytes 77 & 78 yield the recommended dwell time in milliseconds
      *        76 are flags
      *        75 is the recommended number of samples
      */
     devx->nsamples = (int)ptr[-4];
     devx->flags    = (int)ptr[-3];
     devx->dwell = (unsigned int)((ptr[-2] << 8) | ptr[-1]);
     if (devx->dwell < 100)
	  devx->dwell = 100;

     /*
      *  Loop while the high bit is clear
      */
     tmp = temp_data;
     while (!((*ptr) & 0x80))
	  *tmp++ = *ptr++;
     *tmp++ = (*ptr++) & 0x7f;
     devx->calib1_eng = CDec(temp_data, tmp - temp_data);

     /*
      *  Parse the raw calibration point
      */
     devx->calib1_raw = ((int)ptr[0] << 8) | (int)ptr[1];
     ptr += 2;

     /*
      *  Second calibration point
      */
     tmp = temp_data;
     while (!((*ptr) & 0x80))
	  *tmp++ = *ptr++;
     *tmp++ = (*ptr++) & 0x7f;
     devx->calib2_eng = CDec(temp_data, tmp - temp_data);

     /*
      *  Parse the raw calibration point
      */
     devx->calib2_raw = ((int)ptr[0] << 8) | (int)ptr[1];
     ptr += 2;

     /*
      *  Temperature calibration
      */
     tmp = temp_data;
     while (!((*ptr) & 0x80))
	  *tmp++ = *ptr++;
     *tmp++ = (*ptr++) & 0x7f;
     devx->temp_calib = CDec(temp_data, tmp - temp_data);

     /*
      *  Temperature coefficient
      */
     devx->temp_coeff = ((int)ptr[0] << 8) | (int)ptr[1];

     /*
      *  Derived values used to generate engineering values from probe
      *  readings
      */
     devx->scale =
	  (devx->calib2_eng - devx->calib1_eng) / 
	  (float)(devx->calib2_raw - devx->calib1_raw);

     devx->offset = devx->calib1_eng -
	  ((float)devx->calib1_raw * devx->scale);

     /*
      *  Note that the probe's calibration data has been read
      */
     devx->device_state = EDS_INITDONE;

     /*
      *  Unlock the structure
      */
     dev_unlock(dev);

     /*
      *  Success!
      */
     istat = ERR_OK;

done:
     return(istat);
}


int
eds_aprobe_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     int analog_value, istat;
     float analog_value_temp_compensated, eng_val, last_temp, offset, scale;
     unsigned char data[14];
     eds_aprobe_t *devx;
     time_t t0, t1;
     static ha7net_crc_t crc = HA7NET_CRC16(0, 12, 0);

     /*
      *  Bozo check
      */
     if (!ctx || !dev)
     {
	  dev_debug("eds_aprobe_read(%d): Invalid call arguments; ctx=%p, "
		    "dev=%p; neither may be non-zero", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     devx = (eds_aprobe_t *)dev_private(dev);
     if (!devx || !devx->ds18s20 || dev_fcode(dev) != OWIRE_DEV_2406)
     {
	  dev_debug("eds_aprobe_read(%d): The device dev=%p with family "
		    "code 0x%02x does not appear to be an EDS Analog Probe "
		    "or eds_aprobe_init() has not yet been called for this "
		    "device", __LINE__, dev, dev_fcode(dev));
	  return(ERR_NO);
     }

     /*
      *  Get a current temperature read for the probe's associated DS18S20
      *  Use flags of zero so that we don't release the bus lock which
      *  ha7net_gettemp() will obtain
      */
again:
     istat = dev_read(ctx, devx->ds18s20, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("eds_aprobe_read(%d): Unable to perform a temperature "
		    "measurement with the EDS Analog Probe's associated "
		    "DS18S20 (ROM id \"%s\"); ha7net_gettemp() returned "
		    "the error %d; %s",
		    __LINE__, dev_romid(devx->ds18s20), istat,
		    err_strerror(istat));
	  return(istat);
     }

     /*
      *  Lock down the structure while we alter field values
      */

     last_temp = devx->ds18s20->data.val[0][devx->ds18s20->data.n_current];
     dev_lock(dev);
     dev->data.val[0][dev->data.n_current] = last_temp;
     dev_unlock(dev);

     /*
      *  Now, warm up the DS2406.  Note that the writeblock will reset
      *  the 1-Wire bus and then select the DS2406.  We'll use the bus
      *  lock from the prior gettemp call.
      */
     t0 = time(NULL);
     istat = ha7net_writeblock_ex(ctx, dev, data, 14,
				  "F5A6FFFFFEFFFFFFFFFFFFFFFFFF", &crc, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("eds_aprobe_read(%d): Unable to initiate a conversion; "
		    "ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  if (istat == ERR_CRC)
	       dev_debug("eds_aprobe_read(%d): Read data was 0x"
			 "%02x%02x%02x%02x%02x%02x%02x"
			 "%02x%02x%02x%02x%02x%02x%02x",
			 __LINE__,
			 data[0], data[1], data[2], data[3], data[4],
			 data[5], data[6], data[7], data[8], data[9],
			 data[10], data[11], data[12], data[13]);
	  return(istat);
     }

     /*
      *  Need to wait for devx->dwell milliseconds
      */
     os_sleep(devx->dwell);

     /*
      *  Now repeat the exercise, but let's pay attention to the result
      *  this time.
      *
      *  TX: F5 A6 FF FF FE FF FF FF FF FF FF FF FF FF
      *  RX: F5 A6 FF C7 FE FF FF FF FF BC FF F0 DE 26
      *                a                 b     c  e  f
      *
      *   a - DS2406 Channel Info byte
      *   b - Analog value high byte (1's complement)
      *   c - Analog value low byte  (1's complement)
      *   e - CRC16 high byte
      *   f - CRC16 low byte
      */
     istat = ha7net_writeblock_ex(ctx, dev, data, 14,
				  "F5A6FFFFFEFFFFFFFFFFFFFFFFFF", &crc, 0);
     t1 = time(NULL);
     if (istat != ERR_OK)
     {
	  dev_debug("eds_aprobe_read(%d): Unable to read the device's "
		    "scratch pad; ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  if (istat == ERR_CRC)
	       dev_debug("eds_aprobe_read(%d): Read data was 0x"
			 "%02x%02x%02x%02x%02x%02x%02x"
			 "%02x%02x%02x%02x%02x%02x%02x",
			 __LINE__,
			 data[0], data[1], data[2], data[3], data[4],
			 data[5], data[6], data[7], data[8], data[9],
			 data[10], data[11], data[12], data[13]);
	  return(istat);
     }

     /*
      *  If this is the first read of this probe, then go back and do it
      *  again!
      */
     if (devx->device_state < EDS_READONCE)
     {
#if 0
	  dev_info("eds_aprobe_read(%d): Since this was the "
		   "first read of this EDS Analog Probe, we're "
		   "going to do a second read.", __LINE__);
#endif
	  /*
	   *  We're the only one who looks at this field, so no need
	   *  to lock it down
	   */
	  devx->device_state = EDS_READONCE;
	  goto again;
     }

     /*
      *  Now, extract the data which is in two bytes & 1's complement
      */
     analog_value = parseAnalogData(data[9], data[11]);

     /*
      *  Temperature compensated value
      */
     analog_value_temp_compensated = 
	  (((float)analog_value *
	    (1.0 + (last_temp - devx->temp_calib) * 
	     ((float)devx->temp_coeff / 1000000.0))));

     eng_val = (analog_value_temp_compensated * devx->scale) + devx->offset;

#if 0
     eng_val = ((float)analog_value * devx->scale) + devx->offset;
     eng_val = eng_val *
	  (1.0 + (last_temp - devx->temp_calib) * 
	   ((float)devx->temp_coeff / 1000000.0));
#endif

     dev_lock(dev);
     dev->data.val[1][dev->data.n_current] = eng_val;
     dev->data.time[dev->data.n_current] = t0 + (int)(difftime(t1, t0)/2.0);
     dev_unlock(dev);

     /*
      *  Yeah
      */
     return(ERR_OK);
}


int
eds_aprobe_show(ha7net_t *ctx, device_t *dev, unsigned int flags,
		device_proc_out_t *proc, void *out_ctx)
{
     eds_aprobe_t *devx;

     if (!dev || !proc)
	  return(ERR_BADARGS);

     if (!(devx = (eds_aprobe_t *)dev_private(dev)))
     {
	  (*proc)(out_ctx,
"The device does not appear to be initialized: the private device field\n"
"is NULL.\n");
	  return(ERR_NO);
     }
     else if (!devx->ds18s20)
     {
	  (*proc)(out_ctx,
"The device does not appear to be initialized: the associated DS18S20\n"
"device has not yet been identified.\n");
	  return(ERR_NO);
     }
     else if (devx->device_state < EDS_INITDONE)
     {
	  (*proc)(out_ctx, "The device does not appear to be initialized.\n");
	  return(ERR_NO);
     }

     (*proc)(out_ctx,
"EDS Analog Probe\n"
"  Probe type = %s\n"
"     DS18S20 = %s\n"
"       Dwell = %u milliseconds\n"
"     Samples = %d (recommended)\n"
"       Flags = %u\n"
"\n"
"  Calibration data:\n"
"\n"
"    Calib1 Eng = %f\n"
"    Calib1 Raw = %d\n"
"    Calib2 Eng = %f\n"
"    Calib2 Raw = %d\n"
"    Temp calib = %f C\n"
"    Temp coeff = %d\n"
"         Scale = %f (derived)\n"
"        Offset = %f (derived)\n",
	     devx->device_ctype, dev_romid(devx->ds18s20),
	     devx->dwell, devx->nsamples, devx->flags,
	     devx->calib1_eng, devx->calib1_raw, devx->calib2_eng,
	     devx->calib2_raw, devx->temp_calib, devx->temp_coeff,
	     devx->scale, devx->offset);

     return(ERR_OK);
}
