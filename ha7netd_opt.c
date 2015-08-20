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
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "opt.h"
#include "os.h"
#include "ha7net.h"
#include "ha7netd.h"

#define PARSE_ALT 0
#define PARSE_FLA 1
#define PARSE_PER 2

static opt_parse_proc_t parse_value;

static ha7netd_opt_t odummy;
static opt_bulkload_t ha7netd_opts[] = {
     { OBULK_NUMP("altitude",     odummy.altitude,  0,
		  OPT_DTYPE_INT,  parse_value,     (void *)PARSE_ALT) },
     { OBULK_STR("averages",      odummy.avgs,      0) },
     { OBULK_STR("cmd",           odummy.cmd,       0) },
     { OBULK_STR("data",          odummy.dpath,     0) },
     { OBULK_STR("host",          odummy.host,      0) },
     { OBULK_STR("latitude",      odummy.lat,       0) },
     { OBULK_STR("location",      odummy.loc,       0) },
     { OBULK_STR("longitude",     odummy.lon,       0) },
     { OBULK_UINT("max_failures", odummy.max_fails, 0) },
     { OBULK_NUMP("period",       odummy.period,    0,
		  OPT_DTYPE_INT,  parse_value,    (void *)PARSE_PER) },
     { OBULK_USHORT("port",       odummy.port,      0) },
     { OBULK_UINT("timeout",      odummy.tmo,       0) },
     { OBULK_TERM }
};

static ha7netd_dopt_t ddummy;
static opt_bulkload_t ha7netd_dopts[] = {
     { OBULK_STR("averages",        ddummy.avgs,     0) },
     { OBULK_STR("device_specific", ddummy.spec,     0) },
     { OBULK_NUMP("flags",          ddummy.flags,    0,
		  OPT_DTYPE_UINT,   parse_value,   (void *)PARSE_FLA) },
     { OBULK_FLOAT("gain",          ddummy.gain)        },
     { OBULK_STR("hint",            ddummy.hint,     0) },
     { OBULK_STR("location",        ddummy.loc,      0) },
     { OBULK_FLOAT("offset",        ddummy.offset)      },
     { OBULK_TERM }
};

static opt_parse_proc_t ha7netd_opt_facstr;
static ha7netd_gopt_t gdummy;
static opt_bulkload_t ha7netd_gopts[] = {
     { OBULK_INT("debug",            gdummy.debug,    0) },
     { OBULK_STR("log_facility",     gdummy.facility, 0) },
     { OBULK_STR("user",             gdummy.user,     0) },
     { OBULK_TERM }
};

extern const char default_facility[];

static const char     *default_avgs     = "10m 1h";
static const char     *default_cmd      = "xml_to_html.sh %x";
static int             default_debug    = 1;
static const char     *default_dpath    = "data/";
static int             default_fails    = 10;
static const char     *default_host     = "192.168.0.250"; /* HA7Net default */
static const char     *default_loc      = "A cornfield in Iowa";
static int             default_period   = 60 * 2;    /* 2 minutes  */
static unsigned short  default_port     = 80;
static unsigned int    default_tmo      = 60 * 1000; /* 60 seconds */
static const char     *default_user     = "";
static device_period_array_t default_periods = {10*60, 60*60, 0, 0};

extern void dbglog(const char *fmt, ...);

static void ha7netd_list_dispose(ha7netd_opt_t *list);
static void ha7netd_dev_list_dispose(device_loc_t *list);
static int ha7netd_list_build(void *our_ctx, void *optlib_ctx,
  const char *gname, size_t gnlen, const char *gval, size_t gvlen);
static int ha7netd_devlist_build(void *our_ctx, void *optlib_ctx,
  const char *gname, size_t gnlen, const char *gval, size_t gvlen);
static int ha7netd_ignlist_build(void *our_ctx, void *optlib_ctx,
  const char *gname, size_t gnlen, const char *gval, size_t gvlen);

static void insert_val(int val, int vals[], size_t *nlen, size_t nmax);
static int parse_periods(device_period_array_t periods, size_t nmax,
  const char *str);

#define STATE_F 1
#define STATE_K 2
#define STATE_M 3

