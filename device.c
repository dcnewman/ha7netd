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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "err.h"
#include "atmos.h"
#include "convert.h"
#include "vapor.h"
#include "glob.h"

/*
 *  Must do this before including device.h
 */
#define __BUILD_DNAMES__
#define __BUILD_UNITS__
#include "xml_const.h"
#undef __BUILD_DNAMES__
#undef __BUILD_UNITS__

#define __DEV_SKIP_DEBUG
#include "device.h"
#undef __DEV_SKIP_DEBUG

#include "ha7net.h"
#include "owire_devices_private.h"

static device_proc_init_t dev_default_init;
static device_proc_read_t dev_default_read;

static device_dispatch_t *device_drivers[256];

#if defined(DRIVER)
#undef DRIVER
#endif

#if defined(DECLARE)
#undef DECLARE
#endif

#define DRIVER(a1,a2,a3,a4,a5,a6,a7,a8)
#define DECLARE(a1,a2) \
     extern a1 a2;
#include "devices.h"
#undef DECLARE
#undef DRIVER

#define DECLARE(t,p)
#define DRIVER(name,fcode,drvinit,drvdone,devinit,devdone,devread,devshow)  \
     { NULL, fcode, name, sizeof(name)-1, drvinit, drvdone, devinit, \
       devdone, devread, devshow },

