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
#include "device.h"
#include "ha7net.h"
#include "device_tai_8570.h"

static const char *tai_8570_name       = "TAI8570";

static const char *tai_8570_pres_prec  = "%0.1f";
static const char *tai_8570_temp_prec  = "%0.1f";

/*
 *  SEQUENCES WE WRITE TO THE TAI 8570
 *
 *  Write a 1 bit to the MS5534, we put on the wire the bit
 *     sequence 0, 1, 1, 1, 0, 0, 0, 0.  That means we are
 *     sending a 0x0E (the lowest bit is sent first, not the highest).
 *
 *  To write a 0 bit, we send 0, 0, 1, 0, 0, 0, 0, 0 which corresponds to 0x04
 */

/* NOTE: Figure 4e in the TAI8570 Data sheet shows two bit number 13s and   */
/*       suggest 22 bits total. However, the text speaks of 21 bits.  The   */
/*       Intersema MS5534 spec sheet shows the sequence correctly and with  */
/*       21 bits.                                                           */

/*  Reset:                       1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0  */
static const char *cmd_reset = "0E040E040E040E040E040E040E040E040404040404";

/*  Start sequence: 1 1 1  */
#define CMD_START "0E0E0E"

/*  Stop sequence:  0 0 0 */
#define CMD_STOP  "040404"

/*
 *  If you carefully look at the timing figures, you will see that when
 *  reading Words 1 - 4, one additional clock at SCLK is needed...  To
 *  bring about this additional clock, we send one final zero bit as
 *  represented by the final "04" at the end of each of the following
 *  command sequences.
 */

/*  Read Word 1 (calibration W1):            0 1 0 1 0 1             0      */
static const char cmd_readw1[] = CMD_START "040E040E040E" CMD_STOP "04";

/*  Read word 2 (calibration W2):            0 1 0 1 1 0             0      */
static const char cmd_readw2[] = CMD_START "040E040E0E04" CMD_STOP "04";

/*  Read word 3 (calibration W3):            0 1 1 0 0 1             0      */
static const char cmd_readw3[] = CMD_START "040E0E04040E" CMD_STOP "04";

/*  Read word 4 (calibration W4):            0 1 1 0 1 0             0      */
static const char cmd_readw4[] = CMD_START "040E0E040E04" CMD_STOP "04";

/*
 *  As spelled out in the spec sheets, two additional clocks are needed
 *  at SCLK in order for the conversion to proceed correctly.  Hence the
 *  two additional zero bits written by the commands below.  To effect
 *  those two addtional bits, we have tacked "0404" on to the end of each
 *  command sequence.
 */

/*  Read D1 (pressure, mbar):                1 0 1 0             0 0         */
static const char cmd_readd1[] = CMD_START "0E040E04" CMD_STOP "0404";

/*  Read D2 (temperature, C):                1 0 0 1             0 0         */
static const char cmd_readd2[] = CMD_START "0E04040E" CMD_STOP "0404";

static const char *cmd_readw[4] = {
     cmd_readw1, cmd_readw2, cmd_readw3, cmd_readw4};

#define CHANNEL_ACCESS "F5"   /* Channel access command                     */
#define CFG_READW      "EEFF" /* Channel Control word for the read DS2406   */
#define CFG_WRITW      "8CFF" /* Channel Control word for the write DS2406  */

static const char *cfg_read      = CHANNEL_ACCESS CFG_READW "FF";
static size_t      cfg_read_len  = 4;

static const char *cfg_write     = CHANNEL_ACCESS CFG_WRITW "FF";
static size_t      cfg_write_len = 4;

typedef struct {
     device_t *rdev;   /* Read DS2406                                       */
     device_t *wdev;   /* Write DS2406                                      */
     int ignore_state; /* State of the ignore bit of the partner DS2406 dev */
     int c1;  /* Pressure sensitivity, senst1                    (15 bits)  */
     int c2;  /* Pressure offset, offt1                          (12 bits)  */
     int c3;  /* Temp. coefficient of pressure sensitivity, tcs  (10 bits)  */
     int c4;  /* Temp. coefficient of pressure offset, tco       (10 bits)  */
     int c5;  /* Reference temp., tref                           (11 bits)  */
     int c6;  /* Temp. coefficient of the temp. [?], tempsens    ( 6 bits)  */
     int ut1; /* Calibration temp = 8*c5 + 20224                            */
} tai_8570_t;


/*
 *  Compute a corrected temperature as per the Intersema MS5534a data sheet
 *  (DA5534_022.doc, ECN493, 17 July 2002)
 */