static int
parse_value(void *ctx, void *outbuf, size_t outbufsize, const char *inbuf,
	    size_t inlen, const opt_option_t *opt, const opt_bulkload_t *item)
{
     int blank_seen, digit_seen, mode, sign, state, units_seen, val;
     char c;
     size_t i, j;
     unsigned int uval;

     /*
      *  Watch out for bogus input
      */
     if (!outbuf || outbufsize != sizeof(int) || !inbuf)
	  return(ERR_BADARGS);

     /*
      *  Parsing mode
      */
     mode = (int)ctx;
     if (mode != PARSE_ALT && mode != PARSE_PER && mode != PARSE_FLA)
	  return(ERR_NO);

     /*
      *  Ignore leading LWSP
      */
     j = inlen;
     for (i = 0; i < j; i++)
     {
	  if (!isspace(*inbuf))
	       break;
	  --inlen;
	  ++inbuf;
     }

     /*
      *  Deal with an empty line
      */
     if (!inlen)
     {
	  /*
	   *  Use the default
	   */
	  switch(mode)
	  {
	  case PARSE_ALT :
	       *(int *)outbuf = 0;
	       break;

	  case PARSE_PER :
	       *(int *)outbuf = default_period;
	       break;

	  case PARSE_FLA :
	       *(unsigned int *)outbuf = 0;
	       break;

	  default :
	       return(ERR_NO);
	  }
	  return(ERR_OK);
     }

     if (mode == PARSE_FLA)
	  goto parse_flags;

     val        = 0;
     sign       = 0;
     blank_seen = 0;
     digit_seen = 0;
     units_seen = 0;
     state      = 0;

     /*
      *  Parse the line
      */
     for (i = 0; i < inlen; i++)
     {
	  switch((c = *inbuf++))
	  {
	  case '-' :
	       if (digit_seen || sign != 0)
		    return(ERR_SYNTAX);
	       sign = -1;
	       break;

	  case '+' :
	       if (digit_seen || sign != 0)
		    return(ERR_SYNTAX);
	       sign = 1;
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
	       if (units_seen || blank_seen)
		    return(ERR_SYNTAX);
	       val = (val * 10) + c - '0';
	       digit_seen = 1;
	       break;

	  case ' ' :
	  case '\t' :
	       if (state != 0)
		    /*
		     *  No white space in the middle of a unit name
		     */
		    return(ERR_SYNTAX);
	       blank_seen = 1;
	       break;

	  case 'd' :
	  case 'D' :
	       if (!digit_seen || units_seen || mode != PARSE_PER)
		    return(ERR_SYNTAX);
	       val = val * 60 * 60 * 24;
	       units_seen = 1;
	       break;

	  case 'f' :
	  case 'F' :
	       if (!digit_seen || units_seen || mode != PARSE_ALT)
		    return(ERR_SYNTAX);
	       state = STATE_F;
	       break;

	  case 'h' :
	  case 'H' :
	       if (!digit_seen || units_seen || mode != PARSE_PER)
		    return(ERR_SYNTAX);
	       val = val * 60 * 60;
	       units_seen = 1;
	       break;

	  case 'i' :
	  case 'I' :
	       if (state != STATE_M)
		    return(ERR_SYNTAX);
	       /*
		*  Convert from miles to meters
		*/
	       val = (int)(0.5 + (float)val * 1609.344);
	       state      = 0;
	       units_seen = 1;
	       break;

	  case 'k' :
	  case 'K' :
	       if (!digit_seen || units_seen || mode != PARSE_ALT)
		    return(ERR_SYNTAX);
	       state = STATE_K;
	       break;

	  case 'm' :
	  case 'M' :
	       if (!digit_seen || units_seen)
		    return(ERR_SYNTAX);
	       if (mode == PARSE_PER)
	       {
		    /*
		     *  Convert from minutes to seconds
		     */
		    val = val * 60;
		    units_seen = 1;
	       }
	       else if (mode == PARSE_ALT)
	       {
		    if (state == 0)
		    {
			 state      = STATE_M;
			 units_seen = 1;  /* in which case it is meters */
		    }
		    else if (state == STATE_K)
		    {
			 /*
			  *  Convert from kilometers to meters
			  */
			 val = val * 1000;
			 state      = 0;
			 units_seen = 1;
		    }
		    else
			 return(ERR_SYNTAX);
	       }
	       break;

	  case 's' :
	  case 'S' :
	       if (!digit_seen || units_seen || mode != PARSE_PER)
		    return(ERR_SYNTAX);
	       units_seen = 1;
	       break;

	  case 't' :
	  case 'T' :
	       if (state != STATE_F)
		    return(ERR_SYNTAX);
	       /*
		*  Convert from feet to meters 1ft = 0.3048 m (exactly)
		*/
	       val = (int)(0.5 + (float)val * 0.3048);
	       state      = 0;
	       units_seen = 1;
	       break;

	  default :
	       /*
		*  Invalid character encountered
		*/
	       return(ERR_SYNTAX);
	  }
     }

     /*
      *  Deal with cases where the parse ended prematurely
      */
     if (state != 0 && state != STATE_M)
	  /*
	   *  Saw only a 'f' or 'k' with no following 't' or 'm'
	   *  A 'm' with no following 'i' is, however, okay.
	   */
	  return(ERR_SYNTAX);

     if (!digit_seen)
     {
	  /*
	   *  Enforce a default value if the line was syntactically blank
	   */
	  switch (mode)
	  {
	  case PARSE_ALT :
	       val = 0;
	       break;

	  case PARSE_PER :
	       val = default_period;
	       break;

	  default :
	       val = 0;
	       break;
	  }
     }
     else if (!units_seen)
     {
	  /*
	   *  If no units were specified then assume default units
	   */
	  switch (mode)
	  {
	  case PARSE_ALT :
	       break;

	  case PARSE_PER :
	       /*
		*  Default units are minutes so adjust to seconds
		*/
	       val *= 60;
	       break;

	  default :
	       break;
	  }
     }

     /*
      *  Handle any sign specification (useful for altitudes below sea level)
      */
     if (sign < 0)
	  val = -val;

     /*
      *  Return the result
      */
     *(int *)outbuf = val;

     return(ERR_OK);

     /*
      *  Parse a space or comma delimited list of device flags
      */
parse_flags:
     uval = 0;
     blank_seen = 1;
     for (i = 0; i < inlen; i++)
     {
	  switch((c = *inbuf++))
	  {
	  case ' '  :
	  case '\v' :
	  case '\t' :
	  case ','  :
	       blank_seen = 1;
	       break;
	       
	  case 'o' :
	  case 'O' :
	       if (blank_seen)
		    uval |= DEV_FLAGS_OUTSIDE;
	       blank_seen = 0;
	       break;

	  case 'i' :
	  case 'I' :
	       if (blank_seen)
		    uval &= ~DEV_FLAGS_OUTSIDE;
	       blank_seen = 0;
	       break;

	  default :
	       blank_seen = 0;
	       break;
	  }
     }

     /*
      *  Return the result
      */
     *(unsigned int *)outbuf = uval;

     /*
      *  Return a success
      */
     return(ERR_OK);
}

