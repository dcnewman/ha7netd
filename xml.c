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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#if defined(__NO_SPAWN)
#include <stdlib.h>
#endif

#include "err.h"
#include "debug.h"
#include "os.h"
#include "daily.h"
#include "device.h"
#include "ha7net.h"
#include "weather.h"
#include "convert.h"
#include "vapor.h"
#include "xml.h"

static const char preamble[] =
     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
     "<wstation xsi:noNamespaceSchemaLocation=\"wstation.xsd\"\n"
     "          xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
     "          name=\"%s\"\n"
     "          time=\"%s\"\n"
     "          date=\"%s\"\n"
     "          period=\"%u\">\n"
     "\n";

static const char postamble[] = "</wstation>\n";

static void xml_rm(xml_out_t *ctx);
static int xml_mv(xml_out_t *ctx, const char *fname);

static debug_proc_t  our_debug_ap;
static debug_proc_t *debug_proc = our_debug_ap;
static void         *debug_ctx  = NULL;
static int dbglvl     = 0;
static int do_debug   = 0;
static int do_trace   = 0;
static int do_verbose = 0;

static void
our_debug_ap(void *ctx, int reason, const char *fmt, va_list ap)
{

     (void)ctx;
     (void)reason;

     vfprintf(stderr, fmt, ap);
     fputc('\n', stderr);
     fflush(stderr);
}