static float
tai_8570_temp_calc(float *dt_, unsigned char hi_b, unsigned char lo_b, int c6,
		   int ut1)
{
     int d2;
     float dt, t;

     d2 = ((int)hi_b << 8) | (int)lo_b;

     if (d2 >= ut1)
     {
	  /*
	   *  First order correction:
	   *
	   *    dt = d2 - ut1
	   *     t = ( 200 + dt (c6 + 50) / 2^10 ) / 10
	   *       = 20 + dt (c6 + 50) / 10240
	   */
	  dt = (float)(d2 - ut1);
	  t = 20.0 + dt * (float)(c6 + 50) / 10240.0;
     }
     else	  
     {
	  /*
	   *  Second order correction:
	   *
	   *    dt = (d2 - ut1) - ((d2 - ut1) 2^-7) * ((d2 - ut1) 2^-7) * 2^-2
	   *       = (d2 - ut1) - (d2 - ut1)^2 * 2^-16
	   *     t = ( 200 + dt (c6 + 50) * 2^-10 + dt * 2^-8 ) / 10
	   */
	  dt = (float)(d2 - ut1);
	  dt = dt - dt*dt/65536.0;
	  t = 20.0 + dt * (float)(c6 + 50) / 10240.0 + dt / 2560.0;
     }
     if (dt_)
	  *dt_ = dt;
     return(t);
}


/*
 *  Compute a temperature corrected pressure as per the Intersema MS5534a
 *  data sheet (DA5534_022.doc, ECN493, 17 July 2002)
 */

static float
tai_8570_pres_calc(unsigned char hi_b, unsigned char lo_b, float dt,
		   int c1, int c2, int c3, int c4)
{
     int d1;
     float off, p, sens, x;

     d1 = ((int)hi_b << 8) | (int)lo_b;

     off = (float)(c2 * 4) + (float)(c4 - 512) * dt / 4096.0;
     sens = (float)c1 + (float)c3 * dt / 1024.0 + 24576.0;
     x = sens * (float)(d1 - 7168) / 16384.0 - off;
     p = x / 32.0 + 250.0;
     return(p);
}