#undef STATE_F
#undef STATE_K
#undef STATE_M
#undef PARSE_ALT
#undef PARSE_FLA
#undef PARSE_PER


/*
 *  Insert val into vals[] using a descending order sort (vals[i] >= vals[i+1])
 *  [Yes, this is an insertion sort]
 */
static void
insert_val(int val, int vals[], size_t *nlen, size_t nmax)
{
     size_t i, j;

     if (!nlen)
	  /*
	   *  Error
	   */
	  return;

     if (*nlen >= nmax)
	  /*
	   *  No room
	   */
	  return;

     /*
      *  Determine where to insert this value
      */
     for (i = 0; i < *nlen; i++)
	  if (val > vals[i])
	       break;

     /*
      *  Insert at the tail if i == *nlen
      */
     if (i >= *nlen)
     {
	  vals[i] = val;
	  *nlen = *nlen + 1;
	  return;
     }

     /*
      *  Insert somewhere in between
      */
     for (j = *nlen; j > i; j--)
	  vals[j] = vals[j-1];
     vals[i] = val;
     *nlen = *nlen + 1;
}


static int
parse_periods(device_period_array_t periods, size_t nmax, const char *str)
{
     size_t n;
     int p[NPERS + 1], digit_seen, val;
     static const char *empty_str = "";

     /*
      *  Initializations
      */
     n   = 0;
     val = 0;
     digit_seen = 0;
     memset(p, 0, (NPERS + 1) * sizeof(int));
     memset(periods, 0, sizeof(device_period_array_t));
     if (!str)
	  str = empty_str;

     /*
      *  Our temp only allows for NPERS + 1 entries
      */
     if (nmax > NPERS)
	  nmax = NPERS;

     while (*str)
     {
	  switch(*str)
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
	       digit_seen = 1;
	       val = (val * 10) + (*str) - '0';
	       break;

	  case ',' :
	  case ' ' :
	  case '\t' :
	       /*
		*  Minutes are used as the default units
		*/
	       if (!val)
		    break;
	       insert_val(val * 60, p, &n, nmax + 1);
	       val = 0;
	       digit_seen = 0;
	       break;

	  case 'd' :
	  case 'D' :
	       if (!digit_seen)
		    /*
		     *  Naked 'd' seen
		     */
		    return(ERR_SYNTAX);
	       digit_seen = 0;
	       if (!val)
		    break;
	       insert_val(val * 60 * 60 * 24, p, &n, nmax + 1);
	       val = 0;
	       break;

	  case 'h' :
	  case 'H' :
	       if (!digit_seen)
		    /*
		     *  Naked 'h' seen
		     */
		    return(ERR_SYNTAX);
	       digit_seen = 0;
	       if (!val)
		    break;
	       insert_val(val * 60 * 60, p, &n, nmax + 1);
	       val = 0;
	       break;

	  case 'm' :
	  case 'M' :
	       if (!digit_seen)
		    /*
		     *  Naked 'm' seen
		     */
		    return(ERR_SYNTAX);
	       digit_seen = 0;
	       if (!val)
		    break;
	       insert_val(val * 60, p, &n, nmax + 1);
	       val = 0;
	       break;

	  case 's' :
	  case 'S' :
	       if (!digit_seen)
		    /*
		     *  Naked 's' seen
		     */
		    return(ERR_SYNTAX);
	       digit_seen = 0;
	       if (!val)
		    break;
	       insert_val(val, p, &n, nmax + 1);
	       val = 0;
	       break;

	  default :
	       /*
		*  Invalid character encountered
		*/
	       return(ERR_SYNTAX);
	  }

	  if (n >= nmax)
	       /*
		*  Too many averaging periods specified
		*/
	       return(ERR_TOOLONG);
	  str++;
     }

     /*
      *  Did the string end with a value lacking any units?
      */
     if (val && digit_seen)
	  insert_val(val * 60, p, &n, nmax + 1);
     if (n >= nmax)
	  return(ERR_TOOLONG);

     /*
      *  Copy the values over
      */
     for (n = 0; n < nmax; n++)
	  periods[n] = p[n];

     return(ERR_OK);
}