static device_dispatch_t driver_block[] = {
#include "devices.h"
     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static device_proc_read_t dev_default_read;

static device_dispatch_t default_driver = {
     NULL, 0x00, "Default driver", 14, 0, 0, dev_default_init, 0,
     dev_default_read, NULL
};

#undef DRIVER
#undef DECLARE

static hi_lo_t hi_lo_init = {
     { 1.0e+38,  1.0e+38,  1.0e+38},
     {-1.0e+38, -1.0e+38, -1.0e+38},
     { (time_t)0, (time_t)0, (time_t)0},
     { (time_t)0, (time_t)0, (time_t)0},
     { EMPTY_TIMESTR, EMPTY_TIMESTR, EMPTY_TIMESTR},
     { EMPTY_TIMESTR, EMPTY_TIMESTR, EMPTY_TIMESTR}
};

static const char *dtype_names[1 + DEV_DTYPE_LAST - DEV_DTYPE_UNKNOWN];
static const char *dtype_descs[1 + DEV_DTYPE_LAST - DEV_DTYPE_UNKNOWN];
static const char *unit_names[1 + DEV_UNIT_LAST - DEV_UNIT_UNKNOWN];

static debug_proc_t  our_debug_ap;
static debug_proc_t *debug_proc = our_debug_ap;
static void         *debug_ctx  = NULL;
static int dbglvl        = 0;
static int dev_dodebug   = 0;
int        dev_dotrace   = 0;
int        dev_doverbose = 0;

static os_pthread_mutex_t mutex;
static int initialized = 0;

/*
 *  Default routine to write error information to stderr when (1) debug
 *  output is requested via the debug flags, and (2) no output procedure
 *  has been supplied by the caller.
 */

static void
our_debug_ap(void *ctx, int reason, const char *fmt, va_list ap)
{
     (void)ctx;
     (void)reason;

     vfprintf(stderr, fmt, ap);
     fputc('\n', stderr);
     fflush(stderr);
}


/*
 *  Log an error to the event log when the debug bits indicate DEBUG_ERRS
 */

void
dev_debug(const char *fmt, ...)
{
     va_list ap;

     if (1 || (dev_dodebug && debug_proc))
     {

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


/*
 *  Log verbose error information to the event log when the debug bits
 *  have both DEBUG_ERRS and DEBUG_VERBOSE set.  This has the effect of
 *  giving us a stack trace when an error occurs: rather than just
 *  having the source of the error and final consumer -- both ends of
 *  the subroutine call stack -- report the error, all intermediates
 *  in the call stack will also report the error.
 */

void
dev_detail(const char *fmt, ...)
{
     if (dev_doverbose && debug_proc)
     {
	  va_list ap;

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


/*
 *  Provide call trace information when the DEBUG_TRACE_HA7NET bit is set
 *  in the debug flags.
 */

void
dev_trace(const char *fmt, ...)
{
     va_list ap;

     if (dev_dotrace && debug_proc)
     {
	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


void
dev_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc  = proc ? proc : our_debug_ap;
     debug_ctx   = proc ? ctx : NULL;
     dbglvl      = flags;
     dev_dodebug   = ((dbglvl & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     dev_doverbose = (dev_dodebug && (dbglvl & DEBUG_VERBOSE)) ? 1 : 0;
     dev_dotrace   = ((dbglvl & DEBUG_TRACE_DEV) && debug_proc) ? 1: 0;

}

/*
 *  Since we export debug(), trace(), and do_trace, they were given
 *  the more export friendly names of dev_debug(), dev_trace() and
 *  dev_dotrace.  However, let's #define them back to the names used
 *  by the other modules...
 */
#define trace        dev_trace
#define detail       dev_detail
#define debug        dev_debug
#define do_trace     dev_dotrace

const char *
dev_strfcodeu(unsigned char fc, int *unknown)
{
     static const char *really_unknown = "Uknown family code";
     const char *tmp = owire_devices[fc];

     if (unknown)
	  *unknown = (!tmp || !strcmp(tmp, "Uknown family code 0x")) ? 1 : 0;
     return(tmp ? tmp : really_unknown);
}


const char *
dev_strfcode(unsigned char fc)
{
     return(dev_strfcodeu(fc, NULL));
}


device_dispatch_t *
dev_driver_get(unsigned char fc, const char *hint, size_t hlen)
{
     device_dispatch_t *dev, *tmp;

     dev = device_drivers[fc];
     if (!hint || !hlen || !dev)
	  return(dev);

     /*
      *  Use the hint to find the most apt driver.  This is necessary
      *  for situations like the AAG TAI-8540 Humidity Sensor and the
      *  HBI H3-R1-K Humidity Sensor, both of which pack onboard DS18S20
      *  and DS2438(Z) sensors making them indistinguishable.  Their
      *  difference lies in the HIH-3600 vs. HIH-4000 series humidity
      *  sensors which have different correction calculations and thus
      *  are not interchangeable.
      */
     tmp = dev;
     while (tmp)
     {
	  if (!strncasecmp(tmp->name, hint, hlen))
	       return(tmp);
	  tmp = tmp->next;
     }
     return(dev);
}


void
dev_lib_done(void)
{
     device_dispatch_t *drivers;

     if (do_trace)
	  trace("dev_lib_done(%d): Called", __LINE__);

     if (!initialized)
	  return;

     os_pthread_mutex_lock(&mutex);

     if (initialized)
     {
	  drivers = driver_block;
	  while (drivers->fcode)
	  {
	       if (drivers->drv_done)
		    (*drivers->drv_done)();
	       drivers++;
	  }
     }

     initialized = 0;
     ha7net_lib_done();
     os_pthread_mutex_unlock(&mutex);
     os_pthread_mutex_destroy(&mutex);
}


int
dev_lib_init(void)
{
     device_dispatch_t *drivers;
     size_t i;
     int ires;

     if (do_trace)
	  trace("dev_lib_init(%d): Called", __LINE__);

     if (initialized)
	  return(ERR_OK);

     os_pthread_mutex_init(&mutex, NULL);

     os_pthread_mutex_lock(&mutex);
     if (initialized)
     {
	  /*
	   *  Probably bad news if we're here...
	   */
	  os_pthread_mutex_unlock(&mutex);
	  return(ERR_OK);
     }

     /*
      *  Just in case this has not been done yet
      */
     ha7net_lib_init();

     /*
      *  Initialize the data type names
      */
     i = 0;
     while(build_dnames[i].dtype >= 0)
     {
	  dtype_names[build_dnames[i].dtype] = build_dnames[i].dname;
	  dtype_descs[build_dnames[i].dtype] = build_dnames[i].descr;
	  i++;
     }

     /*
      *  Initialize the list of unit abbreviations
      */
     i = 0;
     while(build_units[i].atype >= 0)
     {
	  unit_names[build_units[i].atype] = build_units[i].abbrev;
	  i++;
     }

     /*
      *  Now initialize the device driver list
      */
     memset(device_drivers, 0, 256 * sizeof(device_dispatch_t *));
     drivers = driver_block;
     while (drivers->fcode)
     {
	  if (drivers->drv_init)
	  {
	       ires = (*drivers->drv_init)();
	       if (ires != ERR_OK)
		    continue;
	  }
	  drivers->next = device_drivers[drivers->fcode];
	  device_drivers[drivers->fcode] = drivers;
	  drivers++;
     }
     for (i = 0; i < 255; i++)
     {
	  if (device_drivers[i])
	       continue;
	  device_drivers[i] = &default_driver;
     }

     initialized = 1;
     os_pthread_mutex_unlock(&mutex);

     /*
      *  All done
      */
     return(ERR_OK);
}


static int
dev_default_init(ha7net_t *ctx, device_t *dev, device_t *devs)
{
     if (do_trace)
	  trace("dev_default_init(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p; devs=%p",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, devs);

     return(ERR_EOM);
}


static int
dev_default_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     if (do_trace)
	  trace("dev_default_read(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p; flags=0x%x (%u)",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, flags,
	       flags);

     return(ERR_NO);
}


int
dev_init(ha7net_t *ctx, device_t *dev, device_t *devs)
{
     int i, istat;

     if (do_trace)
	  trace("dev_init(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p; devs=%p",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, devs);

     if (!dev)
     {
	  debug("dev_init(%d): Invalid call arguments supplied; ctx=%p, "
		"dev=%p; devs=%p", __LINE__, ctx, dev, devs);
	  return(ERR_BADARGS);
     }

     if (!dev->driver)
	  /*
	   *  Not supposed to happen!
	   */
	  dev->driver = dev_driver_get(dev->fcode, NULL, 0);

     /*
      *  Return now if there is nothing to do: either the device
      *  does not have an initialization routine OR the device
      *  has been marked "ignore".
      */
     if (!dev->driver->init || dev_flag_test(dev, DEV_FLAGS_IGNORE))
	  goto nearly_done;

     /*
      *  Call the initialization procedure
      */
again:
     istat = (*dev->driver->init)(ctx, dev, devs);
     if (istat != ERR_OK)
     {
	  if (istat != ERR_EOM)
	       return(istat);
	  if (dev->driver->next)
	  {
	       dev->driver = dev->driver->next;
	       goto again;
	  }
	  dev_flag_set(dev, DEV_FLAGS_IGNORE);
	  return(ERR_EOM);
     }

     /*
      *  As fld_dtype[] is used to index into other arrays, we perform
      *  sanity checks on this array
      */
     for (i = 0; i < NVALS; i++)
	  if (dev->data.fld_used[i] &&
	      (dev->data.fld_dtype[i] < DEV_DTYPE_FIRST ||
	       dev->data.fld_dtype[i] > DEV_DTYPE_LAST))
	       dev->data.fld_dtype[i] = DEV_DTYPE_UNKNOWN;

nearly_done:
     dev_flag_set(dev, DEV_FLAGS_INITIALIZED);
     return(ERR_OK);
}


int
dev_done(ha7net_t *ctx, device_t *dev, device_t *devs)
{
     int istat;

     if (do_trace)
	  trace("dev_done(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p; devs=%p",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, devs);

     /*
      *  Could just as easily be done with a macro
      */
     if (!dev)
     {
	  debug("dev_done(%d): Invalid call arguments supplied; ctx=%p, "
		"dev=%p; devs=%p", __LINE__, ctx, dev, devs);
	  return(ERR_BADARGS);
     }

     /*
      *  Return now if there is nothing to do
      */
     if (!dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	  return(ERR_OK);

     istat = (dev->driver && dev->driver->done) ?
	  (*dev->driver->done)(ctx, dev, devs) : ERR_OK;

     dev_flag_clear(dev, DEV_FLAGS_INITIALIZED);

     /*
      *  And return our result
      */
     return(istat);
}


int
dev_stats(device_t *dev, int fld_start, int fld_end, size_t fld_ignore1,
	  size_t fld_ignore2)
{
     int dt, i, i0, i1, i2, j, k;
     float dt2, dt_sum[NVALS][NPERS];
     size_t n0;
     time_t t0, t1, t2;

     /*
      *  Sanity test
      */
     if (!dev)
	  return(ERR_BADARGS);

     /*
      *  Force the starting and ending indices to be within range
      */
     if (fld_start < 0)
	  fld_start = 0;
     if (fld_end >= NVALS)
	  fld_end = NVALS - 1;

     dev_lock(dev);

     n0 = dev->data.n_current;

     for (i = fld_start; i <= fld_end; i++)
     {
	  /*
	   *  Ignore unused data fields
	   */
	  if (!dev->data.fld_used[i] || i == fld_ignore1 || i == fld_ignore2)
	       continue;

	  /*
	   *  Minima
	   */
	  if (dev->data.val[i][n0] < dev->data.today.min[i])
	  {
	       dev->data.today.min[i] = dev->data.val[i][n0];
	       dev->data.today.tmin[i] = dev->data.time[n0];
	       dev->data.today.tmin_str[i][0] = 0;
	  }
	  /* Cannot use else as we init the min to a LARGE value
	   * and the max to a SMALL value.  So, first value we
	   * see sets both min and max.
	   */
	  /*
	   *  Maxima
	   */
	  if (dev->data.val[i][n0] > dev->data.today.max[i])
	  {
	       dev->data.today.max[i] = dev->data.val[i][n0];
	       dev->data.today.tmax[i] = dev->data.time[n0];
	       dev->data.today.tmax_str[i][0] = 0;
	  }
     }

     /*
      *  Running averages
      *
      *  We compute these in a tedious fashion.  We could just add in
      *  the new data and subtract out the trailing data; however, that
      *  might lead to excessive cumulative roundoff errors.
      */

     /*
      *  First, see if we need to compute any running averages
      */
     if (dev->data.avgs.period[0] <= 0)
	  goto skip_averages;

     /*
      *  Zero the fields
      */
     for (j = fld_start; j <= fld_end; j++)
     {
	  if (!dev->data.fld_used[j] || j == fld_ignore1 || j == fld_ignore2)
	       break;
	  for (k = 0; k < NPERS; k++)
	  {
	       dev->data.avgs.avg[j][k] = 0.0;
	       dt_sum[j][k] = 0.0;
	  }
     }
     memset(dev->data.avgs.range_exists, 0, sizeof(device_period_array_t));

     /*
      *  t0 = current time
      */
     i0 = (int)n0;
     t0 = dev->data.time[i0];

     /*
      *  Now compute the running averages
      *
      *  Note that since there can be missing samples; we cannot readily
      *  compute how many samples to look back.  That is, even if we know
      *  that we want a 30 minute average and that the sampling periond is
      *  2 minutes, we do not know if looking back 15 samples is correct or
      *  not: if we missed a sampling cycle, then we would only want to
      *  look back 14 samples....  The easiest way to deal with this is
      *  to just start moving backwards in time until we hit samples outside
      *  of our largest sampling window, dev->data.avgs.period[0].
      */
     for (i = 1; i < NPAST; i++)
     {
	  i1 = i0 - i;
	  i2 = i1 + 1;
	  if (i1 < 0)
	  {
	       i1 = NPAST + i1;
	       if (i2 < 0)
		    i2 = NPAST + i2;
	  }
	  t1 = dev->data.time[i1];
	  t2 = dev->data.time[i2];
	  if (t1 == (time_t)0 || t2 == (time_t)0)
	       /*
		*  We've gone beyond recorded history
		*/
	       break;
	  else if (t1 == DEV_MISSING_TVALUE || t2 == DEV_MISSING_TVALUE)
	       /*
		*  Missing value
		*/
	       continue;
	  dt = (int)difftime(t0, t1);
	  if (dt > dev->data.avgs.period[0])
	       /*
		*  We're now looking too far back in time
		*/
	       break;
	  dt2 = (float)difftime(t2, t1);
	  for (j = fld_start; j <= fld_end; j++)
	  {
	       if (!dev->data.fld_used[j] || j == fld_ignore1 ||
		   j == fld_ignore2)
		    continue;
	       for (k = 0; k < NPERS; k++)
	       {
		    if (dev->data.avgs.period[k] <= 0)
			 /*
			  *  No more averaging periods
			  */
			 break;
		    if (dt > dev->data.avgs.period[k])
		    {
			 /*
			  *  We're looking too far back in time for this
			  *  averaging period.
			  */
			 dev->data.avgs.range_exists[k] = 1;
			 break;
		    }
		    else if (dt >= dev->data.avgs.period_approx[k])
			 dev->data.avgs.range_exists[k] = 1;

		    /*
		     *  We compute an integrated average:
		     *
		     *      v(i-1)
		     *         *---
		     *         |   ---  v(i)
		     *         |      ---*
		     *         |         |
		     *         *---------*
		     *      t(i-1)      t(i)
		     *
		     *  Area = area of rectangle + area of triangle
		     *       = [t(i) - t(i-1)] * v(i-1)
		     *           + 1/2 [t(i) - t(i-1)] * [v(i) - v(i-1)]
		     *       = [t(i) - t(i-1)] * [v(i-1) + 1/2v(i) - 1/2v(i-1)]
		     *       = [t(i) - t(i-1)] * [v(i) + v(i-1)] / 2
		     */   
		    dev->data.avgs.avg[j][k] +=
			 0.5 * (dev->data.val[j][i2] + dev->data.val[j][i1])
			                                                 * dt2;
		    dt_sum[j][k] += dt2;
	       }
	  }
     }
     /*
      *  And, compute the averages
      */
     for (j = fld_start; j <= fld_end; j++)
     {
	  if (!dev->data.fld_used[j] || j == fld_ignore1 || j == fld_ignore2)
	       break;
	  for (k = 0; k < NPERS; k++)
	       if (dt_sum[j][k] > 0.0)
		    dev->data.avgs.avg[j][k] = dev->data.avgs.avg[j][k] /
			 dt_sum[j][k];
	       else
		    dev->data.avgs.avg[j][k] = 0.0;
     }

skip_averages:
     dev_unlock(dev);

     /*
      *  All done
      */
     return(ERR_OK);
}


int
dev_read(ha7net_t *ctx, device_t *dev, unsigned int flags)
{
     int istat;
     size_t n0, n1;

     if (do_trace)
	  trace("dev_read(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p and flags=0x%x (%u)",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, flags,
	       flags);

     if (!dev)
     {
	  debug("dev_read(%d): Invalid call arguments supplied; ctx=%p, "
		"dev=%p", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     /*
      *  Return now if there is nothing to do
      */
     if (!dev->driver || !dev->driver->read)
     {
	  debug("dev_read(%): Device driver has supplied no routine to "
		"read the device", __LINE__);
	  return(ERR_NO);
     }

     /*
      *  Select the next data bin
      */
     dev_lock(dev);
     n1 = dev->data.n_current;
     dev->data.n_current  = n0 = (n1 < (NPAST-1)) ? n1 + 1 : 0;
     dev->data.n_previous = n1;
     dev_unlock(dev);

     /*
      *  Call the read procedure
      */
     istat = (*dev->driver->read)(ctx, dev, flags);
     if (istat != ERR_OK)
     {
	  /*
	   *  Indicate a missing value
	   */
	  dev_lock(dev);
	  dev->data.time[n0] = DEV_MISSING_TVALUE;
	  dev_unlock(dev);

	  /*
	   *  And return the error
	   */
	  return(istat);
     }

     /*
      *  Update running stats: minima, maxima, and averages
      *
      *  If the device has pressure correction enabled, then ignore
      *  that slot for now as we cannot compute it until all the devices
      *  it depends upon have been read.
      */
     dev_stats(dev, 0, NVALS - 1, 
	       dev->data.pcor ? dev->data.pcor->fld_spare : NVALS,
	       dev->data.pcor ? dev->data.pcor->fld_spare2 : NVALS);

     /*
      *  And return
      */
     return(ERR_OK);
}


int
dev_show(ha7net_t *ctx, device_t *dev, unsigned int flags,
	 device_proc_out_t *out, void *out_ctx)
{
     if (do_trace)
	  trace("dev_show(%d): Called for device %s (dev=%p) with "
		"ha7net ctx=%p, flags=0x%x (%u), out=%p, out_ctx=%p",
		__LINE__, dev ? dev_romid(dev) : "(null)", dev, ctx, flags,
		flags, out, out_ctx);

     if (!dev || !out)
     {
	  debug("dev_show(%d): Invalid call arguments supplied; dev=%p, "
		"out=%p", __LINE__, dev, out);
	  return(ERR_BADARGS);
     }

     /*
      *  Return now if there is nothing to do
      */
     if (!dev->driver || !dev->driver->show)
	  return(ERR_OK);

     /*
      *  Call the read procedure
      */
     return((*dev->driver->show)(ctx, dev, flags, out, out_ctx));
}


device_t *
dev_group_get(device_t *dev)
{
     if (!dev || (!dev->group2.next && !dev->group2.prev))
	  return(NULL);

     /*
      *  Return now if there is nothing to do
      */
     while (dev->group2.prev)
	  dev = dev->group2.prev;
     return(dev);
}


int
dev_list_done(ha7net_t *ctx, device_t *devices)
{
     device_t *dev;
     int istat;

     if (do_trace)
	  trace("dev_list_done(%d): Called with ctx=%p, devices=%p",
		__LINE__, ctx, devices);

     /*
      *  Bozo checks
      */
     if (!ctx || !devices)
     {
	  debug("dev_list_done(%d): Bad call arguments supplied; ctx=%p, "
		"devices=%p", __LINE__, ctx);
	  return(ERR_BADARGS);
     }

     /*
      *  Loop through the list, initializing each device that has an
      *  initializion routine.
      */
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (!dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       goto skip_me;
	  dev_done(ctx, dev, devices);
	  dev_flag_clear(dev, DEV_FLAGS_INITIALIZED);
     skip_me:
	  dev++;
     }
     ha7net_releaselock(ctx);

     /*
      *  All done
      */
     return(ERR_OK);
}


int
dev_list_init(ha7net_t *ctx, device_t *devices)
{
     int badness_happened, istat;
     device_t *dev;

     if (do_trace)
	  trace("dev_list_init(%d): Called with ctx=%p, devices=%p",
		__LINE__, ctx, devices);

     /*
      *  Bozo checks
      */
     if (!ctx || !devices)
     {
	  debug("dev_list_init(%d): Bad call arguments supplied; ctx=%p, "
		"devices=%p", __LINE__, ctx);
	  return(ERR_BADARGS);
     }

     /*
      *  Loop through the list, initializing each device that has an
      *  initializion routine.
      */
     badness_happened = 0;
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE) ||
	      dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       goto skip_me;
	  istat = dev_init(ctx, dev, devices);
	  if (istat == ERR_EOM)
	  {
	       debug("dev_list_init(%d): Ignoring the device %s (dev=%p) "
		     "with family code 0x%02x (%s); no available driver "
		     "for this device; consider adding it to a "
		     "\"[ignore]\" block in the configuration file",
		     __LINE__, dev_romid(dev), dev, dev_fcode(dev),
		     dev_strfcode(dev_fcode(dev)));
	       dev_flag_set(dev, DEV_FLAGS_IGNORE);
	  }
	  else if (istat != ERR_OK)
	  {
	       detail("dev_list_init(%d): Error initializing device %s "
		      "(dev=%p) with family code 0x%02x (%s); error is %d; "
		      "%s", __LINE__, dev_romid(dev), dev, dev_fcode(dev),
		      dev_strfcode(dev_fcode(dev)), istat,
		      err_strerror(istat));
	       badness_happened = 1;
	  }
     skip_me:
	  dev++;
     }
     ha7net_releaselock(ctx);

     /*
      *  All done
      */
     return(badness_happened ? ERR_NO : ERR_OK);
}


int
dev_info_hints(device_t *devices, size_t ndevices, const device_loc_t *linfo)
{
     size_t l;

     if (do_trace)
	  trace("dev_info_hints(%d): Called with devices=%p, ndevices=%u, "
		"linfo=%p", __LINE__, devices, ndevices, linfo);

     /*
      *  Sanity checks
      */
     if (!devices && ndevices)
     {
	  debug("dev_info_hints(%d): Bad call arguments supplied; no "
		"device list supplied; devices=%p, ndevices=%u",
		__LINE__, devices, ndevices);	  
	  return(ERR_BADARGS);
     }

     for (l = 0; l < ndevices; l++)
	  dev_romid_cannonical(devices[l].romid, OWIRE_ID_LEN+1,
			       devices[l].romid, OWIRE_ID_LEN);

     /*
      *  Apply any hints to further refine our choice of drivers
      */
     while (linfo)
     {
	  /*
	   *  Locate this sensor in the device list
	   *  NOTE: we assume that the rom ids in linfo have
	   *  already been cannonicalized.  We cannot do that
	   *  here as other threads may also be using the linfo
	   *  list.
	   */
	  for (l = 0; l < ndevices; l++)
	  {
	       if (memcmp(linfo->romid, devices[l].romid, sizeof(linfo->romid)))
		    /*
		     *  ROM id's do not match
		     */
		    continue;
	       
	       /*
		*  If a driver hint was supplied, then update our notion
		*  of which driver to use for this device.
		*/
	       if (linfo->hint && linfo->hint[0])
		    devices[l].driver = dev_driver_get(devices[l].fcode,
						       linfo->hint, linfo->hlen);
	       if (linfo->group1.ref)
	       {
		    devices[l].group1 = linfo->group1;
		    devices[l].group1.next = NULL;
		    devices[l].group1.prev = NULL;
	       }
	  }
	  linfo = linfo->next;
     }

     /*
      *  All done
      */
     return(ERR_OK);
}


int
dev_info_merge(device_t *devices, size_t ndevices, int apply_hints,
	       device_period_array_t periods, const device_loc_t *linfo,
	       const device_ignore_t *ilist)
{
     int groups_seen;
     size_t l;
     device_period_array_t period_approx;
     char *tmpl;

     if (do_trace)
	  trace("dev_info_merge(%d): Called with devices=%p, ndevices=%u, "
		"apply_hints=%d, periods=%p, linfo=%p, ilist=%p",
		__LINE__, devices, ndevices, apply_hints, periods, linfo, ilist);

     /*
      *  Sanity checks
      */
     if (!devices && ndevices)
     {
	  debug("dev_info_merge(%d): Bad call arguments supplied; no "
		"device list supplied; devices=%p, ndevices=%u",
		__LINE__, devices, ndevices);	  
	  return(ERR_BADARGS);
     }

     /*
      *  Cannonicalize the rom id strings and establish default
      *  averaging periods
      */
     if (periods)
     {
	  for (l = 0; l < NPERS; l++)
	       period_approx[l] = (int)(0.95 * (float)periods[l]);
     }

     for (l = 0; l < ndevices; l++)
     {
	  dev_romid_cannonical(devices[l].romid, OWIRE_ID_LEN+1,
			       devices[l].romid, OWIRE_ID_LEN);
	  if (periods)
	  {
	       memcpy(&devices[l].data.avgs.period, periods,
		      sizeof(device_period_array_t));
	       memcpy(&devices[l].data.avgs.period_approx, period_approx,
		      sizeof(device_period_array_t));
	  }
     }

     /*
      *  Copy location and group name information from the device_loc_t list
      *  to the device_t array.
      */
     groups_seen = 0;
     while (linfo)
     {
	  /*
	   *  Locate this sensor in the device list
	   *  NOTE: we assume that the rom ids in linfo have
	   *  already been cannonicalized.  We cannot do that
	   *  here as other threads may also be using the linfo
	   *  list.
	   */
	  for (l = 0; l < ndevices; l++)
	  {
	       if (memcmp(linfo->romid, devices[l].romid,
			  sizeof(linfo->romid)))
		    /*
		     *  ROM id's do not match
		     */
		    continue;
	       
	       /*
		*  If a driver hint was supplied, then update our notion
		*  of which driver to use for this device.
		*/
	       if (apply_hints && linfo->hint && linfo->hint[0])
		    devices[l].driver = dev_driver_get(devices[l].fcode,
						       linfo->hint, linfo->hlen);

	       /*
		*  Copy over any device flags
		*/
	       if (linfo->flags)
		    dev_flag_set(&devices[l], linfo->flags);

	       /*
		*  Device specific averaging periods
		*/
	       if (linfo->periods[0])
	       {
		    size_t m;

		    memmove(&devices[l].data.avgs.period, linfo->periods,
			    sizeof(device_period_array_t));
		    for (m = 0; m < NPERS; m++)
			 devices[l].data.avgs.period_approx[m] =
			   (int)(0.95 * (float)devices[l].data.avgs.period[m]);
	       }

	       /*
		*  Allocate memory for the device description/location string
		*  and the device specific configuration data
		*/
	       tmpl = (char *)malloc(linfo->dlen + 1 + linfo->slen + 1);
	       if (!tmpl)
	       {
		    debug("dev_info_merge(%d): Insufficient virtual memory",
			  __LINE__);
		    return(ERR_NOMEM);
	       }

	       /*
		*  Device location/description
		*/
	       if (linfo->dlen)
		    memmove(tmpl, linfo->desc, linfo->dlen);
	       tmpl[linfo->dlen] = '\0';
	       devices[l].desc   = tmpl;
	       devices[l].dlen   = linfo->dlen;

	       /*
		*  Device specific information
		*/
	       devices[l].gain   = linfo->gain;
	       devices[l].offset = linfo->offset;
	       devices[l].spec = tmpl + linfo->dlen + 1;
	       devices[l].slen = linfo->slen;
	       if (linfo->slen && linfo->spec)
		    memmove(devices[l].spec, linfo->spec, linfo->slen);
	       devices[l].spec[linfo->slen] = '\0';

	       if (!linfo->group1.ref)
		    break;
	       groups_seen++;
	       devices[l].group1 = linfo->group1;
	       devices[l].group1.next = NULL;
	       devices[l].group1.prev = NULL;
 	       break;
	  }
	  linfo = linfo->next;
     }

     /*
      *  Now link up all devices in the same group
      */
     if (groups_seen > 1 && ndevices > 1)
     {
	  int ref;
	  size_t m;

	  for (l = 0; l < ndevices - 1; l++)
	  {
	       if (!devices[l].group1.ref)
		    /*
		     *  Skip this device
		     */
		    continue;
	       ref = devices[l].group1.ref;
	       for (m = l + 1; m < ndevices; m++)
	       {
		    if (ref != devices[m].group1.ref)
			 continue;
		    devices[l].group1.next  = &devices[m];
		    devices[m].group1.prev  = &devices[l];
		    break;
	       }
	  }
     }

     /*
      *  And finally, walk the list of devices to ignore and set the
      *  DEV_FLAGS_IGNORE bit on matching devices in the devices array.
      *
      *  Note: we assume that the patterns in the ignore list have
      *  cannonicalization compatible with dev_romid_cannonical().
      *
      *  For glob-style matching, that can be achieved by passing
      *  the pattern through dev_romid_cannonical() AND assuming
      *  that glob-patterns like '[a-z]' can be converted to '[A-Z]'
      *  and still have the intent of the glob-pattern be the same.
      */
     while (ilist)
     {
	  if (ilist->pat[0])
	  {
	       if (isglob(ilist->pat))
	       {
		    int istat;

		    for (l = 0; l < ndevices; l++)
		    {
			 if (devices[l].flags & DEV_FLAGS_IGNORE)
			      continue;
			 istat = glob(ilist->pat, devices[l].romid, 0);
			 if (istat == -1)
			 {
			      debug("dev_info_merge(%d): Bad glob-style "
				    "matching patern, \"%s\"; probably has "
				    "two consecutive '-' in it; not using "
				    "this pattern to select devices to "
				    "ignore", __LINE__, ilist->pat);
			      continue;
			 }
			 else if (istat == 0)
			      continue;
			 devices[l].flags |= DEV_FLAGS_IGNORE;
		    }
	       }
	       else
	       {
		    for (l = 0; l < ndevices; l++)
		    {
			 if (devices[l].flags & DEV_FLAGS_IGNORE)
			      continue;
			 if (ilist->plen != OWIRE_ID_LEN ||
			     memcmp(ilist->pat, devices[l].romid,
				    OWIRE_ID_LEN))
			      continue;
			 devices[l].flags |= DEV_FLAGS_IGNORE;
		    }
	       }
	  }
	  ilist = ilist->next;
     }

     /*
      *  All done
      */
     return(ERR_OK);
}


void
dev_romid_cannonical(char *dst, size_t dstmaxlen, const char *src,
		     size_t srclen)
{
     size_t n;

     if (!dst || !dstmaxlen)
	  return;

     if (srclen >= dstmaxlen)
	  srclen = dstmaxlen - 1;

     for (n = 0; n < srclen; n++)
	  dst[n] = toupper(src[n]);

     dst[srclen] = '\0';
}


void
dev_ungroup(device_t *dev)
{
     device_t *dev_next;

     if (do_trace)
	  trace("dev_ungroup(%d): Called with dev=%p", __LINE__, dev);

     dev = dev_group_get(dev);
     while (dev)
     {
	  dev_next = dev_group_next(dev);
	  if (dev_next)
	       dev_flag_clear(dev_next, DEV_FLAGS_ISSUB);
	  memset(&dev->group2, 0, sizeof(device_group_t));
	  dev = dev_next;
     }
}


int
dev_group(const char *gname, device_t *dev, ...)
{
     va_list ap;
     device_t *dev_next;
     size_t nlen;

     if (do_trace)
	  trace("dev_group(%d): Called with gname=\"%s\" (%p), dev=%p",
		__LINE__, gname ? gname : "(null)", gname, dev);

     nlen = gname ? strlen(gname) : 0;
     if (nlen >= DEV_GNAME_LEN)
	  nlen = DEV_GNAME_LEN - 1;

     va_start(ap, dev);
     while (dev)
     {
	  if (nlen)
	       memcpy(dev->group2.name, gname, nlen);
	  dev->group2.nlen = nlen;
	  dev->group2.name[nlen] = '\0';
	  dev_next = va_arg(ap, device_t *);
	  dev->group2.next = dev_next;
	  if (dev_next)
	  {
	       dev_flag_set(dev_next, DEV_FLAGS_ISSUB);
	       dev_next->group2.prev = dev;
	  }
	  dev = dev_next;
     }
     va_end(ap);

     return(ERR_OK);
}


void
dev_array_free(device_t *devs)
{
     device_t *tmpd;

     if (!devs)
	  return;

     tmpd = devs;
     while (!dev_flag_test(tmpd, DEV_FLAGS_END))
     {
	  if (tmpd->data.pcor)
	       free(tmpd->data.pcor);
	  os_pthread_mutex_destroy(&tmpd->mutex);
	  if (dev_desc(tmpd))
	       free(dev_desc(tmpd));
	  tmpd++;
     }
     free(devs);
}


device_t *
dev_array(size_t ndevices)
{
     device_t *devs;
     size_t n;

     devs = (device_t *)calloc(ndevices + 1, sizeof(device_t));
     if (!devs)
	  return(NULL);

     for (n = 0; n < ndevices; n++)
     {
	  os_pthread_mutex_init(&devs[n].mutex, NULL);
	  devs[n].data.today     = hi_lo_init;
	  devs[n].data.yesterday = hi_lo_init;
     }
     devs[ndevices].flags = DEV_FLAGS_END;
     return(devs);
}


void
dev_hi_lo_reset(device_t *devs)
{
     if (devs)
     {
	  while (!dev_flag_test(devs, DEV_FLAGS_END))
	  {
	       if (dev_flag_test(devs, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
		   !dev_flag_test(devs, DEV_FLAGS_INITIALIZED))
		    goto skip_me;
	       dev_lock(devs);
	       devs->data.yesterday = devs->data.today;
	       devs->data.today     = hi_lo_init;
	       dev_unlock(devs);
	  skip_me:
	       devs++;
	  }
     }
}


const char *
dev_dtypestr(int dtype)
{
     if (dtype < DEV_DTYPE_UNKNOWN || dtype > DEV_DTYPE_LAST)
	  return(NULL);
     else
	  return(dtype_names[dtype]);
     
}


const char *
dev_dtypedescstr(int dtype)
{
     if (dtype < DEV_DTYPE_UNKNOWN || dtype > DEV_DTYPE_LAST)
	  return(NULL);
     else
	  return(dtype_descs[dtype]);
     
}


const char *
dev_unitstr(int units)
{
     if (units < DEV_UNIT_UNKNOWN || units > DEV_UNIT_LAST)
	  return(NULL);
     else
	  return(unit_names[units]);
     
}


int
dev_pcor_adjust(device_t *dev, int period)
{
     float avg_rh, avg_rh2, avg_temp, avg_temp2, r, r2;
     int count_rh, count_rh2, count_temp, count_temp2, i, past, npast12;
     device_t *dev2;
     size_t fld, fld_press, fld_spare, fld_spare2, n, n2;
     device_press_adj_t *pcor;
     time_t t, t2;

     if (!dev)
     {
	  debug("dev_pcor_adjust(%d): Invalid call arguments supplied; dev=%p",
		__LINE__, dev);
	  return(ERR_BADARGS);
     }
     else if (!(pcor = dev_pcor(dev)))
	  /*
	   *  No pressure correction data for this device
	   */
	  return(ERR_OK);

     dev_lock(dev);
     n = dev->data.n_current;
     t = dev->data.time[n];
     fld_spare  = pcor->fld_spare;
     fld_spare2 = pcor->fld_spare2;
     fld_press  = pcor->fld_press;
     dev_unlock(dev);

     if (t == DEV_MISSING_TVALUE)
	  /*
	   *  Looks like we were unable to read the station pressure.
	   *  Consequently, there's no data to adjust....
	   */
	  return(ERR_OK);
     else if (fld_spare >= NVALS || fld_spare2 >= NVALS || fld_press >= NVALS)
     {
	  /*
	   *  Corrupted data?
	   */
	  debug("dev_pcor_adjust(%d): Corrupted data; "
		"dev->data.pcor->fld_spare=%u, dev->data.pcor->fld_spare2=%u; "
		"dev->data.pcor->fld_press=%u, NVALS=%d", __LINE__, fld_spare,
		fld_spare2, fld_press, NVALS);
	  return(ERR_NO);
     }

     /*
      *  Determine which slot should have the temperature
      *  from 12 hours in the past
      */
     past = 48; /* 15 minute increments */
try_again:
     n2 = (size_t)((60 * 15 * past) / period);
     if (n >= n2)
	  /*
	   *  Only need to look back n2 slots
	   */
	  npast12 = n - n2;
     else if (n2 < NPAST)
	  /*
	   *  Deal with wrapping
	   */
	  npast12 = (NPAST + n) - n2;
     else
	  /*
	   *  We're not storing enough data to look back 12 hours
	   */
	  npast12 = -1;

     /*
      *  See if we have data this far in the past.  If we don't, then
      *  shave 15 minutes off of the 12 hours and try again.
      */
     dev_lock(dev);
     t2 = dev->data.time[npast12];
     dev_unlock(dev);
     if (npast12 == -1 || t2 == 0)
     {
	  if (--past > 0)
	       goto try_again;
	  npast12 = -1;
     }

     /*
      *  Average the outdoor temperatures now and 12 hours previously
      */
     avg_temp    = 0.0;
     avg_temp2   = 0.0;
     count_temp  = 0;
     count_temp2 = 0;
     for (i = 0; i < pcor->ntemp; i++)
     {
	  if ((fld = pcor->temp_flds[i]) >= NVALS)
	       /*
		*  Bogus value for the field index.   Skip it.
		*/
	       continue;
	  dev2 = pcor->temp_devs[i];
	  if (!dev2)
	       /*
		*  Bogus value for the device pointer.  Skip it.
		*/
	       continue;

	  dev_lock(dev2);
	  n2 = dev2->data.n_current;
	  if (dev2->data.time[n2] != DEV_MISSING_TVALUE)
	  {
	       float t = convert_temp(dev2->data.val[fld][n2],
				      dev2->data.fld_units[fld], DEV_UNIT_C);
	       avg_temp  += t;
	       avg_temp2 += t;
	       count_temp++;
	       count_temp2++;
	  }
	  if (npast12 >= 0)
	  {
	       /*
		*  Also average in the temperature 12 hours previously
		*/
	       if (dev2->data.time[npast12] != DEV_MISSING_TVALUE)
	       {
		    avg_temp += convert_temp(dev2->data.val[fld][npast12],
					     dev2->data.fld_units[fld],
					     DEV_UNIT_C);
		    count_temp++;
	       }
	       /*
		*  Hmmmm... we were unable to read that device 12 hours
		*  ago.  Try for 12 hours ago + 1 period
		*/
	       else if (npast12 < (NPAST-1) &&
			dev2->data.time[npast12+1] != DEV_MISSING_TVALUE)
	       {
		    avg_temp += convert_temp(dev2->data.val[fld][npast12+1],
					     dev2->data.fld_units[fld],
					     DEV_UNIT_C);
		    count_temp++;
	       }
	       /*
		*  Or 12 hours ago - 1 period
		*/
	       else if (npast12 > 0 &&
			dev2->data.time[npast12-1] != DEV_MISSING_TVALUE)
	       {
		    avg_temp += convert_temp(dev2->data.val[fld][npast12-1],
					     dev2->data.fld_units[fld],
					     DEV_UNIT_C);
		    count_temp++;
	       }
	  }
	  dev_unlock(dev2);
     }

     /*
      *  Average the outdoor relative humidities
      */
     avg_rh    = 0.0;
     avg_rh2   = 0.0;
     count_rh  = 0;
     count_rh2 = 0;
     for (i = 0; i < pcor->nrh; i++)
     {
	  if ((fld = pcor->rh_flds[i]) >= NVALS)
	       /*
		*  Bogus value for the field index.   Skip it.
		*/
	       continue;
	  dev2 = pcor->rh_devs[i];
	  if (!dev2)
	       /*
		*  Bogus value for the device pointer.  Skip it.
		*/
	       continue;

	  dev_lock(dev2);
	  n2 = dev2->data.n_current;
	  if (dev2->data.time[n2] != DEV_MISSING_TVALUE)
	  {
	       float rh = convert_humidity(dev2->data.val[fld][n2],
					   dev2->data.fld_units[fld],
					   DEV_UNIT_RH);
	       avg_rh  += rh;
	       avg_rh2 += rh;
	       count_rh++;
	       count_rh2++;
	  }
	  dev_unlock(dev2);
     }

     if (count_rh) /* count_rh > 0 implies count_rh2 > 0 */
     {
	  avg_rh  = avg_rh / (float)count_rh;
	  avg_rh2 = avg_rh2 / (float)count_rh2;
     }
     else
     {
	  avg_rh  = -100.0;
	  avg_rh2 = -100.0;
     }

     if (count_temp) /* count_temp > 0 implies count_temp2 > 0 */
     {
	  avg_temp  = avg_temp / (float)count_temp;
	  avg_temp2 = avg_temp2 / (float)count_temp2;
	  r = atmos_press_adjust(pcor->alt_adjust, pcor->alt_station,
				 avg_temp, avg_rh);
	  r2 = atmos_press_adjust(pcor->alt_adjust, pcor->alt_station,
				  avg_temp2, avg_rh2);
	  dev_lock(dev);
	  dev->data.val[fld_spare][n]  = r  * dev->data.val[fld_press][n];
	  dev->data.val[fld_spare2][n] = r2 * dev->data.val[fld_press][n];
	  dev_unlock(dev);
	  dev_stats(dev, fld_spare, fld_spare2, NVALS, NVALS);
     }
     else
     {
	  /*
	   *  Unable to come up with any outside averaged temps.
	   *  At this point, we can either figure out a generic
	   *  ratio which does not take temps into account or
	   *  we can attempt to use the ratio used last time.
	   */
	  if (pcor->ntemp)
	  {
	       /*
		*  We do have temps, just not this time....  So, try
		*  to use the previously used ratio.
		*/
	       dev_lock(dev);
	       n2 = dev->data.n_previous;
	       if (dev->data.time[n2] != DEV_MISSING_TVALUE &&
		   dev->data.time[n2] != (time_t)0 &&
		   dev->data.val[fld_press][n2] != 0.0)
	       {
		    r = dev->data.val[fld_spare][n2] /
			 dev->data.val[fld_press][n2];
		    r2 = dev->data.val[fld_spare2][n2] /
			  dev->data.val[fld_press][n2];
	       }
	       dev->data.val[fld_spare][n]  = r  * dev->data.val[fld_press][n];
	       dev->data.val[fld_spare2][n] = r2 * dev->data.val[fld_press][n];
	       dev_unlock(dev);
	       goto done;
	  }
	  /*
	   *  No temperature data available.  Do the correction
	   *  for sea level at 15C and use the lapse rate of 0.0065 K/gpm
	   *  to assume the corresponding temperature at our altitude
	   */
	  r = atmos_press_adjust(pcor->alt_adjust, pcor->alt_station,
		 15.0 - 0.0065 * atmos_geopotential_alt(pcor->alt_station),
				 avg_rh);
	  r2 = atmos_press_adjust(pcor->alt_adjust, pcor->alt_station,
                 15.0 - 0.0065 * atmos_geopotential_alt(pcor->alt_station),
				 avg_rh2);
	  dev_lock(dev);
	  dev->data.val[fld_spare][n]  = r  * dev->data.val[fld_press][n];
	  dev->data.val[fld_spare2][n] = r2 * dev->data.val[fld_press][n];
	  dev_unlock(dev);
     }

     /*
      *  All done
      */
done:
     return(ERR_OK);
}

int
dev_pcor_add(device_t *device, device_t *devices, int altitude)
{
#define MAXDEVS 100
     device_t *dev, *rh_devs[MAXDEVS+1], *temp_devs[MAXDEVS+1];
     size_t i, ipress, ispare, ispare2, j, nrh, ntemp, rh_flds[MAXDEVS+1],
	  ssize, temp_flds[MAXDEVS+1];
     device_press_adj_t *pcor;

     /*
      *  Sanity checks
      */
     if (!device || !devices)
     {
	  debug("dev_pcor_add(%d): Invalid call arguments supplied; "
		"device=%p, devices=%p", __LINE__, device, devices);
	  return(ERR_BADARGS);
     }

     /*
      *  If the device is at sea level, then there's nothing to do
      */
     if (altitude == 0)
	  /*
	   *  Device is at sea level; no need to correct
	   */
	  return(ERR_OK);

     /*
      *  Does the device measure pressure and are there any spare slots
      *  for a sea level pressure correction?
      */
     ispare  = NVALS;
     ispare2 = NVALS;
     ipress  = NVALS;
     for (i = 0; i < NVALS; i++)
     {
	  if (device->data.fld_used[i])
	  {
	       if (ipress == NVALS &&
		   device->data.fld_dtype[i] == DEV_DTYPE_PRES)
		    ipress = i;
	  }
	  else if (ispare == NVALS)
	       ispare = i;
	  else if (ispare2 == NVALS)
	       ispare2 = i;
     }
     if (ipress == NVALS)
     {
	  detail("dev_pcor_add(%d): Device has no data slots marked as "
		 "measuring pressure", __LINE__);
	  return(ERR_OK);
     }
     else if (ispare == NVALS || ispare2 == NVALS || ispare == ispare2)
     {
	  detail("dev_pcor_add(%d): No spare slots to use for mean sea level "
		 "pressure correction", __LINE__);
	  return(ERR_OK);
     }

     /*
      *  Ensure that ispare < ispare2
      */
     if (ispare > ispare2)
     {
	  size_t itmp = ispare;
	  ispare  = ispare2;
	  ispare2 = itmp;
     }

     /*
      *  See if we have an outside thermometer and hygrometer
      */
     ntemp = 0;
     nrh   = 0;
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE) ||
	      !dev_flag_test(dev, DEV_FLAGS_OUTSIDE))
	       goto skip_me;
	  for (i = 0; i < NVALS; i++)
	  {
	       if (dev->data.fld_used[i])
	       {
		    if (dev->data.fld_dtype[i] == DEV_DTYPE_TEMP &&
			convert_known(DEV_UNIT_C, dev->data.fld_units[i]) &&
			ntemp < MAXDEVS)
		    {
			 
			 temp_flds[ntemp]   = i;
			 temp_devs[ntemp++] = dev;
		    }
		    else if (dev->data.fld_dtype[i] == DEV_DTYPE_RH &&
			     convert_known(DEV_UNIT_RH,
					   dev->data.fld_units[i]) &&
			     nrh < MAXDEVS)
		    {
			 rh_flds[nrh]   = i;
			 rh_devs[nrh++] = dev;
		    }
	       }
	  }
     skip_me:
	  dev++;
     }

     if (!ntemp)
     {
	  /*
	   *  No outside thermometers located.  See if the pressure sensor
	   *  has temperature correction....
	   */
	  for (i = 0; i < NVALS; i++)
	  {
	       if (dev->data.fld_used[i] &&
		   dev->data.fld_dtype[i] == DEV_DTYPE_TEMP &&
		   convert_known(DEV_UNIT_C, dev->data.fld_units[i]))
	       {
		    temp_flds[ntemp]   = i;
		    temp_devs[ntemp++] = dev;
		    /*
		     *  Found the thermometer on the pressure sensor --
		     *  stop looking.
		     */
		    break;
	       }
	  }
	  if (!ntemp)
	  {
	       if (!nrh)
		    /*
		     *  Well, we can do a correction but it will be awefully
		     *  generic.....
		     */
		    debug("dev_pcor_add(%d): No thermometers nor hygrometers "
			  "located; the barometer correction to sea level "
			  "will be crude, taking only the altitude into "
			  "account; ideally the outside temperature and "
			  "humidity (vapor pressure) should also be "
			  "considered", __LINE__);
	       else
		    debug("dev_pcor_add(%d): No thermometers located; the "
			  "barometer correction to sea level will be crude, "
			  "taking only the altitude and humidity (vapor "
			  "pressure) into account; ideally the outside "
			  "temperature should also be considered", __LINE__);
	  }
	  else if (!nrh)
	       /*
		*  We'll be doing the corrections with sensor's temp
		*/
	       debug("dev_pcor_add(%d): No outside thermometer or hygrometers "
		     "found; the barometer correction to sea level will use "
		     "just the barometer's internal thermometer to generate "
		     "an altitude and temperature based correction; ideally "
		     "outside temperature and humidity (vapor pressure) "
		     "should be used", __LINE__);
     }

     /*
      *  Determine how much space we will need to store all this data
      */
     ssize = sizeof(device_press_adj_t) + sizeof(size_t)*(nrh + ntemp) +
	  sizeof(device_t *)*(nrh + ntemp + 2);

     /*
      *  Now find space for the data.  Use any existing device->pcor
      *  memory if it is large enough.
      */
     dev_lock(device);
     pcor = dev_pcor(device);
     dev_pcor_set(device, NULL);
     dev_unlock(device);

     /*
      *  See if we can use any pre-existing field
      */
     if (pcor && pcor->ssize < ssize)
     {
	  /*
	   *  Too small: toss it
	   */
	  free(pcor);
	  pcor = NULL;
     }
     if (!pcor)
     {
	  /*
	   *  Looks like we need to allocate storage
	   */
	  pcor = (device_press_adj_t *)malloc(ssize);
	  if (!pcor)
	  {
	       debug("dev_pcor_add(%d): Insufficient virtual memory",
		     __LINE__);
	       return(ERR_NOMEM);
	  }
	  pcor->ssize = ssize;
     }

     /*
      *  Set up the data for pressure corrections.
      *  NOTE: do not set ssize! It is set when the structure
      *        is allocated.  And since we may be re-using the
      *        structure, the current pcor->ssize value may be
      *        larger than our locally computed ssize variable.
      */
     pcor->alt_station = (float)altitude;
     pcor->alt_adjust  = 0.0;
     pcor->fld_spare   = ispare;
     pcor->fld_spare2  = ispare2;
     pcor->fld_press   = ipress;
     pcor->ntemp       = ntemp;
     pcor->nrh         = nrh;
     pcor->temp_flds = (size_t *)((char *)pcor +
			          sizeof(device_press_adj_t));
     pcor->rh_flds   = (size_t *)((char *)pcor->temp_flds +
				  sizeof(size_t) * ntemp);
     pcor->temp_devs = (device_t **)((char *)pcor->rh_flds +
				     sizeof(size_t) * nrh);
     pcor->rh_devs   = (device_t **)((char *)pcor->temp_devs +
				     sizeof(device_t *) * (ntemp + 1));
     memmove(pcor->temp_flds, temp_flds, sizeof(size_t) * ntemp);
     memmove(pcor->rh_flds,   rh_flds,   sizeof(size_t) * nrh);
     memmove(pcor->temp_devs, temp_devs, sizeof(device_t *) * ntemp);
     memmove(pcor->rh_devs,   rh_devs,   sizeof(device_t *) * nrh);
     pcor->temp_devs[ntemp] = NULL;
     pcor->rh_devs[nrh] = NULL;

     /*
      *  Lock the device
      */
     dev_lock(device);

     /*
      *  And add this field to the device
      */
     device->data.fld_used[pcor->fld_spare]   = DEV_FLD_USED;
     device->data.fld_dtype[pcor->fld_spare]  = DEV_DTYPE_PRSL;
     device->data.fld_units[pcor->fld_spare]  = device->data.fld_units[ipress]; 
     device->data.fld_format[pcor->fld_spare] = device->data.fld_format[ipress];

     device->data.fld_used[pcor->fld_spare2]   = DEV_FLD_USED;
     device->data.fld_dtype[pcor->fld_spare2]  = DEV_DTYPE_PRSL0;
     device->data.fld_units[pcor->fld_spare2]  = device->data.fld_units[ipress]; 
     device->data.fld_format[pcor->fld_spare2] = device->data.fld_format[ipress];

     /*
      *  Now add a reference to the field to the device structure
      */
     dev_pcor_set(device, pcor);

     /*
      *  And unlock the device
      */
     dev_unlock(device);

     /*
      *  All done
      */
     return(ERR_OK);

#undef MAXDEVS

}