static int
tai_8570_assert_pio(ha7net_t *ctx, device_t *dev, int pio_a)
{
     char cmd[16];
     unsigned char data[15];
     int istat;
     ha7net_crc_t crc = HA7NET_CRC16(0, 12, 0);
     static const char *hex = "0123456789ABCDEF";

     if (dev_dotrace)
	  dev_trace("tai_8570_assert_pio(%d): Called with ctx=%p; dev=%p, "
		    "pio_a=%d", __LINE__, ctx, dev, pio_a);

     /*
      *  Check our inputs
      */
     if (!ctx || !dev)
     {
	  dev_debug("tai_8570_assert_pio(%d): Invalid call arguments "
		    "supplied; ctx=%p, dev=%p; neither may be NULL",
		    __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     /*
      *  Read the status byte from register 7
      */
     istat = ha7net_writeblock_ex(ctx, dev, data, 4, "AA0700FF", NULL, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_assert_pio(%d): Unable to read the DS2406's "
		    "status register; ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Check to see if the specified PIO channel is already active.
      *  If it is, then we don't need to assert it and can save an HTTP
      *  round-trip.  So doing eliminates 16(!) ha7net_writeblock_ex()
      *  calls when initializing a TAI 8570 with tai_8570_init().
      */
     if (pio_a)
     {
	  if (data[3] & 0x20)
	       /*
		*  PIO A is already asserted
		*/
	       return(ERR_OK);
	  data[3] |= 0x20;
     }
     else
     {
	  if (data[3] & 0x40)
	       /*
		*  PIO B is already asserted
		*/
	       return(ERR_OK);
	  data[3] |= 0x40;
     }

     /*
      *  Selected PIO channel needs to be asserted...
      */
     strcpy(cmd, "550700xxFFFF");
     cmd[6] = hex[data[3] >> 4];
     cmd[7] = hex[data[3] & 0x0f];
     istat = ha7net_writeblock_ex(ctx, dev, data, 6, cmd, NULL, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_assert_pio(%d): Unable to read the DS2406's "
		    "status register; ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     return(istat);
}


static int
tai_8570_write(ha7net_t *ctx, tai_8570_t *devx, const char *cmd)
{
     char buf[64], *ptr;
     int istat;
     size_t len;

     if (dev_dotrace)
	  dev_trace("tai_8570_write(%d): Called with ctx=%p, devx=%p, "
		    "cmd=\"%s\" (%p)",
		    __LINE__, ctx, devx, cmd ? cmd : "(null)", cmd);

     /*
      *  Check our inputs
      */
     if (!ctx || !devx || !cmd)
     {
	  dev_debug("tai_8570_write(%d): Invalid call arguments supplied; "
		    "ctx=%p, devx=%p, cmd=%p; all must be non-NULL",
		    __LINE__, ctx, devx, cmd);
	  return(ERR_BADARGS);
     }

     /*
      *  Initializations
      */
     ptr = NULL;

     /*
      *  Build the full command
      */
     len = strlen(cmd);
     if ((cfg_write_len + len) >= sizeof(buf))
     {
	  ptr = (char *)malloc(cfg_write_len + len + 1);
	  if (!ptr)
	  {
	       dev_debug("tai_8570_write(%d): Insufficient virtual memory",
			 __LINE__);
	       return(ERR_NOMEM);
	  }
     }
     else
	  ptr = buf;

     /*
      *  The cfg_write prefix is Channel Access Command followed
      *  by two Channel Control bytes which
      */
     strcpy(ptr, cfg_write);
     strcat(ptr, cmd);

     /*
      *  Open PIO A on both the DS2406 reader and writer
      */
     istat = tai_8570_assert_pio(ctx, devx->rdev, 1);
     if (istat == ERR_OK)
	  istat = tai_8570_assert_pio(ctx, devx->wdev, 1);
     if (istat != ERR_OK)
	  goto done;

     /*
      *  Send the command
      */
     istat = ha7net_writeblock(ctx, devx->wdev, NULL, NULL, ptr, 0);
     if (istat != ERR_OK)
	  goto done;

     /*
      *  Re-open PIO A on the DS2406 writer
      */
     istat = tai_8570_assert_pio(ctx, devx->wdev, 1);

     /*
      *  Clean up
      */
done:
     if (ptr && ptr != buf)
	  free(ptr);

     /*
      *  And return
      */
     return(istat);
}


static int
tai_8570_reset(ha7net_t *ctx, tai_8570_t *devx)
{
     int istat;

     if (dev_dotrace)
	  dev_trace("tai_8570_write(%d): Called with ctx=%p, devx=%p",
		    __LINE__, ctx, devx);

     /*
      *  Check our inputs
      */
     if (!ctx || !devx)
     {
	  dev_debug("tai_8570_reset(%d): Invalid call arguments supplied; "
		    "ctx=%p, devx=%p; all must be non-NULL",
		    __LINE__, ctx, devx);
	  return(ERR_BADARGS);
     }

     istat = tai_8570_write(ctx, devx, cmd_reset);
     if (istat == ERR_OK)
	  return(ERR_OK);

     return(istat);
}


static int
tai_8570_readp(ha7net_t *ctx, tai_8570_t *devx, unsigned char *hi_b,
	       unsigned char *lo_b, const char *cmd, unsigned int sleep)
{
     unsigned char data[44], *ptr1, *ptr2, umask, uval1, uval2;
     int i, istat;
     /*
      *  To read a single bit, we send a sequence of 16 (!) bits:
      *
      *   1 x 8, 0, 1, 0, 1 x 5
      *
      *  which corresponds to a 0xFF followed by a 0xFA.  Sooo, for
      *  each bit we wish to read we write 0xFF followed by 0xFA.
      *  But, the HA7Net's WriteBlock.html only allows 32 bytes per
      *  call and we need to write 4 bytes of channel configuration
      *  info followed by 32 bytes of 0xFF 0xFA, and that's not
      *  including additional bytes to receive CRC16 data....
      *
      *  The following two strings, read_seq1 and read_seq2, are
      *  what we need to configure the read DS2406, and then read
      *  16 bits of data *with* CRCs.
      */
     static ha7net_crc_t crc1 = HA7NET_CRC16(0, 12, 8);
     static const char read_seq1[48+1] =
	  CHANNEL_ACCESS CFG_READW "FF"
	  "FFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF";
     /*     1 2 3 4 5 6 7 8 CRC 1 2 3 4 5 6 7 8 CRC   */
     static ha7net_crc_t crc2 = HA7NET_CRC16(0, 8, 8);
     static const char read_seq2[40+1] =
	  "FFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF";
     /*     1 2 3 4 5 6 7 8 CRC 1 2 3 4 5 6 7 8 CRC   */

     if (dev_dotrace)
	  dev_trace("tai_8570_readp(%d): Called with ctx=%p, devx=%p, "
		    "hi_b=%p, lo_b=%p, cmd=\"%s\" (%p), sleep=%u",
		    __LINE__, ctx, devx, hi_b, lo_b, cmd ? cmd : "(null)",
		    cmd, sleep);

     /*
      *  Check our inputs
      */
     if (!ctx || !devx || !cmd)
     {
	  dev_debug("tai_8570_readp(%d): Invalid call arguments supplied; "
		    "ctx=%p, devx=%p, cmd=%p; all must be non-NULL",
		    __LINE__, ctx, devx, cmd);
	  return(ERR_BADARGS);
     }

     /*
      *  Step 1: Send the command to the DS2406 writer
      *          Note that tai_8570_write() will open PIO A and B
      *          on both DS2406s as well as putting the DS2406 writer
      *          into write mode.  It will, additionally, re-open
      *          PIO A on the DS2406 writer after the data is written.
      */
     istat = tai_8570_write(ctx, devx, cmd);
     if (istat != ERR_OK)
	  return(istat);

     /*
      *  Sleep if so instructed
      */
     if (sleep)
	  os_sleep(sleep);

     /*
      *  Step 2: Open PIO B on the DS2406 reader
      */
     istat = tai_8570_assert_pio(ctx, devx->rdev, 0);
     if (istat != ERR_OK)
	  return(istat);

     /*
      *  Step 3: put the DS2406 reader into read mode and read back
      *          the results.  The last write will have ensured
      *          that the DS2406 writer ends up with PIO A high.  The
      *          command we're about to send to the DS2406 reader
      *          will assert PIO A and B on it.
      *
      *          The gotcha here is that we are always reading back 16 bits
      *          but for each bit read we need to send 16 (!!!) bits to
      *          the DS2406 reader.  Well, 16 x 16 = 256 bits = 32 bytes.
      *          But 32 bytes is the maximum amount we can put into a
      *          HA7Net WriteBlock request.  Thus there's no room for the
      *          three additional bytes to put the DS2406 reader into
      *          the correct mode. Soooo, we need to do two WriteBlocks().
      */
     istat = ha7net_writeblock_ex(ctx, devx->rdev, data, 24, read_seq1, 
				  &crc1, 0);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_readp(%d): Unable to put the DS2406 reader into "
		    "read mode; ha7net_writeblock() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     istat = ha7net_writeblock_ex(ctx, NULL, data + 24, 20, read_seq2,
				  &crc2, HA7NET_FLAGS_NORESEND);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_readp(%d): Unable to read data from the DS2406 "
		    "reader; ha7net_writeblock_ex() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  data looks like
      *
      *     ZZZZXyXyXyXyCCXyXyXyXyCCXyXyXyXyCCXyXyXyXyCC
      *     01234567890123456789012345678901234567890123
      *     00000000001111111111222222222233333333334444
      *
      *  ZZZZ = 0xF5 0xEE 0xFF 0x<channel info byte>
      *     X = 0xFF or 0x55 representing a single bit read
      *     y = 0xFA
      *    CC = two byte CRC16
      *
      *  We want to read out the 16 bits corresponding to the high bit
      *  of each of the 16 "X" bytes shown above.
      */
     ptr1  = data + 4;          /* +4 for channel cfg info                  */
     ptr2  = data + 4 + 16 + 4; /* +4 for channel cfg info + 2x CRC16 bytes */
     uval1 = 0;
     uval2 = 0;
     umask = 0x80;
     for (i = 0; i < 8; i++)
     {
	  if (*ptr1 & 0x80)
	       uval1 |= umask;
	  if (*ptr2 & 0x80)
	       uval2 |= umask;
	  umask >>= 1;
	  if (0 != ((i + 1) % 4))
	  {
	       ptr1 += 2;
	       ptr2 += 2;
	  }
	  else
	  {
	       /*
		*  Skip over 2 byte long CRC 16
		*/
	       ptr1 += 2 + 2;
	       ptr2 += 2 + 2;
	  }
     }

     if (hi_b)
	  *hi_b = uval1;
     if (lo_b)
	  *lo_b = uval2;

     /*
      *  All done
      */
     return(ERR_OK);
}


int
tai_8570_done(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     tai_8570_t *devx;

     if (dev_dotrace)
	  dev_trace("tai_8570_done(%d): Called with ctx=%p; dev=%p, "
		    "devices=%p", __LINE__, ctx, dev, devices);

     /*
      *  Bozo check
      */
     if (!ctx || !dev)
     {
	  dev_debug("tai_8570_done(%d): Invalid call arguments; ctx=%p, "
		    "dev=%p; neither may be non-zero", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     devx = (tai_8570_t *)dev_private(dev);
     if (devx)
     {
	  if (!devx->ignore_state)
	  {
	       device_t *ds2406 = dev_group_get(dev);
	       if (ds2406 && (ds2406 = dev_group_next(ds2406)))
		    dev_flag_clear(ds2406, DEV_FLAGS_IGNORE);
	  }
	  free(devx);
     }
     dev_ungroup(dev);
     dev_private_set(dev, NULL);
     return(ERR_OK);
}


int
tai_8570_init(ha7net_t *ctx, device_t *dev, device_t *devices)
{
     unsigned char c, npages, page, *ptr;
     unsigned char data[64];  /* must be max(64, 4*2) */
     ha7net_crc_t crc = HA7NET_CRC16(0, 12, 0);
     tai_8570_t *devx;
     device_t *ds2406, *rdev, *wdev;
     int dev_vcc, ds2406_vcc, i, istat;
     char romid[16+1];
     static const char *hex = "0123456789ABCDEF";
     static const unsigned char tai_8570_signature[13] = {
	  0x0F, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	  0x38, 0x35, 0x37, 0x30, 0x00};

     if (dev_dotrace)
	  dev_trace("tai_8570_init(%d): Called with ctx=%p; dev=%p, "
		    "devices=%p", __LINE__, ctx, dev, devices);

     if (!ctx || !dev)
     {
	  dev_debug("tai_8570_init(%d): Invalid call "
		    "arguments; ctx=%p, dev=%p; none of the preceding call "
		    "arguments should be NULL", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     else if (dev_fcode(dev) != OWIRE_DEV_2406)
     {
	  dev_debug("tai_8570_init(%d): The device dev=%p with family "
		    "code 0x%02x does not appear to be a DS2406 device "
		    "(0x%02x); the device appears to be a %s",
		    __LINE__, dev, dev_fcode(dev), OWIRE_DEV_2406,
		    dev_strfcode(dev_fcode(dev)));
	  return(ERR_NO);
     }

     /*
      *  Initializations
      */
     devx = NULL;

     /*
      *  A TAI 8570 has two DS2406 devices.  One of the two devices
      *  will have a TMEX file named 8570.0.  That file will contain
      *  the ROM ID of the other DS2406.
      */

     /*
      *  Get page 0 of the DS2406 and see if it is a directory page
      *  indicating a file named 8570.0.  On the assumption that the
      *  file will be found in the second page, we go ahead and pull the
      *  first two pages so as to save a little time.
      */
     istat = ha7net_readpages_ex(ctx, dev, data, 2*32, 0, 2, 0);
     if (istat != ERR_OK)
	  return(istat);
     else if (memcmp(data, tai_8570_signature, 13))
	  /*
	   *  Lacks a TMEX directory containing a file named "8570.0"
	   */
	  return(ERR_EOM);

     /*
      *  We have an 8570.0 file.  See what page it starts on
      */
     page   = data[13];
     npages = data[14];
     if (!page || page > 3 || npages != 1)
	  /*
	   *  Not what we are expecting
	   */
	  return(ERR_EOM);

     /*
      *  If the data isn't in page 1, then it's in page 2 or 3 which
      *  (1) we don't expect but can work with, and (2) had best get
      *  on with obtaining.
      */
     if (page == 1)
	  ptr = data + 32;
     else
     {
	  istat = ha7net_readpages_ex(ctx, dev, data, 32, (size_t)page, 1, 0);
	  if (istat != ERR_OK)
	       return(istat);
	  ptr = data;
     }

     /*
      *  We expect a file record of length 9: 8 bytes of ROM id and a NUL
      */
     if (ptr[0] != 9 || ptr[9] != 0x00)
	  return(ERR_EOM);

     /*
      *  The contents of the file are the correct length to be a
      *  64 bit serial number (i.e., a ROM id).  Let's make sure
      *  that the first byte is the family code for a DS 2406.
      */
     if (ptr[1] != (unsigned char)(0xff & OWIRE_DEV_2406))
	  return(ERR_EOM);

     /*
      *  Okay, we have eight bytes which yield the serial number of
      *  the associated DS2406 device.  Let's reverse their order,
      *  putting them into a hex-encoded string representing the serial
      *  number.
      */
     ptr++;
     romid[16] = '\0';
     i = 15;
     while (i >= 0)
     {
	  c = *ptr++;
	  romid[i--] = hex[c & 0x0f];
	  romid[i--] = hex[c >> 4];
     }
     dev_romid_cannonical(romid, sizeof(romid), romid, 16);

     /*
      *  Locate this device in the device list
      */
     ds2406 = devices;
     while(!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (!strcmp(dev_romid(ds2406), romid))
	       break;
	  ds2406++;
     }
     if (dev_flag_test(ds2406, DEV_FLAGS_END))
     {
	  dev_debug("tai_8570_init(%d): The associated DS2406 with ROM id "
		    "%s does not exist in the supplied device list",
		    __LINE__, romid);
	  return(ERR_NO);
     }

     /*
      *  Looks like this DS2406 is indeed a TAI 8570 Pressure Probe
      */

     /*
      *  Determine which DS2406 is the 3-Wire bus writer and which
      *  is the reader.  The writer has Vcc tied to +5V whereas the
      *  reader's is tied to ground.
      *
      *  To determine this, we want to read the Channel Info byte from
      *  each DS2406.  To that end, we send a Channel Access command to
      *  each DS2406.
      *
      *   ALR | IM | TOG | IC | CHS1 | CHS0 | CRC1 | CRC0
      *    0    1     0     1    0       1     1      0
      */
     istat = ha7net_writeblock_ex(ctx, dev, data, 14,
				  "F556FFFFFFFFFFFFFFFFFFFFFFFF", &crc, 0);

     if (istat != ERR_OK)
	  return(istat);
     dev_vcc = (0x80 & data[3]) ? 1 : 0;

     istat = ha7net_writeblock_ex(ctx, ds2406, data, 14,
				  "F556FFFFFFFFFFFFFFFFFFFFFFFF", &crc, 0);
     if (istat != ERR_OK)
	  return(istat);
     ds2406_vcc = (0x80 & data[3]) ? 1 : 0;

     /*
      *  Make sure that ds2406_vcc XOR dev_vcc is true
      */
     if ((ds2406_vcc && dev_vcc) || !(ds2406_vcc || dev_vcc))
	  return(ERR_EOM);

     /*
      *  We've jumped through enough hoops to believe that this
      *  is really a TAI8570.  Let's allocate some VM for
      *  a tai_8570_t structure.
      */
     devx = (tai_8570_t *)calloc(1, sizeof(tai_8570_t));
     if (!devx)
     {
	  dev_debug("tai_8570_init(%d): Insufficient virtual memory",
		    __LINE__);
	  return(ERR_NOMEM);
     }

     /*
      *  Sort out which device is which
      */
     if (dev_vcc)
     {
	  devx->wdev = dev;
	  devx->rdev = ds2406;
     }
     else
     {
	  devx->wdev = ds2406;
	  devx->rdev = dev;
     }

     /*
      *  Reset the interface between the DS2406s and the MS5534A
      */
     istat = tai_8570_reset(ctx, devx);
     if (istat != ERR_OK && istat != ERR_CRC)
     {
	  dev_debug("tai_8570_init(%d): Unable to reset the TAI 8570; error "
		    "sending the reset sequence", __LINE__);
	  goto done;
     }

     /*
      *  Read the calibration constants.  These are not in a
      *  DS 2406 PROM but rather buried in the MS5534A.  This is
      *  the last step in assuring ourselves that this is indeed
      *  a functional TAI8570.
      *
      *  An example set of calibration constants are as follows.
      *
      *    Xmit: 0xF5EEFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFA
      *            FFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF
      *    Recv: 0xF5EEFF4FFFFA55FAFFFAFFFA8695FFFAFFFAFFFA55FA89D955FAFFFA
      *            FFFA55FA03DE55FAFFFAFFFA55FA03DE
      *    W1 = 0xBE66
      *
      *    Xmit: 0xF5EEFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFA
      *            FFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF
      *    Recv: 0XF5EEFF4755FAFFFAFFFA55FA0DF855FAFFFAFFFAFFFA7D7E55FAFFFA
      *            55FAFFFA5CA6FFFA55FAFFFAFFFAEF73
      *    W2 = 0x675B
      *
      *    Xmit: 0xF5EEFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFA
      *            FFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF
      *    Recv: 0xF5EEFF4FFFFA55FAFFFAFFFA8695FFFA55FA55FAFFFACEABFFFA55FA
      *            FFFA55FA91D3FFFAFFFAFFFAFFFAF779
      *    W3 = 0xB9AF
      *
      *    Xmit: 0xF5EEFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFFFFFAFFFA
      *            FFFAFFFAFFFFFFFAFFFAFFFAFFFAFFFF
      *    Recv: 0xF5EEFF4FFFFAFFFA55FA55FAC1E755FA55FA55FAFFFA44ACFFFA55FA
      *            55FAFFFACEABFFFA55FA55FAFFFACEAB
      *    W4 = 0xC199
      *
      *  Which yields
      *
      *    c1 = 24371 (Pressure sensitivity)
      *    c2 = 3033 (Pressure offset)
      *    c3 = 774 (Temperature coefficient of pressure sensitivity)
      *    c4 = 742 (Temperature coefficient of pressure offset)
      *    c5 = 413 (Reference temperature)
      *    c6 = 27 (Temperature coefficient of the temperature)
      *
      *  And the derived value
      *
      *    ut1 = 23528 (Calibration temperature, 8 C5 + 20224)
      */

     for (i = 0; i < 4; i++)
     {
	  istat = tai_8570_readp(ctx, devx, data + 2 * i, data + 1 + 2 * i,
				 cmd_readw[i], 0);
	  if (istat != ERR_OK)
	  {
	       dev_debug("tai_8570_init(%d): Unable to read the calibrartion "
			 "data from the MS5534 Barometer Module", __LINE__);
	       goto done;
	  }
     }

     /*
      *  c1 is the high 15 bits of word1 ( word1 = data[0]<<8 | data[1] )
      */
     devx->c1 = ((int)data[0] << 7) | ((int)data[1] >> 1);

     /*
      *  c5 is the the lowest bit of word1 and the high 10 bits of word2
      */
     devx->c5 = ((int)(data[1] & 0x01) << 10) | ((int)data[2] << 2) |
	  ((int)(data[3] & 0xC0) >> 6);


     /*
      *  c6 is the lowest 6 bits of word2
      */
     devx->c6 = (int)(0x3F & data[3]);

     /*
      *  c4 is the high 10 bits of word 3
      */
     devx->c4 = ((int)data[4] << 2) | ((int)(data[5] & 0xC0) >> 6);

     /*
      *  c2 is the low 6 bits of word 3 and the low 6 bits of word 4
      */
     devx->c2 = ((int)(0x3f & data[5]) << 6) | (int)(data[7] & 0x3F);

     /*
      *  c3 is the high 10 bits of word 4
      */
     devx->c3 = ((int)data[6] << 2) | ((int)(data[7] & 0xC0) >> 6);

     /*
      *  Reference temperature
      */
     devx->ut1 = 8 * devx->c5 + 20224;

     /*
      *  Lock down the data structure while we make changes to it
      */
     dev_lock(dev);

     /*
      *  Tie this device specific data into the device's descriptor
      */
     dev_private_set(dev, devx);

     /*
      *  Data field info
      */
     dev->data.fld_used[0]   = DEV_FLD_USED;
     dev->data.fld_dtype[0]  = DEV_DTYPE_TEMP;
     dev->data.fld_format[0] = tai_8570_temp_prec;
     dev->data.fld_units[0]  = DEV_UNIT_C;

     dev->data.fld_used[1]   = DEV_FLD_USED;
     dev->data.fld_dtype[1]  = DEV_DTYPE_PRES;
     dev->data.fld_format[1] = tai_8570_pres_prec;
     dev->data.fld_units[1]  = DEV_UNIT_MBAR;

     /*
      *  Group the devices together if they are not already
      */
     dev_group(tai_8570_name, dev, ds2406, NULL);

     /*
      *  Unlock the structure
      */
     dev_unlock(dev);

     /*
      *  Mark the other ds2406 "ignore" so as to prevent an initialization
      *  attempt on it.  Such attempts are only prevented when the current
      *  DS2406 ("dev") is turned up first by the 1-Wire search.
      */
     devx->ignore_state = dev_flag_test(ds2406, DEV_FLAGS_IGNORE);
     dev_flag_set(ds2406, DEV_FLAGS_IGNORE);

     /*
      *  Success!
      */
     istat = ERR_OK;

done:
     if (istat != ERR_OK && devx)
	  free(devx);
     return(istat);
}


int
tai_8570_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     unsigned char data[4];
     tai_8570_t *devx;
     float dt, pres, temp;
     int istat;
     time_t t0, t1;

     if (dev_dotrace)
	  dev_trace("tai_8570_read(%d): Called with ctx=%p; dev=%p, "
		    "flags=0x%x", __LINE__, ctx, dev, flags);

     /*
      *  Bozo check
      */
     if (!ctx || !dev)
     {
	  dev_debug("tai_8570_read(%d): Invalid call arguments; ctx=%p, "
		    "dev=%p; neither may be non-zero", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     devx = (tai_8570_t *)dev_private(dev);
     if (!devx || !devx->rdev || !devx->wdev ||
	 dev_fcode(dev) != OWIRE_DEV_2406)
     {
	  dev_debug("tai_8570_read(%d): The device dev=%p with family code "
		    "0x%02x does not appear to be an AAG TAI 8570 Pressure "
		    "Sensor or tai_8570_init() has not yet been called for "
		    "this device", __LINE__, dev, dev_fcode(dev));
	  return(ERR_NO);
     }

     /*
      *  Reset the interface between the DS2406s and the MS5534A
      */
     istat = tai_8570_reset(ctx, devx);
     if (istat != ERR_OK && istat != ERR_CRC)
     {
	  dev_debug("tai_8570_read(%d): Unable to reset the TAI 8570; error "
		    "sending the reset sequence", __LINE__);
	  return(istat);
     }

     /*
      *  Request a pressure conversion.  The Intersema MS5534 spec sheet
      *  gives 35 milliseconds as the maximum time required for a conversion.
      *
      *  Using the example calibration constants from tai_8570_init(),
      *  an example pressure reading is 0x45a2 which corresponds to a
      *  reading of 876.5 mbar = 0.8650 atm = 25.88 in Hg (which is typical
      *  for the elevation of 4,200 ft = 1.3 km where I live).  [example
      *  temperature is 0x5ef2).
      */
     t0 = time(NULL);
     istat = tai_8570_readp(ctx, devx, data, data + 1, cmd_readd1, 35);
     t1 = time(NULL);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_reset(%d): Unable to perform a pressure "
		    "measurement; tai_8570_readp() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Reset the interface between the DS2406s and the MS5534A
      */
     istat = tai_8570_reset(ctx, devx);
     if (istat != ERR_OK && istat != ERR_CRC)
     {
	  dev_debug("tai_8570_read(%d): Unable to reset the TAI 8570; error "
		    "sending the reset sequence", __LINE__);
	  return(istat);
     }

     /*
      *  Request a temperature conversion.
      *
      *  Using the example calibration constants shown in the comments
      *  of tai_8570_init(), a reading of 0x5ef2 corresponds to a
      *  temperature of 25.9 C = 78.5 F.
      */
     istat = tai_8570_readp(ctx, devx, data + 2, data + 3, cmd_readd2, 35);
     if (istat != ERR_OK)
     {
	  dev_debug("tai_8570_reset(%d): Unable to perform a pressure "
		    "measurement; tai_8570_readp() returned %d; %s",
		    __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     temp = tai_8570_temp_calc(&dt, data[2], data[3], devx->c6, devx->ut1);
     pres = tai_8570_pres_calc(data[0], data[1], dt, devx->c1, devx->c2,
			       devx->c3, devx->c4);

     dev_lock(dev);
     dev->data.time[dev->data.n_current] = t0 + (int)(difftime(t1, t0)/2.0);
     dev->data.val[0][dev->data.n_current] = temp;
     dev->data.val[1][dev->data.n_current] = pres;
     dev_unlock(dev);

     return(ERR_OK);
}


int
tai_8570_show(ha7net_t *ctx, device_t *dev, unsigned int flags,
	      device_proc_out_t *proc, void *out_ctx)
{
     tai_8570_t *devx;

     if (dev_dotrace)
	  dev_trace("tai_8570_show(%d): Called with ctx=%p; dev=%p, "
		    "flags=0x%x, proc=%p, out_ctx=%p",
		    __LINE__, ctx, dev, flags, proc, out_ctx);

     if (!dev || !proc)
	  return(ERR_BADARGS);

     if (!(devx = (tai_8570_t *)dev_private(dev)))
     {
	  (*proc)(out_ctx,
"The device does not appear to be initialized: the private device field\n"
"is NULL.\n");
	  return(ERR_NO);
     }
     else if (!devx->wdev || !devx->rdev)
     {
	  (*proc)(out_ctx,
"The device does not appear to be initialized: the read and write DS2406\n"
"devices have not yet been identified.\n");
	  return(ERR_NO);
     }

     (*proc)(out_ctx,
"AAG TAI 8570 Barometric Pressure Sensor\n"
"  Write DS2406 = %s\n"
"   Read DS2406 = %s\n"
"\n"
"  Calibration constants as read from the Intersema MS5534 Barometer module:\n"
"\n"
"    c1 = %d (Pressure sensitivity)\n"
"    c2 = %d (Pressure offset)\n"
"    c3 = %d (Temperature coefficient of pressure sensitivity)\n"
"    c4 = %d (Temperature coefficient of pressure offset)\n"
"    c5 = %d (Reference temperature)\n"
"    c6 = %d (Temperature coefficient of the temperature)\n"
"   ut1 = %d (Calibration temperature, 8 C5 + 20224)\n"
"\n"
"  (Constant names shown are as per the Intersema data sheet, DA5534_022.doc\n"
"  dated 17 July 2002.)\n",
         dev_romid(devx->wdev), dev_romid(devx->rdev),
         devx->c1, devx->c2, devx->c3, devx->c4, devx->c5, devx->c6,
         devx->ut1);

     return(ERR_OK);
}