#if 0
static int
ha7netd_opt_facstr(void *ctx, void *outbuf, size_t outbufsize,
		   const char *inbuf, size_t inlen, const opt_option_t *opt,
		   const opt_bulkload_t *item)
{
     int fac;

     (void)ctx;
     (void)inlen;
     (void)opt;
     (void)item;

     if (!outbuf || !inbuf || outbufsize < sizeof(int))
	  return(ERR_BADARGS);
     fac = os_facstr(inbuf);
     if (fac)
     {
	  *((int *)outbuf) = fac;
	  return(ERR_OK);
     }
     else
	  return(ERR_NO);
}

#endif

static void
copy(char *dst, const char *src, size_t maxlen)
{
     size_t len;

     if (!dst || !maxlen)
	  return;

     len = src ? strlen(src) : 0;
     if (len >= maxlen)
	  len = maxlen - 1;
     if (len)
	  memmove(dst, src, len);
     dst[len] = '\0';
}


void
ha7netd_opt_defaults(ha7netd_opt_t *opts, ha7netd_gopt_t *gblopts)
{
     if (gblopts)
     {
	  memset(gblopts, 0, sizeof(ha7netd_gopt_t));
	  gblopts->debug = default_debug;
	  copy(gblopts->facility, default_facility, sizeof(gblopts->facility));
	  copy(gblopts->user, default_user, sizeof(gblopts->user));
     }

     if (opts)
     {
	  memset(opts, 0, sizeof(ha7netd_opt_t));

	  opts->altitude  = HA7NETD_NO_ALTITUDE;
	  opts->max_fails = default_fails;
	  opts->period    = default_period;
	  opts->port      = default_port;
	  opts->tmo       = default_tmo;

	  memmove(opts->periods, default_periods,
		  sizeof(device_period_array_t));

	  copy(opts->avgs,  default_avgs,  sizeof(opts->avgs));
	  copy(opts->cmd,   default_cmd,   sizeof(opts->cmd));
	  copy(opts->dpath, default_dpath, sizeof(opts->dpath));
	  copy(opts->host,  default_host,  sizeof(opts->host));
	  copy(opts->loc,   default_loc,   sizeof(opts->loc));
     }
}


static void
ha7netd_list_dispose(ha7netd_opt_t *list)
{
     ha7netd_opt_t *tmpl;

     while (list)
     {
	  tmpl = list->next;
	  free(list);
	  list = tmpl;
     }
}