void
xml_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     dbglvl     = flags;
     do_debug   = ((flags & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     do_trace   = ((flags & DEBUG_TRACE_XML) && debug_proc) ? 1: 0;

     /*
      *  Push the settings down to the HA7NET & device layer
      */
     ha7net_debug_set(proc, ctx, flags);
     dev_debug_set(proc, ctx, flags);
     daily_debug_set(proc, ctx, flags);
}


/*
 *  Log an error to the event log when the debug bits indicate DEBUG_ERRS
 */

static void
debug(const char *fmt, ...)
{
     if (do_debug && debug_proc)
     {
	  va_list ap;

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

static void
detail(const char *fmt, ...)
{
     if (do_verbose && debug_proc)
     {
	  va_list ap;

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


/*
 *  Record non-error/non-warning events
 */

static void
info(const char *fmt, ...)
{
     if (do_debug && debug_proc)
     {
	  va_list ap;

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}


/*
 *  Provide call trace information when the DEBUG_TRACE_HA7NET bit is set
 *  in the debug flags.
 */

static void
trace(const char *fmt, ...)
{
     if (do_debug && debug_proc)
     {
	  va_list ap;

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}

#if defined(DEBUG_CHECK)
#define trace  printf
#define info   printf
#define debug  printf
#define detail printf
#endif

static os_pthread_mutex_t mutex;
static int initialized = 0;
static unsigned long seqno = 0;
static os_pid_t pid = 0;

void
xml_lib_done(void)
{
     if (!initialized)
	  return;
     os_pthread_mutex_destroy(&mutex);
     initialized = 0;
}


int
xml_lib_init()
{
     if (initialized)
	  return(ERR_OK);

     os_pthread_mutex_init(&mutex, NULL);
     os_pthread_mutex_lock(&mutex);
     if (!initialized)
     {
	  pid         = os_getpid();
	  seqno       = 0;
	  initialized = 1;
     }
     os_pthread_mutex_unlock(&mutex);
	 return(ERR_OK);
}


static unsigned long
next_seqno(void)
{
     unsigned long s;

     os_pthread_mutex_lock(&mutex);
     s = seqno++;
     if (seqno > 0x7fffffff)
	  seqno = 0;
     os_pthread_mutex_unlock(&mutex);
     return(s);
}


int
xml_open(xml_out_t *ctx, const weather_station_t *wsinfo, const char *tmpdir)
{
     int fd;
     FILE *fp;
     size_t len;
     unsigned long s;

     if (do_trace)
	  trace("xml_open(%d): Called with ctx=%p, wsinfo=%p, tmpdir=\"%s\" "
		"(%p)",
		__LINE__, ctx, wsinfo, tmpdir ? tmpdir : "(null)", tmpdir);
     /*
      *  Sanity checks
      */
     if (!ctx)
     {
	  debug("xml_open(%d): Invalid call arguments; ctx=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Initializations
      */
     ctx->wsinfo   = wsinfo;
     ctx->fp       = NULL;
     ctx->first    = 0;
     ctx->fname[0] = '\0';

     /*
      *  This should have been done by our caller whilst single-threaded...
      *  I suppose maybe we would all be better off if instead we threw an
      *  error?
      */
     if (!initialized)
     {
	  debug("xml_open(%d): Someone forgot to call xml_lib_init()!  I'll "
		"call it now, but it should really be called while single "
		"threaded...", __LINE__);
	  xml_lib_init();
     }

     /*
      *  Construct the name for the temporary file
      */
     s = next_seqno();
     if (!tmpdir || !(len = strlen(tmpdir)))
	  snprintf(ctx->fname, sizeof(ctx->fname),
		   "./.tmp-%x-%lx.xml", pid, s);
     else
	  snprintf(ctx->fname, sizeof(ctx->fname),
		   "./%s.tmp-%x-%lx.xml", tmpdir, pid, s);

     /*
      *  Open the file
      */
     fd = open(ctx->fname, O_WRONLY | O_CREAT | O_EXCL | O_TEXT, 0600);
     if (fd < 0)
     {
	  debug("xml_open(%d): Unable to open a temporary output "
		"file; open(\"%s\", O_WRONLY|O_CREAT|O_EXCL, 0600) call "
		"failed; errno=%d; %s",
		__LINE__, ctx->fname ? ctx->fname : "(null)", errno,
		strerror(errno));
	  ctx->fname[0] = '\0';
	  return(ERR_NO);
     }

     /*
      *  Attach a stream to the file
      */
     fp = fdopen(fd, "w");
     if (!fp)
     {
	  debug("xml_open(%d): Unable to open a temporary output "
		"file; fdopen(%d, \"w\") call failed; errno=%d; %s",
		__LINE__, fd, errno, strerror(errno));
	  ctx->fname[0] = '\0';
	  return(ERR_NO);
     }

     /*
      * Success
      */
     ctx->fp    = fp;
     ctx->first = 1;

     return(ERR_OK);
}


static void
xml_rm(xml_out_t *ctx)
{
     if (do_trace)
	  trace("xml_rm(%d): Called with ctx=%p", __LINE__);

     if (!ctx)
	  return;

     /*
      *  Close the file first...
      */
     if (ctx->fp)
     {
	  fclose(ctx->fp);
	  ctx->fp = NULL;
     }

     /*
      *  And remove it
      */
     if (ctx->fname[0])
     {
	  remove(ctx->fname);
	  ctx->fname[0] = '\0';
     }
}


int
xml_close(xml_out_t *ctx, int delete, const char *fname)
{
     int istat;

     if (do_trace)
	  trace("xml_close(%d): Called with ctx=%p, delete=%d, "
		"fname=\"%s\" (%p)",
		__LINE__, ctx, delete, fname ? fname : "(null)", fname);

     /*
      *  Sanity checks
      */
     if (!ctx)
     {
	  debug("xml_close(%d): Bad call arguments supplied; ctx=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Just do the delete if requested
      */
     if (delete)
     {
	  xml_rm(ctx);
	  return(ERR_OK);
     }

     /*
      *  Further sanity checks for the non-delete case
      */
     if (!ctx->fp || !ctx->fname[0])
     {
	  debug("xml_close(%d): Incorrect call; the output file to be closed "
		"is not open", __LINE__);
	  istat = ERR_NO;
	  goto done_bad;
     }

     /*
      *  Write the postamble
      */
     if (0 > fprintf(ctx->fp, postamble))
     {
	  debug("xml_close(%d): Error writing to the output file; fprintf() "
		"call failed; errno=%d; %s",
		__LINE__, errno, strerror(errno));
	  istat = ERR_NO;
	  goto done_bad;
     }

     /*
      *  Close the file
      */
     if (fclose(ctx->fp) && !delete)
     {
	  debug("xml_close(%d): Error closing the output file; fclose() "
		"call failed; errno=%d; %s",
		__LINE__, errno, strerror(errno));
	  istat = ERR_NO;
	  goto done_bad;
     }
     ctx->fp = NULL;

     /*
      *  And rename the file, overriding any old file
      */
     if (fname)
     {
	  size_t l;

	  if (rename(ctx->fname, fname))
	  {
	       debug("xml_close(%d): Unable to rename the file; "
		     "rename(\"%s\", \"%s\") call failed; errno=%d; %s",
		     __LINE__, ctx->fname ? ctx->fname : "",
		     fname ? fname : "", errno, strerror(errno));
	       istat = ERR_NO;
	       goto done_bad;

	  }

	  /*
	   *  Record the new file name
	   */
	  l = strlen(fname);
	  if (l >= sizeof(ctx->fname))
	       l = sizeof(ctx->fname) - 1;
	  memcpy(ctx->fname, fname, l);
	  ctx->fname[l] = '\0';
     }
     istat = ERR_OK;
     goto done;

done_bad:
     xml_rm(ctx);
     ctx->fname[0] = '\0';

done:
     ctx->fp = NULL;
     return(istat);
}


int
xml_write(xml_out_t *ctx, device_t *dev, int period, const char *title)
{
     const char *desc, *desc_drv, *qdesc, *qdesc_drv, *qtitle;
     int dispose, dispose_drv, fld_rh, fld_temp, i, istat, j;

     if (do_trace)
	  trace("xml_write(%d): Called with ctx=%p, dev=%p "
		"(dev->romid=\"%s\"), period=%d, title=\"%s\" (%p)",
		__LINE__, ctx, dev, dev ? dev_romid(dev) : "(null)",
		period, title ? title : "(null)", title);
     /*
      *  Sanity checks
      */
     if (!ctx || !dev)
     {
	  debug("xml_write(%d): Invalid call arguments supplied; "
		"ctx=%p, dev=%p", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }
     else if (!ctx->fp)
     {
	  debug("xml_write(%d): Invalid call arguments supplied; "
		"ctx->fp=NULL suggesting that the temporary output file has "
		"yet to be opened via xml_open()", __LINE__);
	  return(ERR_NO);
     }

     /*
      *  Write the XML processing instructions and opening tag
      */
     if (ctx->first)
     {
	  char dateb[64], timeb[64];
	  int hour;
	  const char *loc, *tm_zone;
	  time_t tm;
	  struct tm tmbuf;
	  long tm_gmtoff;

	  tm = time(NULL);
	  localtime_r(&tm, &tmbuf);

	  /*
	   *  Not all platforms have a strftime which supports %z
	   *  (GMT offset in '+' | '-' MMSS).  So, we cook this
	   *  time string up ourselves....
	   */
	  hour = tmbuf.tm_hour % 12;
	  if (!hour && tmbuf.tm_hour > 0)
	       hour = 12;
	  os_tzone(&tm_gmtoff, &tm_zone, dateb, sizeof(dateb));
	  snprintf(timeb, sizeof(timeb), "%d:%02d %s %c%02d%02d (%s)",
		   hour, tmbuf.tm_min, (tmbuf.tm_hour < 12) ? "AM" : "PM",
		   (tm_gmtoff >= 0) ? '+' : '-', abs(tm_gmtoff / 3600),
		   abs(tm_gmtoff / 60) % 60, tm_zone);

	  /*
	   *  For the time being, this seems to work on most platforms
	   */
	  strftime(dateb, sizeof(dateb), "%A, %e %B %G", &tmbuf);

	  if (title)
	  {
	       qtitle = xml_strquote(0, 0, &dispose, title, strlen(title));
	       if (!qtitle)
	       {
		    detail("xml_write(%d): Insufficient virtual "
			   "memory", __LINE__);
		    return(ERR_NOMEM);
	       }
	  }
	  else
	  {
	       dispose = 0;
	       qtitle  = "unknown";
	  }
	  istat = fprintf(ctx->fp, preamble, qtitle, timeb, dateb, period);
	  if (dispose)
	       free((char *)qtitle);
	  if (istat < 0)
	       goto write_error;

	  if (ctx->wsinfo &&
	      (ctx->wsinfo->have_altitude || ctx->wsinfo->longitude[0] ||
	       ctx->wsinfo->latitude[0]))
	  {
	       if (0 > fprintf(ctx->fp, "  <station>\n"))
		    goto write_error;
	       if (ctx->wsinfo->longitude[0] &&
		   0 > fprintf(ctx->fp, "    <longitude v=\"%s\"/>\n",
			       ctx->wsinfo->longitude))
		    goto write_error;
	       if (ctx->wsinfo->latitude[0] &&
		   0 > fprintf(ctx->fp, "    <latitude v=\"%s\"/>\n",
			       ctx->wsinfo->latitude))
		    goto write_error;
	       if (ctx->wsinfo->have_altitude &&
		   0 > fprintf(ctx->fp,
			       "    <altitude v=\"%d\" units=\"%s\"/>\n",
			       ctx->wsinfo->altitude, dev_unitstr(DEV_UNIT_M)))
		    goto write_error;
	       if (0 > fprintf(ctx->fp, "  </station>\n\n"))
		    goto write_error;
	  }
	  ctx->first = 0;
     }

     desc = dev_desc(dev);
     if (desc && desc[0])
     {
	  qdesc = xml_strquote(0, 0, &dispose, desc, dev_dlen(dev));
	  if (!qdesc)
	  {
	       detail("xml_write(%d): Insufficient virtual memory",
		      __LINE__);
	       return(ERR_NOMEM);
	  }
     }
     else
     {
	  qdesc   = "";
	  dispose = 0;
     }

     desc_drv = dev_desc_drv(dev);
     if (desc_drv && desc_drv[0])
     {
	  qdesc_drv = xml_strquote(0, 0, &dispose_drv, desc_drv,
				   dev_dlen_drv(dev));
	  if (!qdesc_drv)
	  {
	       if (dispose)
		    free((char *)qdesc);
	       detail("xml_write(%d): Insufficient virtual memory",
		      __LINE__);
	       return(ERR_NOMEM);
	  }
     }
     else
     {
	  qdesc_drv   = "";
	  dispose_drv = 0;
     }

     istat = fprintf(ctx->fp,
		     "  <sensor id=\"%s\">\n"
		     "    <driver>%s</driver>\n"
		     "    <description>%s</description>\n",
		     dev_romid(dev), qdesc_drv, qdesc);
     if (dispose)
	  free((char *)qdesc);
     if (dispose_drv)
	  free((char *)qdesc_drv);
     if (istat < 0)
	  goto write_error;

     /*
      *  Output <averages p="p1 p2 p3" p-units="s"/>
      */
     if (dev->data.avgs.period[0] > 0)
     {
	  int do_space = 0;
	  int do_first = 1;

	  for (j = NPERS - 1; j >= 0; j--)
	  {
	       if (dev->data.avgs.period[j] <= 0)
		    continue;
	       if (!dev->data.avgs.range_exists[j])
		    continue;
	       if (do_first)
	       {
		    if (0 > fprintf(ctx->fp, "    <averages p=\""))
			 goto write_error;
		    do_first = 0;
		    do_space = 0;
	       }
	       if (do_space)
	       {
		    if (EOF == fputc(' ', ctx->fp))
			 goto write_error;
		    do_space = 0;
	       }
	       if (0 > fprintf(ctx->fp, "%u", dev->data.avgs.period[j]))
		    goto write_error;
	       do_space = 1;
	  }
	  if (!do_first && 0 > fprintf(ctx->fp, "\" p-units=\"%s\"/>\n",
				       dev_unitstr(DEV_UNIT_S)))
	       goto write_error;
     }

     /*
      *  Write the fields
      */
     fld_rh   = NVALS;
     fld_temp = NVALS;

     for (i = 0; i < NVALS; i++)
     {
	  const char *fmt, *units;

	  if (!dev->data.fld_used[i])
	       continue;

	  fmt = dev->data.fld_format[i] ? dev->data.fld_format[i] : "%f";
	  units = dev_unitstr(dev->data.fld_units[i]);

	  /*
	   *  Current value
	   */
	  if (dev->data.time[dev->data.n_current] != DEV_MISSING_TVALUE)
	  {
	       if (0 > fprintf(ctx->fp, "    <value type=\"%s\" v=\"",
			       dev_dtypestr(dev->data.fld_dtype[i])) ||
		   0 > fprintf(ctx->fp, fmt,
			       dev->data.val[i][dev->data.n_current]))
		    goto write_error;
	  }
	  else
	  {
	       /*
		*  Missing value
		*/
	       if (0 > fprintf(ctx->fp, "    <value type=\"%s\" v=\"%c",
			       dev_dtypestr(dev->data.fld_dtype[i]),
			       DEV_MISSING_VALUE))
		   goto write_error;
	  }
	  if (units)
	  {
	       if (0 > fprintf(ctx->fp, "\" units=\"%s\">\n", units))
		    goto write_error;
	  }
	  else
	       if (0 > fprintf(ctx->fp, "\">\n"))
		    goto write_error;

	  /*
	   *  Running averages <averages v="a1 a2 a3" units="xx"/>
	   */
	  if (dev->data.avgs.period[0] > 0)
	  {
	       int do_space = 0;
	       int do_first = 1;

	       for (j = NPERS - 1; j >= 0; j--)
	       {
		    if (dev->data.avgs.period[j] <= 0)
			 continue;
		    if (!dev->data.avgs.range_exists[j])
			 continue;
		    if (do_first)
		    {
			 if (0 > fprintf(ctx->fp, "      <averages v=\""))
			      goto write_error;
			 do_first = 0;
			 do_space = 0;
		    }
		    if (do_space)
		    {
			 if (EOF == fputc(' ', ctx->fp))
			      goto write_error;
			 do_space = 0;
		    }
		    if (0 > fprintf(ctx->fp, fmt, dev->data.avgs.avg[i][j]))
			 goto write_error;
		    do_space = 1;
	       }
	       if (!do_first && 0 > fprintf(ctx->fp, "\" units=\"%s\"/>\n",
					    units))
		    goto write_error;
	  }

	  /*
	   *  Today's high and low
	   */
	  if (dev->data.today.min[i] <= dev->data.today.max[i])
	  {
	       /*
		*  Today's extrema
		*/
	       if (!dev->data.today.tmin_str[i][0])
		    make_timestr(dev->data.today.tmin_str[i],
				 dev->data.today.tmin[i], 0);
	       if (!dev->data.today.tmax_str[i][0])
		    make_timestr(dev->data.today.tmax_str[i],
				 dev->data.today.tmax[i], 0);
	       if (0 > fprintf(ctx->fp, "      <extrema v=\"") ||
		   0 > fprintf(ctx->fp, fmt, dev->data.today.min[i]) ||
		   EOF == fputc(' ', ctx->fp) ||
		   0 > fprintf(ctx->fp, fmt, dev->data.today.max[i]) ||
		   0 > fprintf(ctx->fp, "\" time=\"%s %s\"",
			       dev->data.today.tmin_str[i],
			       dev->data.today.tmax_str[i]))
		    goto write_error;
	       if (units)
	       {
		    if (0 > fprintf(ctx->fp, " units=\"%s\"/>\n", units))
			 goto write_error;
	       }
	       else
		    if (0 > fprintf(ctx->fp, "/>\n"))
			 goto write_error;
	  }

	  /*
	   *  Yesterday's high and low
	   */
	  if (dev->data.yesterday.min[i] <= dev->data.yesterday.max[i])
	  {
	       /*
		*  Yesterday's extrema
		*/
	       if (!dev->data.yesterday.tmin_str[i][0])
		    make_timestr(dev->data.yesterday.tmin_str[i],
				 dev->data.yesterday.tmin[i], 0);
	       if (!dev->data.yesterday.tmax_str[i][0])
		    make_timestr(dev->data.yesterday.tmax_str[i],
				 dev->data.yesterday.tmax[i], 0);
	       if (0 > fprintf(ctx->fp, "      <yesterday>\n"
			       "        <extrema v=\"") ||
		   0 > fprintf(ctx->fp, fmt, dev->data.yesterday.min[i]) ||
		   EOF == fputc(' ', ctx->fp) ||
		   0 > fprintf(ctx->fp, fmt, dev->data.yesterday.max[i]) ||
		   0 > fprintf(ctx->fp, "\" time=\"%s %s\"",
			       dev->data.yesterday.tmin_str[i],
			       dev->data.yesterday.tmax_str[i]))
		    goto write_error;
	       if (units)
	       {
		    if (0 > fprintf(ctx->fp, " units=\"%s\"/>\n"
				    "      </yesterday>\n", units))
			 goto write_error;
	       }
	       else
		    if (0 > fprintf(ctx->fp, "/>\n      </yesterday>\n"))
			 goto write_error;
	  }
	  if (0 > fprintf(ctx->fp, "    </value>\n"))
	       goto write_error;

	  if (dev->data.fld_dtype[i] == DEV_DTYPE_RH &&
	      convert_known(DEV_UNIT_RH, dev->data.fld_units[i]))
	       fld_rh = i;
	  else if (dev->data.fld_dtype[i] == DEV_DTYPE_TEMP &&
		   convert_known(DEV_UNIT_C, dev->data.fld_units[i]))
	       fld_temp = i;
     }

     /*
      *  Output dew point data?
      */
     if (fld_rh < NVALS && fld_temp < NVALS)
     {
	  if (0 > fprintf(ctx->fp, "    <value type=\"%s\" v=\"%.f\" "
			  "units=\"%s\"/>\n",
	  dev_dtypestr(DEV_DTYPE_DEWP),
	  dewpoint(convert_humidity(dev->data.val[fld_rh][dev->data.n_current],
				    dev->data.fld_units[fld_rh],
				    DEV_UNIT_RH),
		   convert_temp(dev->data.val[fld_temp][dev->data.n_current],
				dev->data.fld_units[fld_temp],
				DEV_UNIT_C)),
	  dev_unitstr(DEV_UNIT_C)))
	       goto write_error;
     }

     if (0 < fprintf(ctx->fp, "  </sensor>\n\n"))
	  return(ERR_OK);

write_error:
     debug("xml_write(%d): Error writing temperature data to the "
	   "output file; fprintf() call failed; errno=%d; %s",
	   __LINE__, errno, strerror(errno));
     return(ERR_NO);
}


int
xml_tohtml(xml_out_t *ctx, const char *cmd, const char *xml_fname,
	   int deletexml)
{
     os_argv_t argv;
     char c, buf[1024], *ptr2;
     int buflen, istat, len, percent_seen;
     os_pid_t pid;
     const char *ptr1;

     if (do_trace)
	  trace("xml_tohtml(%d): Called with ctx=%p, cmd=\"%s\" (%p), "
		"xml_fname=\"%s\" (%p), deletexml=%d",
		__LINE__, ctx, cmd ? cmd : "(null)", cmd,
		xml_fname ? xml_fname : "(null)", xml_fname, deletexml);

     if (!xml_fname && (!ctx || !ctx->fname[0]))
     {
	  /*
	   *  We lack a name for the XML file to transform!
	   */
	  debug("xml_tohtml(%d): Invalid call arguments; xml_fname=NULL "
		"and ctx=%p and ctx->fname[0]=0x%02x; we don't know what the "
		"input XML file is", __LINE__, ctx, ctx ? ctx->fname[0] : 0);
	  return(ERR_BADARGS);
     }

     /*
      *  Close the file if it is not closed already
      */
     if (ctx && ctx->fp)
     {
	  istat = xml_close(ctx, 0, xml_fname);
	  if (istat != ERR_OK)
	  {
	       detail("xml_tohtml(%d): Unable to close the temporary data "
		      "file and rename it to \"%s\"; xml_close() returned %d; "
		      "%s", __LINE__, xml_fname ? xml_fname : "", istat,
		      err_strerror(istat));
	       goto done;
	  }
     }

     /*
      *  Determine the name of the XML file to transform
      */
     if (!xml_fname)
	  xml_fname = ctx->fname;

     /*
      *  Replace %x with the name of the XML file.  We take a little
      *  os_dependent short cutting here.  On Windows, the argv[] list
      *  is ignored.  On Unix the command line is ignored....
      */
     buflen = sizeof(buf) - 1;
     percent_seen = 0;
     ptr2 = buf;
     ptr1 = cmd;
     while ((c = *ptr1++) && (buflen > 0))
     {
	  if (percent_seen)
	  {
	       if (c == 'x')
	       {
		    if (xml_fname)
		    {
			 len = strlen(xml_fname);
			 if (len <= buflen)
			 {
			      memcpy(ptr2, xml_fname, len);
			      ptr2    += len;
			      buflen -= len;
			 }
		    }
	       }
#if 0
	       else if (c == 'h')
	       {
		    if (html_fname)
		    {
			 len = strlen(html_fname);
			 if (len <= buflen)
			 {
			      memcpy(ptr2, html_fname, len);
			      ptr2    += len;
			      buflen -= len;
			 }
			 else
			      break;
		    }
	       }
#endif
	       else if (c == '%')
	       {
		    *ptr2++ = '%';
		    buflen--;
	       }
	       else
	       {
		    if (buflen >= 2)
		    {
			 *ptr2++ = '%';
			 *ptr2++ = c;
			 buflen -= 2;
		    }
		    else
			 break;
	       }
	       percent_seen = 0;
	  }
	  else
	  {
	       if (c == '%')
		    percent_seen = 1;
	       else
	       {
		    *ptr2++ = c;
		    buflen--;
	       }
	  }
	  if (buflen <= 0)
	       break;
     }
     if (*ptr1)
     {
	  debug("xml_tohtml(%d): Internal formatting buffer is too short "
		"to format the XML to HTML conversion command", __LINE__);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Don't forget to NUL terminate the buffer
      */
     *ptr2 = '\0';

     /*
      *  Now execute the command
      */
#if !defined(__NO_SPAWN)
     istat = os_spawn_init(buf, &argv);
     if (istat)
     {
	  debug("xml_tohtml(%d): Unable to construct an argv[] list from "
		"the command line, \"%s\"", __LINE__, buf ? buf : "(null)");
	  istat = ERR_NO;
	  goto done;
     }
     pid = os_spawn_nowait(buf, &argv, "INFILE", ctx->fname, NULL);
     if (!pid)
     {
	  debug("xml_tohtml(%d): Attempt to execute the command \"%s\" "
		"failed; errno=%d; %s", __LINE__, buf, errno, strerror(errno));
	  istat = ERR_NO;
     }
     else
	  istat = ERR_OK;
     os_spawn_free(&argv);
#else
     debug("xml_tohtml(%d): Executing the command \"%s\"", __LINE__,
           buf ? buf : "(null)");
     istat = system(buf);
     debug("xml_tohtml(%d): Execution result is %d", __LINE__, istat);
#endif

done:
     /*
      *  Delete the input XML file?
      */
     if (deletexml)
     {
	  if (ctx && ctx->fname[0])
	       xml_rm(ctx);
	  else if (xml_fname)
	       remove(xml_fname);
     }

     return(istat);
}
