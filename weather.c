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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "err.h"
#include "debug.h"
#include "os.h"
#include "math.h"
#include "ha7net.h"
#include "weather.h"
#include "daily.h"
#include "xml.h"

static os_shutdown_t *shutdown_info = NULL;
static int            shutdown_flag = 0;

static void our_debug_ap(void *ctx, int reason, const char *fmt, va_list ap);

typedef struct {
     device_t *dev;
     size_t    fld;
} column_t;

static void debug(const char *fmt, ...);
static void detail(const char *fmt, ...);
static void info(const char *fmt, ...);
static void trace(const char *fmt, ...);
static int weather_data_fname(char **fname, time_t t, size_t days_ago,
  const char *fpath);
static int weather_data_read(device_t *devices, size_t days_ago,
  const char *fpath);
static int weather_data_write(device_t *devices, time_t tavg, int *first,
  const char *fpath);
static int weather_xml_write(device_t *devices, int period,
  weather_info_t *winfo);

static int weather_list_record(device_t *devices, ha7net_t *ha7net, int period,
  weather_info_t *tinfo);
static int weather_writedat(device_t *devices, time_t this_time,
  const char *fpath);

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
weather_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     dbglvl     = flags;
     do_debug   = ((flags & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     do_trace   = ((flags & DEBUG_TRACE_WEATHER) && debug_proc) ? 1: 0;

     /*
      *  Push the settings down to the HA7NET & device layer
      */
     dev_debug_set(proc, ctx, flags);
     xml_debug_set(proc, ctx, flags);
     ha7net_debug_set(proc, ctx, flags);
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

static int
weather_data_fname(char **fname, time_t t, size_t days_ago, const char *fpath)
{
     size_t len;
     char *ptr;
     struct tm this_tm;
     int val;

     /*
      *  Date & time info
      */
     if ((time_t)t == 0)
	  t = time(NULL);
     localtime_r(&t, &this_tm);
     if (days_ago)
     {
	  /*
	   *  Get a time_t value which is 24 hrs * days_ago in the past
	   */
	  this_tm.tm_mday -= days_ago;
	  t = mktime(&this_tm);

	  /*
	   *  Now get a struct tm corresponding to the time_t in the past
	   */
	  localtime_r(&t, &this_tm);
     }

     /*
      *  Return now if we have no means of returning the file name
      */
     if (!fname)
	  return(ERR_OK);

     /*
      *  Data file name will be: <ha7net-device-name>-yyyymmdd.dat
      */

     /*
      *  Build the file path: "./" <ha7net-device-name> "-yyyymmdd.dat"
      */
     if (!fpath)
     {
	  len   = 0;
	  fpath = "";
     }
     else
	  len = strlen(fpath);

     *fname = (char *)malloc(2 + len + 13 + 1);
     if (!(*fname))
     {
	  debug("weather_data_fname(%d): Insufficient virtual memory",
		__LINE__);
	  return(ERR_NOMEM);
     }

     /*
      *  Prefix
      */
     ptr = *fname;
     *ptr++ = '.';
     *ptr++ = '/';
     memcpy(ptr, fpath, len);
     ptr += len;
     *ptr++ = '-';

     /*
      *  Four digit year
      */
     val = this_tm.tm_year + 1900;
     ptr[3] = '0' + val % 10; val /= 10;
     ptr[2] = '0' + val % 10; val /= 10;
     ptr[1] = '0' + val % 10; val /= 10;
     ptr[0] = '0' + val % 10; /* Modulus *should* be unnecessary */
     ptr += 4;

     /*
      *  Two digit month
      */
     val = this_tm.tm_mon + 1;
     ptr[1] = '0' + val % 10; val /= 10;
     ptr[0] = '0' + val % 10;
     ptr += 2;

     /*
      *  Two digit day of the month
      */
     val = this_tm.tm_mday;
     ptr[1] = '0' + val % 10; val /= 10;
     ptr[0] = '0' + val % 10;
     ptr += 2;

     /*
      *  Append ".dat"
      */
     *ptr++ = '.';
     *ptr++ = 'd';
     *ptr++ = 'a';
     *ptr++ = 't';

     /*
      *  And, of course, a NUL terminator
      */
     *ptr = '\0';

     /*
      *  All done
      */
     return(ERR_OK);
}


static int
weather_data_read(device_t *devices, size_t days_ago, const char *fpath)
{
     int advance_n, e, es, fd, fld, istat, last_fld, r, s, state;
     char c, *fname, line[1024], romid[OWIRE_ID_LEN+1];
     size_t colmax, colnum, n;
     column_t *columns;
     device_t *dev, *last_dev;
     float fval;
     ssize_t i, missing, nread, romid_len;
     time_t tval;
     static const int is_hexdigit[256] = {
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
	  0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

     if (do_trace)
	  trace("weather_data_read(%d): Called with devices=%p, days_ago=%u, "
		"fpath=\"%s\" (%p)",
		__LINE__, devices, days_ago, fpath ? fpath : "(null)", fpath);

     if (!devices)
     {
	  debug("weather_data_read(%d): Invalid call arguments; devices=NULL",
		__LINE__, devices);
	  return(ERR_BADARGS);
     }

     /*
      *  Initializations
      */
     colmax  = 0;
     columns = NULL;
     fname   = NULL;
     fd      = -1;

     fname = NULL;
     istat = weather_data_fname(&fname, (time_t)0, days_ago, fpath);
     if (istat != ERR_OK)
     {
	  debug("weather_data_read(%d): Unable to generate a data file name; "
		"weather_data_fname(%d) returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  goto done;
     }
     else if (!fname)
     {
	  debug("weather_data_read(%d): weather_data_fname() returned a "
		"success but no file name pointer; coding error?",
		__LINE__);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Now open the file in read mode
      */
     fd = open(fname, O_RDONLY | O_TEXT, 0);
     if (fd < 0)
     {
	  if (!os_fexists(fname))
	       /*
		*  File does not exist
		*/
	       istat = ERR_EOM;
	  else
	  {
	       debug("weather_data_read(%d): Unable to open a data file; "
		     "open(\"%s\", O_RDONLY, 0) call failed; errno=%d; %s",
		     __LINE__, fname ? fname : "", errno, strerror(errno));
	       istat = ERR_NO;
	  }
	  goto done;
     }

     /*
      *  Parse the comment section of the data file.  It should look like
      *
      *  #<colnum>:<ROM id>:<fmt>:<units>:<comment>
      *
      *  For each column > 1, we need to know the ROM id of the device
      *  to associate data for that column with.  The device may no longer
      *  exist in which case we ignore data for that column.
      */

/*
 *  Parsing states
 */
#define S_BOL      0
#define S_COLNUM   1
#define S_ROMID    2
#define S_SKIP2EOL 3
#define S_DONE     4
#define S_SKIPLWSP 5
#define S_VALUE    6

     /*
      *  Input buffer state
      */
     nread = 0;
     i     = 0;

     /*
      *  Index of the next data point....  We set this once, way
      *  before the loop which parses the data.  Reason being that
      *  we may, part way through the data file encounter a new
      *  set of comments describing new devices -- the result of
      *  the data logger being restarted.  When we encounter such
      *  a case, we come back to the comment parsing loop BUT we
      *  want to preserve the data index.  Hence why we set it here
      *  and not at the head of the data parsing loop.
      */
     n = NPAST;

     /*
      *  State information for parsing a comment section of the data file
      */
     state    = S_BOL;
     last_dev = NULL;
     last_fld = -1;

read_loop_1:
     while (i < nread && state != S_DONE)
     {
	  c = line[i++];
	  switch (state)
	  {
	  case S_BOL :
	       if (c != '#')
	       {
		    --i;
		    state = S_DONE;
	       }
	       else
	       {
		    state  = S_COLNUM;
		    colnum = 0;
	       }
	       break;

	  case S_COLNUM :
	       switch (c)
	       {
	       case '0' :
	       case '1' :
	       case '2' :
	       case '3' :
	       case '4' :
	       case '5' :
	       case '6' :
	       case '7' :
	       case '8' :
	       case '9' :
		    colnum = colnum * 10 + (int)(c - '0');
		    break;

	       case ':' :
		    if (colnum > 0)
		    {
			 state = S_ROMID;
			 romid_len = 0;
		    }
		    else
			 state = S_SKIP2EOL;
		    break;

	       default :
		    state = S_SKIP2EOL;
		    break;
	       }
	       break;

	  case S_ROMID :
	       if (romid_len < OWIRE_ID_LEN)
	       {
		    if (is_hexdigit[(unsigned char)c])
			 romid[romid_len++] = c;
		    else
			 state = S_SKIP2EOL;
	       }
	       else if (romid_len == OWIRE_ID_LEN && c == ':')
	       {
		    /*
		     *  We now have the column number AND ROM id
		     */
		    state = S_SKIP2EOL;

		    /*
		     *  Cannonicalize the ROM id.  This in case the
		     *  cannonicalization has changed across resets.
		     */
		    dev_romid_cannonical(romid, OWIRE_ID_LEN+1,
					 romid, OWIRE_ID_LEN);

		    /*
		     *  Find the matching device in the device list
		     */
		    dev = devices;
		    while (!dev_flag_test(dev, DEV_FLAGS_END))
		    {
			 if (dev_flag_test(dev, DEV_FLAGS_IGNORE |
					   DEV_FLAGS_ISSUB) ||
			     !dev_flag_test(dev, DEV_FLAGS_INITIALIZED) ||
			     memcmp(dev_romid(dev), romid, OWIRE_ID_LEN))
			      goto skip_me;
			 if (dev != last_dev)
			 {
			      last_dev = dev;
			      last_fld = -1;
			 }
			 for (fld = last_fld + 1; fld < NVALS; fld++)
			      if (dev->data.fld_used[fld] == DEV_FLD_USED)
				   break;
			 if (fld == NVALS)
			 {
			      debug("weather_data_read(%d): Data file has too "
				    "many columns of values for the device "
				    "with ROM id %.*s",
				    __LINE__, OWIRE_ID_LEN, romid);
			      istat = ERR_NO;
			      goto done;
			 }
			 last_fld = fld;
			 if (colnum >= colmax)
			 {
			      size_t colmax_new =
				   200 * (int)((colnum + 199) / 200);
			      column_t *tmp = (column_t *)
				   realloc(columns,
					   colmax_new * sizeof(column_t));
			      if (!tmp)
			      {
				   debug("weather_data_read(%d): Insufficient "
					 "virtual memory", __LINE__);
				   istat = ERR_NOMEM;
			      }
			      memset((char *)tmp + colmax * sizeof(column_t),
				     0,
				     (colmax_new - colmax) * sizeof(column_t));
			      columns = tmp;
			      colmax  = colmax_new;
			 }
			 columns[colnum].dev = dev;
			 columns[colnum].fld = fld;

			 /*
			  *  End the device search
			  */
			 break;

		    skip_me:
			 dev++;
		    }
	       }
	       else
		    state = S_SKIP2EOL;
	       break;

	  case S_SKIP2EOL :
	       if (c == '\n')
		    state = S_BOL;
	       break;

	  default :
	       debug("weather_data_read(%d): Coding error; while parsing the "
		     "comment section of the file \"%s\", we found that the "
		     "parsing state had the unknown value %d",
		     __LINE__, fname ? fname : "(null)", state);
	       istat = ERR_NO;
	       goto done;
	  }
     }

     /*
      *  while() loop over for the time being.  Is it over because
      *  we've reached the end of the comments, or because we've
      *  exhausted the input buffer and need to read more data?
      */
     if (state != S_DONE)
     {
	  /*
	   *  Still processing comments and we've exhausted the input buffer.
	   *  Read more file text to and resume comment processing.
	   */
	  if (0 < (nread = read(fd, line, sizeof(line))))
	  {
	       i = 0;
	       goto read_loop_1;
	  }
	  if (nread == 0)
	       /*
		*  Premature eof but we can live with that
		*/
	       istat = ERR_OK;
	  else
	  {
	       /*
		*  I/O error of some sort
		*/
	       debug("weather_data_read(%d): Encountered an I/O error "
		     "while reading the data file \"%s\"; errno=%d; %s",
		     __LINE__, fname ? fname : "(null)",
		     errno, strerror(errno));
	       istat = ERR_READ;
	  }
	  goto done;
     }

     /*
      *  If we're here, then we've reached the end of a comment section.
      */

     /*
      *  Did we find any columns of interest?
      */
     if (!columns)
     {
	  debug("weather_data_read(%d): The data file is not appropriately "
		"formatted or does not contain any data", __LINE__);
	  istat = ERR_OK;
	  goto done;
     }

     /*
      *  Now parse the data within the file.
      *  Note that we've already set n=NPAST and we do not want to reset it.
      */
     colnum    = 0;
     advance_n = 1;
     state     = S_BOL;

read_loop_2:
     while (i < nread)
     {
	  c = line[i++];
	  switch (state)
	  {
	  case S_SKIP2EOL :
	       if (c == '\n')
		    state = S_BOL;
	       break;
	       
	  case S_BOL :
	       colnum    = 0;
	       advance_n = 1;
	       state     = S_SKIPLWSP;
	       /*
		*  Fall through to next case
		*/

	  case S_SKIPLWSP :
	       if (c == ' ' || c == '\t' || c == '\r')
		    break;
	       else if (c == '\n')
	       {
		    colnum = 0;
		    break;
	       }
	       else if (c == '#' && colnum == 0)
	       {
		    /*
		     *  Comment...? at the start of the line...?
		     *  The data recording was stopped and then
		     *  restarted.  Dump columns, parse these
		     *  new comments, and then continue reading
		     *  data.
		     */
		    memset(columns, 0, colmax * sizeof(column_t));
		    state    = S_COLNUM;
		    last_dev = NULL;
		    last_fld = -1;
		    colnum   = 0;
		    goto read_loop_1;
	       }

	       /*
		*  New column of data
		*/
	       if (colnum == 0)
	       {
		    advance_n = 1;
		    colnum    = 1;
		    tval      = (time_t)0;
	       }
	       else
	       {
		    colnum++;
		    missing = 0;   /* Missing value    */
		    fval    = 0.0; /* Value            */
		    r       = 0;   /* 1/10, 1/100, ... */
		    s       = 1;   /* Mantissa sign    */
		    es      = 0;   /* Exponent sign    */
	       }

	       /*
		*  Fall through to the next state
		*/
	       state = S_VALUE;

	  case S_VALUE :
	       if (colnum == 1)
	       {
		    if ('0' <= c && c <= '9')
			 tval = tval * 10 + (time_t)(c - '0');
		    else
		    {
			 if (c == ' ' || c == '\t')
			      state = S_SKIPLWSP;
			 else
			      state = S_SKIP2EOL;
		    }
	       }
	       else
	       {
		    switch(c)
		    {
		    case DEV_MISSING_VALUE :
			 missing = 1;
			 break;

		    case '0' :
		    case '1' :
		    case '2' :
		    case '3' :
		    case '4' :
		    case '5' :
		    case '6' :
		    case '7' :
		    case '8' :
		    case '9' :
			 if (es)
			      /*
			       *  Mantissa
			       */
			      e = e * 10 + (int)(c - '0');
			 else if (r == 0)
			      fval = fval * 10.0 + (float)(c - '0');
			 else
			 {
			      /*
			       *  We've seen a decimal point already
			       */
			      fval += (float)(c - '0') / (float)r;
			      r *= 10;
			 }
			 break;

		    case 'E' :
		    case 'D' :
			 e  = 0;  /* Exponent        */
			 es = 1;  /* Exponent sign + */
			 break;

		    case '.' :
		    case ',' :
			 if (es)
			 {
			      /*
			       *  Decimal points not allowed in the exponent
			       */
			      detail("weather_data_read(%d): Encountered a "
				     "value expressed in scientific notation "
				     "but with a decimal point in the "
				     "exponent; file is \"%s\"",
				     __LINE__, fname ? fname : "(null)");
			      state = S_SKIP2EOL;
			      break;
			 }
			 r = 10;
			 break;

		    case '-' :
			 if (es)
			      es = -1; /* Negative exponent */
			 else
			      s = -1;  /* Negative mantissa */
			 break;

		    case '+' :
			 if (es)
			      es = 1; /* Positive exponent */
			 else
			      s = 1;  /* Positive mantissa */
			 break;

		    case ' ' :
		    case '\t' :
			 state = S_SKIPLWSP;
			 break;

		    case '\r' :
			 state = S_SKIP2EOL;
			 break;

		    case '\n' :
			 state = S_BOL;
			 break;

		    default :
			 state = S_SKIP2EOL;
			 break;
		    }

		    if (state == S_VALUE)
			 /*
			  *  Still parsing the value
			  */
			 break;
		    if (tval == (time_t)0 || colnum >= colmax)
		    {
			 /*
			  *  Not good
			  */
			 if (state != S_BOL)
			      state = S_SKIP2EOL;
			 break;
		    }
		    else if (!(dev = columns[colnum].dev))
		    {
			 /*
			  *  The data for this column corresponds to a
			  *  device not turned up by the 1-Wire bus search
			  */
			 break;
		    }
		    else if ((fld = columns[colnum].fld) >= NVALS)
		    {
			 debug("weather_data_read(%d): Coding error; we've "
			       "found that columns[%d].fld = %d which "
			       "exceeds NVALS=%d",
			       __LINE__, colnum, fld, NVALS);
			 istat = ERR_NO;
			 goto done;
		    }

		    /*
		     *  Now that we have an acceptable data value, go ahead
		     *  and advance our index into the list of values.
		     *  Note that the initial value for n is NPAST so that
		     *  on the first advance we set it to the index value 0.
		     */
		    if (advance_n)
		    {
			 advance_n = 0;
			 n = (n < (NPAST - 1)) ? n + 1 : 0;
		    }
		    dev->data.n_current = n;

		    if (missing)
		    {
			 /*
			  *  Missing value
			  */
			 dev->data.time[n]     = DEV_MISSING_TVALUE;
			 dev->data.val[fld][n] = 0.0;
		    }
		    else
		    {
			 /*
			  *  Adjust the sign if necessary
			  */
			 if (es && e)
			 {
			      if (es < 0)
				   e = -e;
			      fval = fval * (float)pow(10.0, (double)e);
			 }
			 if (s == -1)
			      fval = -fval;

			 /*
			  *  Store the value
			  */
			 dev->data.time[n]     = tval;
			 dev->data.val[fld][n] = fval;

			 /*
			  *  Handle extrema
			  */
			 if (dev->data.today.min[fld] > fval)
			 {
			      dev->data.today.min[fld]  = fval;
			      dev->data.today.tmin[fld] = tval;
			 }
			 if (dev->data.today.max[fld] < fval)
			 {
			      dev->data.today.max[fld]  = fval;
			      dev->data.today.tmax[fld] = tval;
			 }
		    }
		    missing = 0;
	       }
	       break;

	  default :
	       debug("weather_data_read(%d): Coding error; while parsing the "
		     "comment section of a data file, we found that the "
		     "parsing state had the unknown value %d",
		     __LINE__, state);
	       istat = ERR_NO;
	       goto done;
	  }
     }

     /*
      *  Refill the read buffer
      */
     if (0 < (nread = read(fd, line, sizeof(line))))
     {
	  i = 0;
	  goto read_loop_2;
     }

#undef S_BOL
#undef S_COLNUM
#undef S_ROMID
#undef S_SKIP2EOL
#undef S_DONE
#undef S_SKIPLWSP
#undef S_VALUE

     /*
      *  EOF achieved
      */
     if (nread >= 0)
	  istat = ERR_OK;
     else
     {
	  debug("weather_data_read(%d): Encountered an I/O error while "
		"reading the data file \"%s\"; errno=%d; %s",
		__LINE__, fname ? fname : "(null)", errno, strerror(errno));
	  istat = ERR_READ;
     }

done:
     /*
      *  Release our resources
      */
     if (fd >= 0)
	  close(fd);
     if (fname)
	  free(fname);
     if (columns)
	  free(columns);

     return(istat);
}


static int
weather_data_write(device_t *devices, time_t tavg, int *first,
		   const char *fpath)
{
     device_t *dev;
     int fd, missing;
     char *fname;
     FILE *fp;
     size_t i, istat, len, n;
     struct stat sbuf;

     if (do_trace)
	  trace("weather_data_write(%d): Called with devices=%p, tavg=%u, "
		"first=%p; *first=%d, fpath=\"%s\" (%p)",
		__LINE__, devices, tavg, first, first ? *first : 0,
		fpath ? fpath : "(null)", fpath);

     if (!devices)
     {
	  debug("weather_data_write(%d): Invalid call arguments; devices=NULL",
		__LINE__, devices);
	  return(ERR_BADARGS);
     }

     fname = NULL;
     istat = weather_data_fname(&fname, tavg, 0, fpath);
     if (istat != ERR_OK)
     {
	  debug("weather_data_write(%d): Unable to generate a data file name; "
		"weather_data_fname(%d) returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  return(istat);
     }
     else if (!fname)
     {
	  debug("weather_data_write(%d): weather_data_fname() returned a "
		"success but no file name pointer; coding error?",
		__LINE__);
	  return(ERR_NO);
     }

     /*
      *  Now open the file in append mode
      */
     fd = open(fname, O_WRONLY | O_APPEND | O_CREAT | O_TEXT, 0644);
     if (fd < 0)
     {
	  debug("weather_data_write(%d): Unable to open a data file; "
		"open(\"%s\", O_APPEND | O_CREAT, 0644) call failed; "
		"errno=%d; %s",
		__LINE__, fname ? fname : "(null)", errno, strerror(errno));
	  free(fname);
	  return(ERR_NO);
     }

     /*
      *  Free up the file name
      */
     free(fname);
     fname = NULL;

     /*
      *  Associate a stream with the file
      */
     fp = fdopen(fd, "a");
     if (!fp)
     {
	  debug("weather_data_write(%d): Unable to open the data file; "
		"fdopen(%d, \"a+\") call failed; errno=%d; %s",
		__LINE__, fd, errno, strerror(errno));
	  return(ERR_NO);
     }

     /*
      *  If the file is new, then write a preamble into it
      */
     if ((first && *first) || (!fstat(fileno(fp), &sbuf) && sbuf.st_size == 0))
     {
	  long tm_gmtoff;
	  const char *tm_zone;
	  char zbuf[64];

	  if (first)
	       *first = 0;

	  os_tzone(&tm_gmtoff, &tm_zone, zbuf, sizeof(zbuf));
	  fprintf(fp,
"#ha7netd v%d.%d (compiled " __DATE__ " " __TIME__ ")\n"
"#All time units are seconds since 00:00 1 Jan 1970 %c%02d%02d (%s)\n"
"#<column>:<ROM id>:<format>:<units>:<type>:<description>\n"
"#1::%%u:s:time_t:Seconds since 1 Jan 1970 00:00\n",
		  WEATHER_VERSION_MAJOR, WEATHER_VERSION_MINOR,
		  (tm_gmtoff >= 0) ? '+' : '-', abs(tm_gmtoff / 3600),
		  abs(tm_gmtoff / 60) % 60, tm_zone);

	  dev = devices;
	  n = 1;
	  while (!dev_flag_test(dev, DEV_FLAGS_END))
	  {
	       if (dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
		   !dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
		    goto skip_me1;
	       for (i = 0; i < NVALS; i++)
	       {
		    if (dev->data.fld_used[i] != DEV_FLD_USED)
			 continue;
		    n++;
		    fprintf(fp, "#%zu:%s:%s:%s:%s:%s\n",
			    n, dev_romid(dev),
			    dev->data.fld_format[i] ?
			      dev->data.fld_format[i] : "%f",
			    dev_unitstr(dev->data.fld_units[i]),
			    dev_dtypestr(dev->data.fld_dtype[i]),
			    dev_dtypedescstr(dev->data.fld_dtype[i]));
	       }
	  skip_me1:
	       dev++;
	  }
     }

     /*
      *  Time stamp
      */
     if (0 > fprintf(fp, "%ld", tavg))
	  goto write_error;

     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
	      !dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       goto skip_me2;
	  n = dev->data.n_current;
	  for (i = 0; i < NVALS; i++)
	  {
	       if (dev->data.fld_used[i] != DEV_FLD_USED)
		    continue;

	       if (EOF == fputc(' ', fp))
		    goto write_error;

	       if (dev->data.time[n] != DEV_MISSING_TVALUE)
	       {
		    if (dev->data.fld_format[i])
		    {
			 if (0 > fprintf(fp, dev->data.fld_format[i],
					 dev->data.val[i][n]))
			      goto write_error;
		    }
		    else
			 if (0 > fprintf(fp, "%f", dev->data.val[i][n]))
			      goto write_error;
	       }
	       else
	       {
		    if (EOF == fputc(DEV_MISSING_VALUE, fp))
			 goto write_error;
	       }
	  }
     skip_me2:
	  dev++;
     }

     /*
      *  And a record terminator
      */
     if (EOF == fputc('\n', fp))
	  goto write_error;

     /*
      *  Flush the data
      */
     if (fsync(fileno(fp)))
	 goto write_error;

     /*
      *  And finally, close the file
      */
     if (!fclose(fp))
	  return(ERR_OK);

     /*
      *  Error on close; fall through to write_error label
      */
     fp = NULL;

write_error:
     if (fp)
	  fclose(fp);
     debug("weather_data_write(%d): A write error occurred whilst appending "
	   "data to the data file; fprintf() call failed; errno=%d; %s",
	   __LINE__, errno, strerror(errno));
     return(ERR_NO);
}


static int
weather_xml_write(device_t *devices, int period, weather_info_t *winfo)
{
     xml_out_t ctx;
     device_t *dev;
     int istat;

     if (do_trace)
	  trace("weather_xml_write(%d): Called with devices=%p, period=%d, "
		"winfo=%p", __LINE__, devices, period, winfo);

     /*
      *  Test our inputs
      */
     if (!devices || !winfo)
     {
	  debug("weather_xml_write(%d): Invalid call arguments supplied; "
		"devices=%p, winfo=%p", __LINE__, devices, winfo);
	  return(ERR_BADARGS);
     }

     /*
      *  Open a temporary file
      */
     istat = xml_open(&ctx, &winfo->wsinfo, winfo->fname_prefix);
     if (istat != ERR_OK)
     {
	  detail("weather_xml_write(%d): Unable to open a temporary output "
		 "file; xml_open() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Write the data to the XML file
      */
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  /*
	   *  Ignore devices which should not be probed
	   */
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
	      !dev_flag_test(dev, DEV_FLAGS_INITIALIZED) ||
	      dev->data.time[dev->data.n_current] == DEV_MISSING_TVALUE)
	       goto skip_me;

	  /*
	   *  Write the record
	   */
	  istat = xml_write(&ctx, dev, period, winfo->title);
	  if (istat != ERR_OK)
	       detail("weather_xml_write(%d): Unable to record data for the "
		      "device with id=\"%s\" (%s); xml_write() returned %d; "
		      "%s", __LINE__, dev_romid(dev),
		      dev_strfcode(dev_fcode(dev)), istat,
		      err_strerror(istat));
     skip_me:
	  dev++;
     }

     /*
      *  Finally produce a web page of the current data
      */
     istat = xml_tohtml(&ctx, winfo->cmd, NULL, 0);
     if (istat != ERR_OK)
	  detail("weather_xml_write(%d): Error generating HTML output; "
		 "xml_tohtml() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));

     /*
      *  All done
      */
done:
     return(istat);
}

static int
weather_list_record(device_t *devices, ha7net_t *ha7net, int period,
		    weather_info_t *winfo)
{
     device_t *dev;
     int flags, istat;
     time_t t0, t1, tavg;

     if (do_trace)
	  trace("weather_list_record(%d): Called with devices=%p, ha7net=%p, "
		"period=%d, winfo=%p",
		__LINE__, devices, ha7net, period, winfo);

     /*
      *  Sanity check
      */
     if (!devices || !ha7net || !winfo)
     {
	  debug("weather_list_record(%d): Bad call arguments supplied; "
		"devices=%p, ha7net=%p, winfo=%p",
		__LINE__, devices, ha7net, winfo);
	  return(ERR_BADARGS);
     }

     /*
      *  Loop over the list of devices, gathering current readings
      */
     t0 = time(NULL);
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  /*
	   *  Check for a shutdown request: we explicitly do this within
	   *  this loop as device reads can be slow.
	   */
	  if (shutdown_flag)
	  {
	       ha7net_releaselock(ha7net);
	       return(ERR_OK);
	  }

	  /*
	   *  Ignore devices which should not be probed
	   */
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
	      !dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       goto skip_me;

	  /*
	   *  Get the current measurements from this device
	   */
	  istat = dev_read(ha7net, dev, 0);
	  if (istat != ERR_OK)
	  {
	       debug("weather_list_record(%d): Unable to read the device with "
		     "id=\"%s\" (%s); istat=%d; %s",
		      __LINE__, dev_romid(dev), dev_strfcode(dev_fcode(dev)),
		     istat, err_strerror(istat));
	       goto skip_me;
	  }

     skip_me:
	  dev++;
     }
     t1 = time(NULL);

     /*
      *  Release any 1-Wire bus master lock
      */
     istat = ha7net_releaselock(ha7net);

     /*
      *  And another shutdown check
      */
     if (shutdown_flag)
	  return(ERR_OK);

     /*
      *  Deal with any pressure corrections
      */
     if (winfo->have_pcor)
     {
	  dev = devices;
	  while (!dev_flag_test(dev, DEV_FLAGS_END))
	  {
	       if (dev_pcor(dev))
		    dev_pcor_adjust(dev, period);
	       dev++;
	  }
     }
     
     /*
      *  Now, write a single data record to the cumulative record
      */
     tavg = t0 + (int)(difftime(t1, t0) / 2.0);
     istat = weather_data_write(devices, tavg, &winfo->first,
				winfo->fname_prefix);
     if (istat != ERR_OK)
	  detail("weather_list_record(%d): Error writing data to the "
		 "cumulative data file; weather_data_write() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));

     /*
      *  Write the XML data
      */
     if (winfo->cmd && winfo->cmd[0])
     {
	  int istat2;

	  istat2 = weather_xml_write(devices, period, winfo);
	  if (istat != ERR_OK)
	       detail("weather_list_record(%d): Error writing current data to "
		      "an XML file and transforming to HTML; "
		      "weather_xml_write() returned %d; %s",
		      __LINE__, istat, err_strerror(istat));
	  if (istat == ERR_OK)
	       istat = istat2;
     }

     /*
      *  All done
      */
     return(istat);
}


int
weather_main(weather_info_t *winfo)
{
     int attempts, dt, ha7net_initialized, istat, period;
     device_t *dev, *devices;
     size_t fails, ndevices, nlogical;
     const char *free_prefix;
     ha7net_t ha7net;
     time_t t0;

     if (!winfo)
     {
	  if (do_trace)
	       trace("weather_main(%d): Called with winfo=%p",
		     __LINE__, winfo);
	  debug("weather_main(%d): Invalid call arguments supplied; "
		"winfo=NULL", __LINE__);
	  return(ERR_BADARGS);	  
     }

     if (do_trace)
	  trace("weather_main(%d): Called with winfo=%p, winfo->altitude=%d, "
		"winfo->host=\"%s\" (%p), winfo->port=%u, winfo->timeout=%u, "
		"winfo->period=%d, winfo->max_fails=%u, "
		"winfo->fname_prefix=\"%s\" (%p), winfo->cmd=\"%s\" (%p), "
		"winfo->title=\"%s\" (%p), linfo=%p, ilist=%p",
		__LINE__, winfo, winfo->wsinfo.altitude,
		winfo->host ? winfo->host : "(null)", winfo->host,
		winfo->port, winfo->timeout, winfo->period, winfo->max_fails,
		winfo->fname_prefix ? winfo->fname_prefix : "(null)",
		winfo->fname_prefix,
		winfo->cmd ? winfo->cmd : "(null)", winfo->cmd,
		winfo->title ? winfo->title : "(null)", winfo->title,
		winfo->linfo, winfo->ilist);

     /*
      *  Initializations
      */
     devices            = NULL;
     free_prefix        = NULL;
     ha7net_initialized = 0;
 
     /*
      *  Initialize the ha7net library
      */
     attempts = 0;

try_connect:
     istat = ha7net_open(&ha7net, winfo->host, winfo->port ? winfo->port : 80,
			 winfo->timeout, 0);
     if (istat != ERR_OK)
     {
	  debug("weather_main(%d): Unable to initialize an ha7net context; "
		"ha7net_open() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  if (++attempts > 10)
	       goto done;
	  /*
	   *  Sleep for 30 seconds and try again
	   */
	  os_sleep(30*1000);
	  goto try_connect;
     }

     /*
      *  Search the 1-Wire bus for available devices
      */
     istat = ha7net_search(&ha7net, &devices, &ndevices, 0, 0,
			   HA7NET_FLAGS_RELEASE);
     if (istat != ERR_OK)
     {
	  debug("weather_main(%d): Unable to search the 1-Wire bus for "
		"devices; ha7net_search() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  goto done;
     }
     ha7net_initialized = 1;

     /*
      *  Initialize the devices
      */
     
     /*
      *  First, note which devices to ignore
      */
     dev_info_hints(devices, ndevices, winfo->linfo);
     dev_info_merge(devices, ndevices, 0, NULL, NULL, winfo->ilist);
     istat = dev_list_init(&ha7net, devices);
     if (istat != ERR_OK)
     {
	  debug("weather_main(%d): Unable to initialize some or all of the "
		"devices; dev_list_init() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Count up the number of logical devices
      */
     nlogical = 0;
     dev = devices;
     while (!dev_flag_test(dev, DEV_FLAGS_END))
     {
	  if (!dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) &&
	      dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       nlogical++;
	  dev++;
     }

     /*
      *  So that folks know that we're alive
      */
     info("ha7netd(%d): %u physical device%s located; %u logical device%s",
	  __LINE__, ndevices, (ndevices != 1) ? "s" : "",
	  nlogical, (nlogical != 1) ? "s" : "");

     /*
      *  Merge into the device list, device location & grouping information
      *  from the configuration file
      */
     dev_info_merge(devices, ndevices, 0, winfo->avg_periods, winfo->linfo,
		    NULL);

     /*
      *  See if there are any barometers which can be adjusted to sea level.
      *  (Must do this after dev_info_merge(,,,winfo->linfo,) as that merges
      *  in the information about what devices make outside temperature and
      *  humidity measurements.
      */
     winfo->have_pcor = 0;
     if (winfo->wsinfo.have_altitude)
     {
	  dev = devices;
	  while (!dev_flag_test(dev, DEV_FLAGS_END))
	  {
	       if (!dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) &&
		   dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       {
		    size_t i;

		    for (i = 0; i < NVALS; i++)
		    {
			 if (dev->data.fld_used[i] &&
			     dev->data.fld_dtype[i] == DEV_DTYPE_PRES)
			 {
			      if (ERR_OK ==
				  dev_pcor_add(dev, devices,
					       winfo->wsinfo.altitude))
				   winfo->have_pcor = 1;
			      break;
			 }
		    }
	       }
	       dev++;
	  }
     }

     /*
      *  Add data/ to the winfo->fname_prefix field
      */
     {	  
	  size_t len1, len2;
	  char *prefix, *ptr;

	  len1 = winfo->fname_path ? strlen(winfo->fname_path) : 0;
	  len2 = winfo->fname_prefix ? strlen(winfo->fname_prefix) : 0;
	  prefix = (char *)malloc(len1 + 1 + len2 + 1);
	  if (!prefix)
	  {
	       debug("weather_main(%d): Insufficient virtual memory",
		     __LINE__);
	       istat = ENOMEM;
	       goto done;
	  }
	  ptr = prefix;
	  if (len1)
	  {
	       memmove(ptr, winfo->fname_path, len1);
	       ptr += len1;
	       *ptr++ = '/';
	  }
	  if (len2)
	  {
	       memmove(ptr, winfo->fname_prefix, len2);
	       ptr += len2;
	  }
	  *ptr = '\0';
	  free_prefix = winfo->fname_prefix;
	  winfo->fname_prefix = prefix;
     }

     /*
      *  Load data from yesterday so that we can determine yesterday's extrema
      */
     istat = weather_data_read(devices, 1, winfo->fname_prefix);
     if (istat != ERR_OK)
     {
	  if (istat != ERR_EOM)
	       debug("weather_main(%d): Unable to read yesterday's weather "
		     "data; weather_data_read() returned %d; %s",
		     __LINE__, istat, err_strerror(istat));
     }
     else
	  /*
	   *  Move the extrema to the slots for yesterday
	   */
	  dev_hi_lo_reset(devices);

     /*
      *  Load today's data from a prior run
      */
     istat = weather_data_read(devices, 0, winfo->fname_prefix);
     if (istat != ERR_OK && istat != ERR_EOM)
	  debug("weather_main(%d): Unable to read today's weather data; "
		"weather_data_read() returned %d; %s",
		__LINE__, istat, err_strerror(istat));

     /*
      *  Let the nightly thread know about this block of devices
      */
     istat = daily_add_devices(devices);
     if (istat != ERR_OK)
	  debug("weather_main(%d): Unable to add the device block %p to the "
		"list of devices to do nightly statistics management of; "
		"daily_add_devices() returned %d; %s",
		__LINE__, istat, err_strerror(istat));

     /*
      *  Minimum period is 1 minute
      */
     period = winfo->period;
     if (period < 60)
	  period = 60;

     /*
      *  Now enter our endless loop of sampling & recording
      */

     winfo->first = 1;
     fails = 0;

loop:
     t0 = time(NULL);
     istat = weather_list_record(devices, &ha7net, period, winfo);
     if (istat != ERR_OK)
     {
	  if (!(fails % 5))
	       debug("weather_main(%d): Error capturing and recording data; "
		     "%d consecutive failure%s so far; weather_list_record() "
		     "returned %d; %s",
		     __LINE__, (fails + 1), (fails != 0) ? "s" : "", istat,
		     err_strerror(istat));
	  if (++fails > winfo->max_fails)
	  {
	       debug("weather_main(%d): Too many consecutive failures; "
		     "aborting", __LINE__);
	       istat = ERR_NO;
	       goto done;
	  }
     }
     else
	  fails = 0;

     /*
      *  Close the connection for now
      */
     ha7net_close(&ha7net, HA7NET_FLAGS_POWERDOWN);

     /*
      *  See how long to sleep
      */
     dt = (int)difftime(time(NULL), t0);

     /*
      *  Sleep unless a shutdown has been triggered
      */
     if (shutdown_flag ||
	 (dt < period  &&
	  0 < os_shutdown_sleep((os_shutdown_t *)winfo->sinfo,
				1000 * (period - dt))))
	  /*
	   *  os_shutdown_sleep() returns non-zero when we awaken
	   *  from a shutdown request.
	   */
	  goto done;

     /*
      *  Do another probe cycle provided that we weren't awakened to shutdown
      */
     goto loop;

done:
     if (free_prefix && winfo->fname_prefix)
     {
	  free((char *)winfo->fname_prefix);
	  winfo->fname_prefix = free_prefix;
     }
     if (devices)
     {
	  dev_list_done(&ha7net, devices);
	  ha7net_search_free(devices);
     }

     if (ha7net_initialized)
	  ha7net_done(&ha7net, HA7NET_FLAGS_POWERDOWN);

     /*
      *  Done for now
      */
     return(istat);
}


void
weather_thread(void *ctx)
{
     weather_info_t *winfo = (weather_info_t *)ctx;

     if (!winfo)
	  return;

     /*
      *  Stand up and be counted
      */
     os_shutdown_thread_incr(shutdown_info);

     /*
      *  Call weather_main() where we will loop until told to shutdown
      */
     winfo->sinfo = (void *)shutdown_info;
     if (ERR_OK != weather_main(winfo))
	  /*
	   *  Not very graceful
	   */
	  exit(1);

     /*
      *  Time to retire ourselves
      */
     os_shutdown_thread_decr(shutdown_info);

     /*
      *  And release the VM associated with this structure.  It was
      *  allocated by the primal thread.
      */
     free(winfo);
}

static int initialized = 0;

int
weather_lib_init(void)
{
     int istat;

     if (do_trace)
	  trace("weather_lib_init(%d): Called", __LINE__);

     if (initialized)
	  return(ERR_OK);

     /*
      *  Initialize the device drivers
      */
     istat = dev_lib_init();
     if (istat != ERR_OK)
     {
	  debug("weather_lib_init(%d): Unable to initialize the device driver "
		"libary; dev_lib_init() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     istat = xml_lib_init();
     if (istat != ERR_OK)
     {
	  debug("weather_lib_init(%d): Unable to initialize the XML library; "
		"xml_lib_init() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  dev_lib_done();
	  return(istat);
     }

     istat = ha7net_lib_init();
     if (istat != ERR_OK)
     {
	  debug("weather_lib_init(%d): Unable to initialize the ha7net "
		"library; ha7net_lib_init() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  xml_lib_done();
	  dev_lib_done();
	  return(istat);
     }

     shutdown_flag = 0;
     shutdown_info = NULL;
     istat = os_shutdown_create(&shutdown_info);
     if (istat || !shutdown_info)
     {
	  int save_errno = errno;
	  ha7net_lib_done();
	  xml_lib_done();
	  dev_lib_done();
	  if (istat)
	  {
	       if (save_errno == ENOMEM)
		    return(ERR_NOMEM);
	       else if (save_errno == EINVAL)
		    return(ERR_BADARGS);
	       else
		    return(ERR_NO);
	  }
	  else
	       return(ERR_NO);
     }

     istat = daily_lib_init();
     if (istat == ERR_OK)
	  istat = daily_start();
     if (istat != ERR_OK)
     {
	  debug("weather_lib_init(%d): Unable to initialize the midnight "
		"thread; daily_thread_start() returned %d; %s",
		__LINE__, istat, err_strerror(istat));
	  ha7net_lib_done();
	  xml_lib_done();
	  dev_lib_done();
	  os_shutdown_finish(shutdown_info, 0);
	  shutdown_info = NULL;
	  return(istat);
     }

     initialized = (istat == ERR_OK) ? 1 : 0;

     return(istat);
}


int
weather_lib_done(unsigned int seconds)
{
     int istat;

     if (do_trace)
	  trace("weather_lib_done(%d): Called", __LINE__);

     if (!initialized)
	  return(ERR_OK);

     /*
      *  Awaken any weather_main() threads which are waiting for the
      *  next probe.
      */
     shutdown_flag = -1;
     if (shutdown_info)
	  os_shutdown_begin(shutdown_info);

     /*
      *  Start the shutdown of the daily thread
      */
     daily_shutdown_begin();

     /*
      *  Shutdown other libraries
      */
     ha7net_lib_done();
     xml_lib_done();
     dev_lib_done();

     /*
      *  Wait for the daily thread to finish
      */
     istat = daily_shutdown_finish(seconds);
     if (istat)
	  debug("weather_lib_done(%d): Unable to stop all nightly processing "
		"threads", __LINE__);
     daily_lib_done();

     /*
      *  Wait for our own stuff to shutdown
      */
     istat = os_shutdown_finish(shutdown_info, 30);
     if (istat)
	  debug("weather_lib_done(%d): Unable to stop all weather logging "
		"threads", __LINE__);
     shutdown_info = NULL;

     initialized = 0;

     return(ERR_OK);
}