static int
ha7netd_list_build(void *our_ctx, void *optlib_ctx, const char *gname,
		  size_t gnlen, const char *gval, size_t gvlen)
{
     ha7netd_opt_t **ha7netd_list, *tmp;
     int istat;

     /*
      *  Sanity checks
      */
     if (!our_ctx || !gval)
     {
	  dbglog("ha7netd_list_build(%d): Bad call arguments supplied; "
		 "our_ctx=%p, gval=%p", __LINE__, our_ctx, gval);
	  return(ERR_BADARGS);
     }

     /*
      *  Allocate storage for the options for a new ha7net device
      */
     ha7netd_list = (ha7netd_opt_t **)our_ctx;
     tmp = (ha7netd_opt_t *)malloc(sizeof(ha7netd_opt_t));
     if (!tmp)
     {
	  dbglog("ha7netd_list_build(%d): Insufficient virtual memory",
		 __LINE__);
	  return(ERR_NOMEM);
     }

     /*
      *  Add the device to the head of the device list
      */
     tmp->next = *ha7netd_list;
     *ha7netd_list = tmp;

     /*
      *  Set the compile-time defaults for the ha7netd device
      */
     ha7netd_opt_defaults(tmp, NULL);

     /*
      *  Set the device's host name to be the value in [ha7net="value"]
      *  There's two ways to do this:
      *
      *    1. Take the value here and now, or
      *    2. Push the value into the option list such that it won't override
      *       any value already specified for this group.
      *
      *  The advantage of choice 2. is that it allows a normal course
      *  of error handling for when the string is too long (i.e., we get
      *  the same error handling as if the file had specified "host=...").
      */
     if (gvlen)
     {
	  size_t len = gvlen;

	  /*
	   *  Store the group value in the tmp->gname field
	   *  This value will be used to prefix the name of the
	   *  cumulative data file for this HA7Net device.
	   */
	  if (len >= MAX_OPT_LEN)
	       len = MAX_OPT_LEN - 1;
	  memmove(tmp->gname, gval, len);
	  tmp->gname[len] = '\0';

	  /*
	   *  Now push the value into the option list as host=<gval> *if*
	   *  no host= option has been specified for this option group.
	   */
	  istat = opt_option_push(optlib_ctx, gname, gval, "host", gval,
				  opt_source(optlib_ctx),
				  opt_lineno(optlib_ctx),
				  OPT_FLAGS_UNDERRIDE);
	  if (istat != ERR_OK)
	  {
	       /*
		*  Okay, do it using method 1...
		*/
	  }
     }

     /*
      *  Now bulk load the options
      */
     istat = opt_bulkload(optlib_ctx, ha7netd_opts, tmp, 0);
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd_list_build(%d): Unable to process the configuration "
		 "file; opt_bulkload() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      * If we had no group value, then use the host name
      */
     if (!gvlen)
     {
	  size_t len = strlen(tmp->host);

	  if (len >= MAX_OPT_LEN)
	       len = MAX_OPT_LEN - 1;
	  memmove(tmp->gname, tmp->host, len);
	  tmp->gname[len] = '\0';
     }

     /*
      *  Handle the averaging periods
      */
     istat = parse_periods(tmp->periods, NPERS, tmp->avgs);
     if (istat == ERR_OK)
	  return(ERR_OK);

     if (istat == ERR_SYNTAX)
	  dbglog("ha7netd_list_build(%d): Invalid averaging period "
		 "specification, \"%s\"",
		 __LINE__, tmp->avgs ? tmp->avgs : "(null)");
     else if (istat == ERR_TOOLONG)
	  dbglog("ha7netd_list_build(%d): No more than %d averaging periods "
		 "may be specified; too many specified with \"%s\"",
		 __LINE__, NPERS, tmp->avgs ? tmp->avgs : "(null)");
     else
	  dbglog("ha7netd_list_build(%d): Unable to parse the list of "
		 "averaging periods, \"%s\"",
		 __LINE__, tmp->avgs ? tmp->avgs : "(null)");
     return(istat);
}


static void
ha7netd_devlist_dispose(device_loc_t *list)
{
     device_loc_t *tmpl;

     while (list)
     {
	  tmpl = list->next;
	  free(list);
	  list = tmpl;
     }
}


static int
ha7netd_devlist_build(void *our_ctx, void *optlib_ctx, const char *gname,
		     size_t gnlen, const char *gval, size_t gvlen)
{
     device_loc_t **device_list, *tmp;
     ha7netd_dopt_t dopt;
     int gref, istat, sleeze;
     size_t hlen, llen, rlen, slen, ssize;
     char *romid, *valu;
     /*
      *  The shrubbery must have exactly 16 hexadecimal digits, no more
      *  and no less!
      */
     static const char *xdigit16 = "^[[:xdigit:]]{16}$";
     static const char *ign_devices = "^[\\!~].*";

     /*
      *  Sanity checks
      */
     if (!our_ctx)
     {
	  dbglog("ha7netd_devlist_build(%d): Bad call arguments supplied; "
		 "our_ctx=NULL", __LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Pointer to the list pointer
      */
     device_list = (device_loc_t **)our_ctx;

     /*
      *  For each distinct device group which has a name (e.g., [device=name]),
      *  we want to generate a distinct reference number.  We do this by
      *  carrying a counter in the device_loc_t structure.  We propogate
      *  the value of this counter forward in the list each time we add
      *  a new node to the linked list.
      */
     if (*device_list)
	  sleeze = (*device_list)->sleeze;
     else
	  sleeze = 0;

     /*
      *  Do we have a group name?
      */
     if (!gval || !gval[0])
     {
	  gvlen = 0;
	  gref  = 0;
     }
     else
	  /*
	   *  We have a named group
	   *  Increment the distinct reference number
	   */
	  gref = ++sleeze;

     /*
      *  Bulk load the options with static names
      */
     memset(&dopt, 0, sizeof(ha7netd_dopt_t));
     dopt.gain   = 1.0;
     dopt.offset = 0.0;
     istat = opt_bulkload(optlib_ctx, ha7netd_dopts, &dopt, 0);
     if (dopt.avgs[0])
     {
	  /*
	   *  Averaging periods specified...
	   */
	  istat = parse_periods(dopt.periods, NPERS, dopt.avgs);
	  if (istat != ERR_OK)
	  {
	       if (istat == ERR_SYNTAX)
		    dbglog("ha7netd_list_build(%d): Invalid averaging period "
			   "specification, \"%s\"", __LINE__, dopt.avgs);
	       else if (istat == ERR_TOOLONG)
		    dbglog("ha7netd_list_build(%d): No more than %d averaging "
			   "periods may be specified; too many specified "
			   "with \"%s\"", __LINE__, NPERS, dopt.avgs);
	       else
		    dbglog("ha7netd_list_build(%d): Unable to parse the list "
			   "of averaging periods, \"%s\"",
			   __LINE__, dopt.avgs);
	       return(istat);
	  }
     }

     /*
      *  Hint length
      */
     hlen = strlen(dopt.hint);;

     /*
      *  Location length
      */
     llen = strlen(dopt.loc);

     /*
      *  Device specific data length
      */
     slen = strlen(dopt.spec);

     /*
      *  Now loop through all options whose name is 16 bytes long
      *  and containing only hexadecimal digits...
      */
loop1:
     istat = opt_get_next(optlib_ctx, (const char **)&romid, &rlen, NULL, NULL,
			  xdigit16, OPT_MATCH_REGEX | OPT_MATCH_NOGLOBAL);
     if (istat != ERR_OK)
	  goto done1;
     else if (!romid || rlen != OWIRE_ID_LEN)
	  goto loop1;

     /*
      *  When we have device specific data it is of the form
      *
      *      option-1=value-1[;option-2=value-2[...]]
      *
      *  To make it a bit easier to parse, we pre- and post-pend semicolons
      *  to it and thus add 2 to its length, not including a NUL terminator.
      */
     ssize  = slen ? 1 + slen + 1 + 1 : 0;
     ssize += sizeof(device_loc_t) + llen;
     tmp = (device_loc_t *)calloc(1, ssize);
     if (!tmp)
     {
	  dbglog("ha7netd_devlist_build(%d): Insufficient virtual memory",
		 __LINE__);
	  istat = ERR_NOMEM;
	  goto done1;
     }

     tmp->next    = *device_list;
     tmp->sleeze  = sleeze;
     *device_list = tmp;
     tmp->ssize   = ssize;
     tmp->flags   = dopt.flags;
     tmp->gain    = dopt.gain;
     tmp->offset  = dopt.offset;
     memmove(tmp->periods, dopt.periods, sizeof(device_period_array_t));

     /*
      *  Since we used calloc() we can omit all of the following when vlen == 0
      */
     if (llen)
     {
	  tmp->dlen = llen;
	  memmove(tmp->desc, dopt.loc, llen);
	  tmp->desc[llen] = '\0';
     }

     if (hlen)
     {
	  tmp->hlen = (hlen < MAXHINT) ? hlen : MAXHINT;
	  memmove(tmp->hint, dopt.hint, tmp->hlen);
	  tmp->hint[tmp->hlen] = '\0';
     }

     /*
      *  Device specific data
      */
     if (slen)
     {
	  char *sptr;

	  tmp->slen = 1 + slen + 1;
	  sptr = tmp->spec = tmp->desc + llen + 1;
	  *sptr++ = ';';
	  memmove(sptr, dopt.spec, slen);
	  sptr += slen;
	  *sptr++ = ';';
	  *sptr = '\0';
     }

     /*
      *  Canonicalize the ROM id
      */
     dev_romid_cannonical(tmp->romid, sizeof(tmp->romid), romid, rlen);

     /*
      *  If this is a named group, then link the devices together under their
      *  group name
      */
     if (!gvlen)
	  goto loop1;

     /*
      *  Save the group name info
      */
     tmp->group1.ref  = gref;
     tmp->group1.nlen = gvlen;
     if (tmp->group1.nlen >= DEV_GNAME_LEN)
	  tmp->group1.nlen = DEV_GNAME_LEN - 1;
     memmove(tmp->group1.name, gval, tmp->group1.nlen);
     tmp->group1.name[tmp->group1.nlen] = '\0';

     goto loop1;

done1:
     if (istat != ERR_EOM)
	  goto done2;

     /*
      *  Now look for devices part of this group but which we should
      *  ignore: we push them into the [ignore] group as we find them
      */
     opt_get_start(optlib_ctx, OPT_MATCH_NOGLOBAL);
loop2:
     istat = opt_get_next(optlib_ctx, (const char **)&romid, &rlen, &valu, NULL,
			  ign_devices, OPT_MATCH_REGEX | OPT_MATCH_NOGLOBAL);
     if (istat != ERR_OK)
	  goto done2;
     else if (!romid || rlen <= 1)
	  goto loop2;

     istat = opt_option_push(optlib_ctx, "ignore", NULL, romid+1, valu,
			     opt_source(optlib_ctx), opt_lineno(optlib_ctx),
			     OPT_FLAGS_ADD);
     if (istat == ERR_OK)
	  goto loop2;

done2:
     if (istat == ERR_EOM)
	  return(ERR_OK);

     /*
      *  Error of some sort
      */
     dbglog("ha7netd_devlist_build(%d): Unable to process the configuration "
	    "file; opt_option_next() returned %d; %s",
	    __LINE__, istat, err_strerror(istat));
     return(istat);
}


static void
ha7netd_ignlist_dispose(device_ignore_t *list)
{
     device_ignore_t *tmpl;

     while (list)
     {
	  tmpl = list->next;
	  free(list);
	  list = tmpl;
     }
}


static int
ha7netd_ignlist_build(void *our_ctx, void *optlib_ctx, const char *gname,
		     size_t gnlen, const char *gval, size_t gvlen)
{
     device_ignore_t **ignore_list, *tmp;
     int istat;
     char *pat;
     size_t plen;

     /*
      *  Sanity checks
      */
     if (!our_ctx)
     {
	  dbglog("ha7netd_ignlist_build(%d): Bad call arguments supplied; "
		 "our_ctx=NULL", __LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Pointer to the list pointer
      */
     ignore_list = (device_ignore_t **)our_ctx;

     /*
      *  Now loop through all options
      */
loop:
     istat = opt_get_next(optlib_ctx, (const char **)&pat, &plen, NULL, NULL,
			  NULL, OPT_MATCH_NOGLOBAL | OPT_MATCH_BEGINS_WITH);
     if (istat != ERR_OK)
	  goto done;
     else if (!pat || !plen)
	  goto loop;
     else if (plen >= DEV_IGNORE_PAT_LEN_MAX)
     {
	  dbglog("ha7netd_ignlist_build(%d): Device name/pattern in the "
		 "'[ignore]' section of the configuration files is too long; "
		 "maximum length is %d bytes; device name/pattern is \"%.*s\"",
		 __LINE__, DEV_IGNORE_PAT_LEN_MAX - 1, plen,
		 pat ? pat : "(null)");
	  goto loop;
     }

     tmp = (device_ignore_t *)malloc(sizeof(device_ignore_t));
     if (!tmp)
     {
	  dbglog("ha7netd_ignlist_build(%d): Insufficient virtual memory",
		 __LINE__);
	  return(ERR_NOMEM);
     }
     tmp->next = *ignore_list;
     *ignore_list = tmp;
     tmp->plen = plen;
     dev_romid_cannonical(tmp->pat, sizeof(tmp->pat), pat, plen);

     goto loop;

done:
     if (istat == ERR_EOM)
	  return(ERR_OK);

     /*
      *  Error of some sort
      */
     dbglog("ha7netd_devlist_build(%d): Unable to process the configuration "
	    "file; opt_option_next() returned %d; %s",
	    __LINE__, istat, err_strerror(istat));
     return(istat);
}


void
ha7netd_config_unload(ha7netd_opt_t *ha7netd_list, device_loc_t *dev_list,
		      device_ignore_t *ign_list)
{
     ha7netd_list_dispose(ha7netd_list);
     ha7netd_devlist_dispose(dev_list);
     ha7netd_ignlist_dispose(ign_list);
}


int
ha7netd_config_load(ha7netd_opt_t **ha7netd_list, device_loc_t **dev_list,
		    device_ignore_t **ign_list, ha7netd_gopt_t *gbl_opts,
		    const char *fname)
{
     device_loc_t *device_list;
     device_ignore_t *ignore_list;
     int istat;
     ha7netd_opt_t *opt_list;
     opt_t opts;

     /*
      *  Ensure that the option file routines can report errors
      */
     opt_debug_set(ha7netd_dbglog, 0, 1);

     /*
      *  Initialize the structure
      */
     opt_init(&opts);

     /*
      *  The [ignore] group allows option names with no "="
      */
     opt_set(&opts, OPT_ITEM_EMPTY_ALLOWED, "ignore|device", 0);

     /*
      *  Now read the config
      */
     istat = opt_read(&opts, fname, NULL);
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd_config_load(%d): Unable to read and parse the "
		 "configuration file \"%s\"; opt_read() returned %d; %s",
		 __LINE__, fname ? fname : "(null)", istat,
		 err_strerror(istat));
	  return(istat);
     }

     /*
      *  Now, push down any command line options: they will override any
      *  global option settings: we allow the command line to set a
      *  host name & port and a debug level.  Everything else is done
      *  via an option file.  This allows minimal operation with no
      *  configuration file.
      */
     if (gbl_opts)
     {
	  int ival;
	  size_t slen;
	  const char *sval;

	  if (gbl_opts->debug_arg)
	       istat = opt_option_push(&opts, NULL, NULL, "debug",
				       gbl_opts->debug_arg, "the command line",
				       0, OPT_FLAGS_OVERRIDE);
	  if (gbl_opts->host_arg)
	       istat = opt_option_push(&opts, NULL, NULL, "host",
				       gbl_opts->host_arg, "the command line",
				       0, OPT_FLAGS_OVERRIDE);
	  if (gbl_opts->port_arg)
	       istat = opt_option_push(&opts, NULL, NULL, "port",
				       gbl_opts->port_arg, "the command line",
				       0, OPT_FLAGS_OVERRIDE);
	  /*
	   *  Get our global option settings
	   */
	  istat = opt_bulkload_init(ha7netd_gopts, &gdummy);
	  if (istat == ERR_OK)
	       istat = opt_bulkload(&opts, ha7netd_gopts, gbl_opts, 0);
	  if (istat != ERR_OK)
	  {
	       dbglog("ha7netd_config_load(%d): Error obtaining our global "
		      "options; opt_bulkload() returned %d; %s",
		      __LINE__, istat, err_strerror(istat));
	       goto done;
	  }
     }

     /*
      *  Initialize our bulk load data structures
      */
     istat = opt_bulkload_init(ha7netd_opts, &odummy);
     istat = opt_bulkload_init(ha7netd_dopts, &ddummy);

     /*
      *  Walk all the groups named "ha7net": this will repeatedly call
      *  build_ha7netd_list() and generate the linked list ha7netd_list.
      *  Each entry in ha7netd_list will represent on ha7net device and
      *  its settings.
      */
     opt_list = NULL;
     istat = opt_group_walk(&opts, "ha7net", OPT_MATCH_GLOBAL_FALLBACK,
			    ha7netd_list_build, (void *)&opt_list);
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd_config_load(%d): Error loading options for HA7Net "
		 "devices; opt_group_walk() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Now walk the lists of devices
      */
     device_list = NULL;
     istat = opt_group_walk(&opts, "device", 0, ha7netd_devlist_build,
			    (void *)&device_list);
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd_config_load(%d): Error loading options for HA7Net "
		 "devices ([devices]); opt_group_walk() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Finally, get the list of devices to ignore.  These names may
      *  be full device names, glob-style matching patterns, or
      *  regex expressions.  In the case of glob-style matching patterns,
      *  we convert them to regex expressions.
      */
     ignore_list = NULL;
     istat = opt_group_walk(&opts, "ignore", 0, ha7netd_ignlist_build,
			    (void *)&ignore_list);
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd_config_load(%d): Error loading options for HA7Net "
		 "devices to ignore ([ignore]); opt_group_walk() returned %d; "
		 "%s", __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  All done
      */
     istat = ERR_OK;
     if (ha7netd_list)
	  *ha7netd_list = opt_list;
     if (dev_list)
	  *dev_list = device_list;
     if (ign_list)
	  *ign_list = ignore_list;
done:
     opt_dispose(&opts);

     /*
      *  And return
      */
     return(istat);
}
