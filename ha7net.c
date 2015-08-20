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
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "utils.h"
#include "ha7net.h"
#include "bm.h"
#include "bm_const.h"
#include "crc.h"

static void ha7net_freestuff(char **results);
static int ha7net_getstuff(ha7net_t *ctx, char **results, size_t *reslens,
  size_t *nresults, size_t maxresults, const char *url, const bm_t *look_for,
  struct timeval *hrt);

static debug_proc_t  our_debug_ap;
static debug_proc_t *debug_proc = our_debug_ap;
static void         *debug_ctx  = NULL;
static int dbglvl     = 0;
static int do_debug   = 0;
static int do_trace   = 0;
static int do_verbose = 0;

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
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}


static void
detail2(const char *fmt, ...)
{
     if (debug_proc)
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
     if (do_trace && debug_proc)
     {
	  va_list ap;

	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}


void
ha7net_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     dbglvl     = flags;
     do_debug   = ((dbglvl & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     do_verbose = (do_debug && (dbglvl & DEBUG_VERBOSE)) ? 1 : 0;
     do_trace   = ((dbglvl & DEBUG_TRACE_HA7NET) && debug_proc) ? 1: 0;

     /*
      *  Push the settings down to the HTTP layer
      */
     http_debug_set(proc, ctx, flags);
}


static int
our_snprintf(const char *caller, size_t caller_line, char *str, size_t size,
	     const char *fmt, ...)
{
     int len;
     va_list ap;

     va_start(ap, fmt);
     len = vsnprintf(str, size, fmt, ap);
     va_end(ap);

     if (len < size)
	  return(ERR_OK);

     debug("%s(%u): Internal, compile-time buffer is too small; snprintf()"
	   "needs a larger buffer to format the HTTP GET URL; snprintf() "
	   "wants %d bytes; we are compiled with a buffer of size %u bytes",
	   caller ? caller : "our_snprintf", caller_line, len, size);

     return(ERR_NO);
}


void
ha7net_lib_done(void)
{
}


int
ha7net_lib_init(void)
{
     return(http_lib_init());
}


void
ha7net_done(ha7net_t *ctx, int flags)
{
     if (do_trace)
	  trace("ha7net_done(%d): Called with ctx=%p, flags=0x%x",
		__LINE__, ctx, flags);

     if (ctx)
     {
	  if (flags & HA7NET_FLAGS_POWERDOWN &&
	      (http_isopen(&ctx->hconn) || ctx->host_len))
	       ha7net_powerdownbus(ctx, 0);

	  if (ctx->hresp_dispose)
	  {
	       http_dispose(&ctx->hresp);
	       ctx->hresp_dispose = 0;
	  }

	  if (http_isopen(&ctx->hconn))
	  {
	       /*
		*  Release any lock
		*/
	       if (ctx->lockid_len && ctx->lockid[0])
		    (void)ha7net_releaselock(ctx);

	       /*
		*  And shut down the connection
		*/
	       http_close(&ctx->hconn);
	  }

	  ctx->host[0]    = '\0';
	  ctx->host_len   = 0;
	  ctx->port       = 0;
	  ctx->lockid_len = 0;
	  ctx->lockid[0]  = '\0';
     }
}


void
ha7net_close(ha7net_t *ctx, int flags)
{
     if (do_trace)
	  trace("ha7net_close(%d): Called with ctx=%p, flags=0x%x",
		__LINE__, ctx, flags);
     if (ctx)
     {
	  if (flags & HA7NET_FLAGS_POWERDOWN &&
	      (http_isopen(&ctx->hconn) || ctx->host_len))
	       ha7net_powerdownbus(ctx, 0);
	  if (http_isopen(&ctx->hconn))
	  {
	       /*
		*  Release any lock
		*/
	       if (ctx->lockid_len && ctx->lockid[0])
		    ha7net_releaselock(ctx);

	       /*
		*  And shut down the connection
		*/
	       http_close(&ctx->hconn);
	  }
     }
}


int
ha7net_open(ha7net_t *ctx, const char *host, unsigned short port,
	    unsigned int timeout, int flags)
{
     size_t hlen;
     int istat;

     if (do_trace)
	  trace("ha7net_open(%d): Called with ctx=%p, host =\"%s\" (%p), "
		"port=%u, timeout=%u, flags=0x%x",
		__LINE__, ctx, host ? host : "(null)", host, port, timeout,
		flags);

     if (!ctx)
     {
	  debug("ha7net_open(%d): Invalid call arguments supplied; ctx=NULL; "
		"call argument #1", __LINE__);
	  return(ERR_BADARGS);
     }
     else if (!host)
     {
	  debug("ha7net_open(%d): Invalid call arguments supplied; host=NULL; "
		"call argument #2", __LINE__);
	  return(ERR_BADARGS);
     }
     else if ((hlen = strlen(host)) > MAX_HOST_LEN)
     {
	  debug("ha7net_open(%d): Invalid call arguments; the supplied host "
		"name is too long; the host name may not exceed %d bytes; "
		"call argument #2", __LINE__, MAX_HOST_LEN);
	  return(ERR_BADARGS);
     }
     else if (!port || port != (0xffff & port))
     {
	  debug("ha7net_open(%d): Invalid call arguments; the supplied TCP "
		"port number is outside the range [1,65535]; call argument #3",
		__LINE__);
	  return(ERR_BADARGS);
     }

     memset(ctx, 0, sizeof(ha7net_t));
     ctx->port = (unsigned short)port;
     ctx->tmo  = timeout;

     /*
      *  It's actually safe *right now* to copy hlen+1 and thereby
      *  pick up our NUL terminator.  But, if I do that today, next
      *  week I or someone else will change the hlen > MAX_HOST_LEN
      *  check above and it will then it will no longer be okay...
      *  (I.e., changing the check to simply set hlen = MAX_HOST_LEN-1.)
      */
     memcpy(ctx->host, host, hlen);
     ctx->host[hlen] = 0;
     ctx->host_len = hlen;

     /*
      *  Since we do a lazy open of the HTTP connection, we need to
      *  initialize ctx->hconn.  Otherwise, we cannot use http_isopen()
      *  to tell us if we need to open the connection or not.
      */
     if (ERR_OK != (istat = http_init(&ctx->hconn)))
     {
	  detail("ha7net_open(%d): Call to http_init() has failed; "
		 "http_init() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Reset the bus?
      */
     if (!(flags & HA7NET_FLAGS_NORESET))
     {
	  istat = ha7net_resetbus(ctx, HA7NET_FLAGS_RELEASE);
	  if (istat != ERR_OK)
	       detail("ha7net_open(%d): Attempt to reset the 1-Wire bus "
		      "failed; ha7net_resetbus() returned %d; %s",
		      __LINE__, istat, err_strerror(istat));
     }

     /*
      *  All done
      */
     return(istat);
}


static void
ha7net_freestuff(char **results)
{
     if (do_trace)
	  trace("ha7net_freestuff(%d): Called with results=%p",
		__LINE__, results);

     while (results[0])
     {
	  free(results[0]);
	  results++;
     }
}


static int
ha7net_getstuff(ha7net_t *ctx, char **results, size_t *reslens,
		size_t *nresults, size_t maxresults, const char *url,
		const bm_t *look_for, struct timeval *hrt)
{
     struct timeval hrt_start, hrt_end;
     size_t i, nres, ulen;
     int istat;
     const unsigned char *uend, *uptr;

     if (do_trace)
	  trace("ha7net_getstuff(%d): Called with ctx=%p, results=%p, "
		"reslens=%p, nresults=%p, maxresults=%u, url=\"%s\" (%p), "
		"look_for->substr=\"%s\"",
		__LINE__, ctx, results, reslens, nresults, maxresults,
		url ? url : "(null)", url,
		look_for ? (const char *)look_for->substr : "(null)");
     if (!ctx || !url)
     {
	  debug("ha7net_getstuff(%d): Invalid call arguments supplied; "
		"ctx=%p, url=%p", __LINE__, ctx, url);
	  return(ERR_BADARGS);
     }
     else if (!ctx->host_len || !ctx->host[0] || !ctx->port)
     {
	  debug("ha7net_getstuff(%d): Invalid call arguments supplied; ctx=%p "
		"has suspect data fields; ctx->host_len=%u, ctx->host[0] = "
		"0x%02x, ctx->port=%u",
		__LINE__, ctx, ctx->host_len, ctx->host[0], ctx->port);
	  return(ERR_BADARGS);
     }

     /*
      *  Count of results found
      */
     nres = 0;

     /*
      *  Release any previous HTTP response? 
      */
     if (ctx->hresp_dispose)
     {
	  http_dispose(&ctx->hresp);
	  ctx->hresp_dispose = 0;
     }

     /*
      *  Open an HTTP connection if we don't have one already
      */
     if (!http_isopen(&ctx->hconn))
     {
	  istat = http_open(&ctx->hconn, ctx->host, ctx->port, ctx->tmo);
	  if (istat != ERR_OK)
	  {
	       detail("ha7net_getstuff(%d): Unable to open a TCP connection "
		      "to %s:%u; http_open() returned %d; %s",
		      __LINE__, ctx->host ? ctx->host : "(null)", ctx->port,
		      istat, err_strerror(istat));
	       goto done;
	  }
     }

     /*
      *  Send the HTTP request.  It will be of the form
      *
      *    "GET" SP <url> SP "HTTP/1.1" CRLF
      *    "Hostname:" SP <hostname> CRLF CRLF
      */
     if (dbglvl & DEBUG_HA7NET_XMIT)
	  detail2("ha7net_getstuff(%d): GET %s",
		  __LINE__, url ? url : "(null)");

     if (hrt)
	  gettimeofday(&hrt_start, NULL);
     istat = http_send_request(&ctx->hconn, "GET", url);
     if (istat != ERR_OK)
     {
	  detail("ha7net_getstuff(%d): Error sending the request \"GET %s "
		 "HTTP/1.1\"; http_send_request() returned %d; %s",
		 __LINE__, url, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Read the HTTP response
      */
     istat = http_read_response(&ctx->hconn, &ctx->hresp);
     if (hrt)
     {
	  gettimeofday(&hrt_end, NULL);
	  hrt->tv_sec  = (hrt_end.tv_sec  - hrt_start.tv_sec) / 2;
	  hrt->tv_usec = (hrt_end.tv_usec - hrt_start.tv_usec) / 2;
     }
     if (istat != ERR_OK)
     {
	  detail("ha7net_getstuff(%d): Error reading the HTTP response; "
		 "http_read_response() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     ctx->hresp_dispose = 1;

     /*
      *  Check the HTTP Status-Code
      */
     if (ctx->hresp.sta_code < 200 || ctx->hresp.sta_code > 299)
     {
	  debug("ha7net_getstuff(%d): Non-success (2yz) HTTP status code "
		"received; HTTP Status-Line is \"%.*s\"",
		__LINE__, ctx->hresp.sta ? ctx->hresp.sta_len : 6,
		ctx->hresp.sta ? ctx->hresp.sta : "(none)");
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Skip now if !results || !maxresults
      *  Presently we don't expect anyone to call us and just want a tally
      *  reported via nresults.
      */
     if (!results || !maxresults || !look_for)
     {
	  istat = ERR_OK;
	  goto done;
     }

     /*
      *  Loop over the message body searching for occurrences of
      *  the search string and then the following VALUE="xxx".  It's
      *  this string "xxx" which we are attempting to pluck out and
      *  pass back via results[].
      */
     uptr = (const unsigned char *)ctx->hresp.bdy;
     ulen = ctx->hresp.bdy_len;
     uend = uptr + ulen;
     do
     {
	  ssize_t len, pos;
	  char *tmp;
	  const unsigned char *uptr2, *uptr3;

	  /*
	   *  Look for the search string
	   */
	  pos = bm_search(uptr, ulen, look_for);
	  if (pos < 0)
	       goto bm_error;
	  else if (pos >= ulen)
	  {
	       if (nres)
		    /*
		     *  We've found at least one occurrence of the search
		     *  string.  That's good enough for me and I'm a computer!
		     */
		    break;

	       /*
		*  We've found NO occurrences of the search string
		*/
	       debug("ha7net_getstuff(%d): Unable to locate the search "
		     "string, '%s', in the HTTP response",
		     __LINE__,
		     look_for ? (const char *)look_for->substr : "(null)");
	       istat = ERR_NO;
	       goto done;
	  }

	  /*
	   *  uptr + pos is the start of the look_for string
	   *  uptr + pos + look_len is the start of the text immediately
	   *     following.
	   */
	  uptr += pos + look_for->sublen;
	  ulen -= pos + look_for->sublen;

	  /*
	   *  Now look for the following 'VALUE="'.  It must appear before
	   *  a following '>'.
	   */
	  pos = bm_search(uptr, ulen, &bm_info_value);
	  if (pos < 0)
	       goto bm_error;
	  else if (pos >= ulen)
	  {
	       if (nres)
		    /*
		     *  We've found at least one occurrence of the search
		     *  string.  That's good enough for me and I'm a computer!
		     */
		    break;

	       /*
		*  We've found NO occurrences of the search string
		*/
	       debug("ha7net_getstuff(%d): Unable to locate the search "
		     "string, '%s', in the HTTP response",
		     __LINE__,
		     look_for ? (const char *)look_for->substr : "(null)");
	       istat = ERR_NO;
	       goto done;
	  }

	  uptr3 = uptr;
	  while (*uptr3 && *uptr3 != '>')
	       uptr3++;
	  if (!*uptr3 || (uptr + pos) >= uptr3)
	  {
	       /*
		*  Something isn't right....
		*/
	       debug("ha7net_getstuff(%d): Unable to locate an occurrence of "
		     "'VALUE=\"...\">' after the search string '%s' in the "
		     "HTTP response",
		     __LINE__,
		     look_for ? (const char *)look_for->substr : "(null)");
	       istat = ERR_NO;
	       goto done;
	  }

	  /*
	   *  Last time I checked, the string 'VALUE="' has 7 octets
	   */
	  uptr += pos + bm_info_value.sublen;
	  ulen -= pos + bm_info_value.sublen;

	  /*
	   *  Look for the trailing double quote
	   */
	  uptr2 = uptr;
	  while(*uptr2 && *uptr2 != '"')
	       uptr2++;
	  if (*uptr2 != '"' || uptr2 > uptr3)
	  {
	       debug("ha7net_getstuff(%d): Unable to locate a closing '\"' "
		     "after a 'VALUE=\"' in the HTTP response", __LINE__);
	       istat = ERR_NO;
	       goto done;
	  }

	  len = uptr2 - uptr;
	  tmp = (char *)malloc(len + 1);
	  if (!tmp)
	  {
	       debug("ha7net_getstuff(%d): Insufficient virtual memory", 
		     __LINE__);
	       istat = ERR_NOMEM;
	       goto done;
	  }
	  memcpy(tmp, uptr, len);
	  tmp[len] = '\0';
	  if (reslens)
	       reslens[nres] = len;
	  results[nres++] = tmp;

	  if (dbglvl & DEBUG_HA7NET_RECV)
	       detail2("ha7net_getstuff(%d): Received \"%.*s\"",
		       __LINE__, len, (const char *)uptr);
	  if ((nres + 1) >= maxresults)
	       break;

	  /*
	   *  Now advance the pointer
	   */
	  uptr = uptr3 + 1;
	  if (uptr < uend)
	       ulen = uend - uptr;
	  else
	       ulen = 0;
     } while (ulen);

     istat = ERR_OK;
     goto done;

bm_error:
     istat = ERR_NO;

done:
     if (istat == ERR_OK)
     {
	  if (results && nres < maxresults)
	  {
	       results[nres] = NULL;
	       if (reslens)
		    reslens[nres] = 0;
	  }
	  if (nresults)
	       *nresults = nres;
     }
     else
     {
	  if (nresults)
	       *nresults = 0;
	  if (results)
	  {
	       size_t j;

	       for (j = 0; j < nres; j++)
	       {
		    if (results[j])
		    {
			 free(results[j]);
			 results[j] = NULL;
			 if (reslens)
			      reslens[j] = 0;
		    }
	       }
	       if (!nres && maxresults)
	       {
		    /* Doesn't happen when nres is 0 */
		    results[0] = NULL;
		    if (reslens)
			 reslens[0] = 0;
	       }
	  }
     }

     /*
      *  All done
      */
     return(istat);
}


int
ha7net_getlock(ha7net_t *ctx)
{
     int istat;
     size_t nresults, reslens[2];
     char *results[2];

     if (do_trace)
	  trace("ha7net_getlock(%d): Called with ctx=%p", __LINE__, ctx);

     /*
      *  Sanity checks
      */
     if (!ctx)
     {
	  debug("ha7net_getlock(%d): Invalid call arguments supplied; "
		"ctx=NULL; call argument #1", __LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Send the HTTP request for a lock and parse the response for a lock ID
      */
     nresults = 0;
     istat = ha7net_getstuff(ctx, results, reslens, &nresults, 2,
			     "/1Wire/GetLock.html", &bm_info_getlock, NULL);
     if (istat != ERR_OK)
     {
	  detail("ha7net_getlock(%d): Error obtaining a lock on the 1-Wire "
		 "bus; ha7net_getstuff() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     else if (!nresults || !results[0])
     {
	  debug("ha7net_getlock(%d): Error obtaining a lock on the 1-Wire "
		"bus; although ha7net_getstuff() returned a success, no lock "
		"ID was returned; most odd and annoying", __LINE__);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Save the lockid
      */
     ctx->lockid_len = reslens[0];
     if (ctx->lockid_len >= sizeof(ctx->lockid))
	  ctx->lockid_len = sizeof(ctx->lockid) - 1;
     memcpy(ctx->lockid, results[0], ctx->lockid_len);
     ctx->lockid[ctx->lockid_len] = '\0';

     /*
      *  And return a success
      */
     istat = ERR_OK;

done:
     /*
      *  Release the VM
      */
     if (nresults)
	  ha7net_freestuff(results);

     /*
      *  Finished
      */
     return(istat);
}


int
ha7net_releaselock(ha7net_t *ctx)
{
     int istat;
     char url[64 + MAX_LOCK_LEN + 1];

     if (do_trace)
	  trace("ha7net_releaselock(%d): Called with ctx=%p", __LINE__, ctx);
     /*
      *  Bozo check
      */
     if (!ctx)
     {
	  debug("ha7net_releaselock(%d): Invalid call arguments supplied; "
		"ctx=NULL; call argument #1", __LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Return now if there is no lock to release
      */
     if (!ctx->lockid_len)
	  /*
	   *  No lock to release
	   */
	  return(ERR_OK);

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/ReleaseLock.html?LockID=<lockid> HTTP/1.1
      */
     istat = our_snprintf("ha7net_releaselock", __LINE__, url, sizeof(url),
			  "/1Wire/ReleaseLock.html?LockID=%.*s",
			  ctx->lockid_len, ctx->lockid);
     if (istat != ERR_OK)
	  /*
	   *  our_snprintf() handles debug output
	   */
	  return(istat);

     /*
      *  Send the request
      */
     istat = ha7net_getstuff(ctx, NULL, NULL, NULL, 0, url, NULL, NULL);

     /*
      *  Clear the lock info, regardless of whether or not the request worked
      */
     ctx->lockid[0]  = '\0';
     ctx->lockid_len = 0;

     /*
      *  See if there was an error
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_releaselock(%d): Error releasing the lock on the "
		 "1-Wire bus; ha7net_getstuff() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

done:
     /*
      *  And return a result
      */
     return(istat);
}


static int
our_getlock(ha7net_t *ctx, const char *caller, size_t caller_line)
{
     int istat;

     istat = ha7net_getlock(ctx);
     if (istat == ERR_OK || !do_debug)
	  return(istat);

     detail("%s(%u): Unable to obtain a lock on the 1-Wire bus; "
	    "ha7net_getlock() returned %d; %s",
	    caller ? caller : "our_getlock", caller_line, istat,
	    err_strerror(istat));

     return(istat);
}


static int
our_releaselock(ha7net_t *ctx, const char *caller, size_t caller_line)
{
     int istat;

     istat = ha7net_releaselock(ctx);
     if (istat == ERR_OK || !do_debug)
	  return(istat);

     detail("%s(%u): Error releasing our lock on the 1-Wire bus; "
	    "ha7net_releaselock() returned %d; %s",
	    caller ? caller : "our_getlock", caller_line, istat,
	    err_strerror(istat));

     return(istat);
}


int
ha7net_powerdownbus(ha7net_t *ctx, int flags)
{
     int istat;
     char url[64 + MAX_LOCK_LEN + 1];

     if (do_trace)
	  trace("ha7net_powerdownbus(%d): Called with ctx=%p, flags=0x%x",
		__LINE__, ctx, flags);
     /*
      *  Bozo check
      */
     if (!ctx)
     {
	  debug("ha7net_powerdownbus(%d): Invalid call arguments; ctx=NULL", 
		__LINE__, ctx);
	  return(ERR_BADARGS);
     }

     /*
      *  If we don't have a lock, then get one
      */
     if (!ctx->lockid[0] || !ctx->lockid_len)
     {
          istat = our_getlock(ctx, "ha7net_powerdownbus", __LINE__);
	  if (istat != ERR_OK)
	       return(istat);
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/PowerDownBus.html
      *
      *  Optional parameters:
      *
      *     LockID = <10 digit lock id>
      */
     istat = our_snprintf("ha7net_powerdownbus", __LINE__,
			  url, sizeof(url),
			  "/1Wire/PowerDownBus.html?LockID=%.*s",
			  ctx->lockid_len, ctx->lockid);
     if (istat != ERR_OK)
	  /*
	   *  Debug output was handled by our_snprintf()
	   */
	  return(istat);

     /*
      *  Send the request
      */
     istat = ha7net_getstuff(ctx, NULL, NULL, NULL, 0, url, NULL, NULL);

     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_powerdownbus", __LINE__);

     /*
      *  Did ha7net_getstuff() return an error?
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_powerdownbus(%d): An error was encountered while "
		 "attempting to power down the 1-Wire bus", __LINE__);
     }
     else
	  ctx->current_device = NULL;

     /*
      *  All done
      */
     return(istat);
}


int
ha7net_resetbus(ha7net_t *ctx, int flags)
{
     int istat;
     char url[64 + MAX_LOCK_LEN + 1];

     if (do_trace)
	  trace("ha7net_resetbus(%d): Called with ctx=%p, flags=0x%x",
		__LINE__, ctx, flags);
     /*
      *  Bozo check
      */
     if (!ctx)
     {
	  debug("ha7net_resetbus(%d): Invalid call arguments; ctx=NULL", 
		__LINE__, ctx);
	  return(ERR_BADARGS);
     }

     /*
      *  If we don't have a lock, then get one
      */
     if (!ctx->lockid[0] || !ctx->lockid_len)
     {
          istat = our_getlock(ctx, "ha7net_resetbus", __LINE__);
	  if (istat != ERR_OK)
	       return(istat);
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/Reset.html
      *
      *  Optional parameters:
      *
      *     LockID = <10 digit lock id>
      */
     istat = our_snprintf("ha7net_resetbus", __LINE__,
			  url, sizeof(url), "/1Wire/Reset.html?LockID=%.*s",
			  ctx->lockid_len, ctx->lockid);
     if (istat != ERR_OK)
	  /*
	   *  Debug output was handled by our_snprintf()
	   */
	  return(istat);

     /*
      *  Send the request
      */
     istat = ha7net_getstuff(ctx, NULL, NULL, NULL, 0, url, NULL, NULL);

     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_resetbus", __LINE__);

     /*
      *  Did ha7net_getstuff() return an error?
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_resetbus(%d): An error was encountered while "
		 "attempting to reset the 1-Wire bus", __LINE__);
     }
     else
	  ctx->current_device = NULL;

     /*
      *  All done
      */
     return(istat);
}


int
ha7net_addressdevice(ha7net_t *ctx, device_t *dev, int flags)
{
     int istat;
     size_t nresults, reslens[2];
     char *results[2];
     char url[64 + MAX_LOCK_LEN + 32 + 32 + 1];

     if (do_trace)
	  trace("ha7net_addressdevice(%d): Called with ctx=%p, dev=%p, "
		"flags=0x%x", __LINE__, ctx, dev, flags);

     if (!ctx || !dev)
     {
	  debug("ha7net_addressdevice(%d): Invalid call arguments; ctx=%p, "
		"dev=%p; none of the preceding call arguments should be 0 "
		"(NULL)", __LINE__, ctx, dev);
	  return(ERR_BADARGS);
     }

     /*
      *  If some other device is currently addressed, then we need to reset
      *  the bus first.
      */
     if (ctx->current_device == dev)
	  /*
	   *  Device already addressed.  Return success.
	   */
	  return(ERR_OK);

     /*
      *  If we don't have a lock, then get one
      */
     if (!ctx->lockid[0] || !ctx->lockid_len)
     {
	  istat = our_getlock(ctx, "ha7net_addressdevice", __LINE__);
	  if (istat != ERR_OK)
	       return(istat);
     }

     /*
      *  If a device is currently selected, then
      *  we will need to reset the bus.
      */
     if (ctx->current_device)
     {
	  /*
	   *  Some other device is addressed; time to reset the
	   *  bus before addressing this device.
	   */
	  istat = ha7net_resetbus(ctx, flags);
	  if (istat != ERR_OK)
	  {
	       detail("ha7net_addressdevice(%d): Unable to reset the 1-Wire "
		      "bus; ha7net_resetbus() returned %d; %s",
		      __LINE__, istat, err_strerror(istat));
	       return(istat);
	  }
	  ctx->current_device = NULL;
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/AddressDevice.html?Address=<ROM code>
      *
      *  Optional parameters:
      *
      *     LockID = <10 digit lock id>
      */
     istat = our_snprintf("ha7net_addressdevice", __LINE__, url, sizeof(url),
			  "/1Wire/AddressDevice.html?Address=%s&LockID=%.*s",
			  dev->romid, ctx->lockid_len, ctx->lockid);
     if (istat != ERR_OK)
	  /*
	   *  Debug output handled by our_snprintf()
	   */
	  return(istat);

     /*
      *  Send the request
      */
     nresults = 0;
     istat = ha7net_getstuff(ctx, results, reslens, &nresults, 2, url,
			     &bm_info_addressdevice, NULL);
     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_addressdevice", __LINE__);

     /*
      *  Did ha7net_getstuff() return an error?
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_addressdevice(%d): An error was encountered while "
		 "attempting to select the device with ROM id \"%s\"; "
		 "ha7net_getstuff() returned %d; %s",
		 __LINE__, dev->romid, istat, err_strerror(istat));
     }
     else
     {
	  if (!nresults || strcmp(results[0], dev->romid))
	  {

	       /*
		*  dev-romid does not appear to be the selected device?
		*/
	       debug("ha7net_addressdevice(%d): An error was encountered "
		     "while attempting to select the device with ROM id "
		     "\"%s\"; the 1-Wire bus master returned a result but "
		     "that result did not indicate that the desired "
		     "device was selected; instead it seems to say that "
		     "\"%s\" was selected; most disconcerting",
		     __LINE__, dev->romid, results[0]);
	       istat = ERR_NO;
	  }
	  ha7net_freestuff(results);
     }

     /*
      *  Note that this device is selected
      */
     ctx->current_device = dev;

     /*
      *  All done
      */
     return(istat);
}


static int
ha7net_prelim(ha7net_t *ctx, device_t *dev, int flags)
{
     int istat;

     if (do_trace)
	  trace("ha7net_prelim(%d): Called with ctx=%p, dev=%p, flags=0x%x",
		__LINE__, ctx, dev, flags);
     /*
      *  Bozo check
      */
     if (!ctx)
     {
	  debug("ha7net_prelim(%d): Invalid call arguments; ctx=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }

     istat = ERR_OK;

     /*
      *  Obtain a lock on the 1-Wire bus
      */
     if (!ctx->lockid[0] || !ctx->lockid_len)
     {
	  istat = our_getlock(ctx, "ha7net_prelim", __LINE__);
	  if (istat != ERR_OK)
	       /*
		*  Debug output handled by our_getlock()
		*/
	       return(istat);
     }

     /*
      *  Select a specific device?
      *  Not needed when doing a WriteBlock.html?Address=<ROM id> as
      *  that HTTP request will reset the bus and then select the specified
      *  device.
      */
     /*
      *  If another device is selected, then reset the bus and select
      *  the device specified in our call argument list
      */     
     if (dev && !(flags & HA7NET_FLAGS_NOSELECT))
     {
	  if ((ctx->current_device && ctx->current_device != dev) ||
	      (!ctx->current_device && (flags & HA7NET_FLAGS_SELECT)))
	  {
	       istat = ha7net_addressdevice(ctx, dev, 0);
	       if (istat != ERR_OK)
		    detail("ha7net_prelim(%d): ha7net_addressdevice() call "
			   "failed and returned %d; %s",
			   __LINE__, istat, err_strerror(istat));
	  }
     }

     /*
      *  All done
      */
     return(istat);
}


void
ha7net_search_free(device_t *devices)
{
     dev_array_free(devices);
}


int
ha7net_search(ha7net_t *ctx, device_t **devices, size_t *ndevices,
	      unsigned char family_code, int cond_state, int flags)
{
     device_t *devs;
     int dispose, istat, len;
     size_t i, ncount, nresults, reslens[1024];
     char *results[1024];
     char url[64 + MAX_LOCK_LEN + 32 + 32 + 1];

     if (do_trace)
	  trace("ha7net_search(%d): Called with ctx=%p, devices=%p, "
		"ndevices=%p, family_code=0x%02x, cond_state=%d, flags=0x%x",
		__LINE__, ctx, devices, ndevices, family_code, cond_state,
		flags);
     /*
      *  Bozo check
      */
     if (!ctx || !devices)
     {
	  detail("ha7net_search(%d): Invalid call arguments supplied; ctx=%p, "
		 "devices=%p; both must be non-zero", __LINE__, ctx, devices);
	  return(ERR_BADARGS);
     }

     /*
      *  For the time being
      */
     *devices = NULL;
     if (ndevices)
	  *ndevices = 0;

     /*
      *  Flag indicating whether or not we have results to free
      */
     dispose = 0;

     /*
      *  If we don't have a lock, then get one
      */
     istat = ha7net_prelim(ctx, NULL, flags);
     if (istat != ERR_OK)
     {
	  detail("ha7net_search(%d): Unable to obtain a bus lock from the "
		 "1-Wire bus master; ha7net_prelim() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/Search.html
      *
      *  Optional parameters:
      *
      *     LockID      = <10 digit lock id>
      *     FamilyCode  = <2-byte hex family code>
      *     Conditional = "0" | "1"
      */
     len = snprintf(url, sizeof(url), "/1Wire/Search.html?LockID=%.*s%s",
					(int)ctx->lockid_len, ctx->lockid,
					cond_state ? "&Conditional=1" : "");
     if (family_code)
	  snprintf(url + len, sizeof(url) - len,
		   "&FamilyCode=%x", family_code);
     /*
      *  Send the request
      */
     nresults = 0;
     istat = ha7net_getstuff(ctx, results, reslens, &nresults, 1024, url,
			     &bm_info_search, NULL);

     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_search", __LINE__);

     /*
      *  Did ha7net_getstuff() return an error?
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_search(%d): An error was encountered while searching "
		 "the 1-Wire bus for devices; ha7net_getstuff() returned %d; "
		 "%s", __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     dispose = 1;

     /*
      *  Process the results of our ha7net_getstuff() call
      */
     if (!nresults)
     {
	  debug("ha7net_search(%d): Search returned no devices", __LINE__);
	  istat = ERR_OK;
	  goto done;
     }

     /*
      *  Ignore anything with a ROM code whose length is not 8 bytes
      *  (16 bytes when represented as hexadecimal)
      */
     ncount = 0;
     for (i = 0; i < nresults; i++)
	  if (reslens[i] == OWIRE_ID_LEN)
	       ncount++;
     if (!ncount)
     {
	  debug("ha7net_search(%d): Search returned no devices with 64 bit "
		"addresses", __LINE__);
	  istat = ERR_OK;
	  goto done;
     }

     /*
      *  Allocate storage for the device list
      */
     devs = dev_array(ncount);
     if (!devs)
     {
	  debug("ha7net_search(%d): Insufficient virtual memory", __LINE__);
	  istat = ERR_NOMEM;
	  goto done;
     }
     *devices = devs;
     if (ndevices)
	  *ndevices = ncount;

     /*
      *  Copy the devices to the list
      */
     for (i = 0; i < nresults; i++)
     {
	  if (reslens[i] == OWIRE_ID_LEN)
	  {
	       devs->fcode = (unsigned char)
		    (0xff & strtoul(results[i] + reslens[i] - 2, NULL, 16));
	       memcpy(devs->romid, results[i], OWIRE_ID_LEN);
	       devs->romid[OWIRE_ID_LEN] = '\0';
	       devs->driver = dev_driver_get(devs->fcode, NULL, 0);
	       devs++;
	  }
     }

done:
     if (dispose)
	  ha7net_freestuff(results);

     /*
      *  And return our results
      */
     return(istat);
}


void
ha7net_readpages_free(char *data)
{
     if (data)
	  free(data);
}


int
ha7net_readpages(ha7net_t *ctx, device_t *dev, char **data, size_t *dlen,
		 size_t start_page, size_t npages, int flags)
{
     int dispose, istat;
     size_t len, n, nresults, reslens[HA7NET_MAX_RESULTS];
     char *results[HA7NET_MAX_RESULTS];
     char url[64 + MAX_LOCK_LEN + 32 + 32 + 32 + 1];

     if (do_trace)
	  trace("ha7net_readpages(%d): Called with ctx=%p, dev=%p, data=%p, "
		"dlen=%d, start_page=%u, npages=%u, flags=0x%x",
		__LINE__, ctx, dev, data, dlen, start_page, npages, flags);

     /*
      *  Bozo checks
      */
     if (!ctx)
     {
	  debug("ha7net_readpages(%d): Invalid call arguments supplied; "
		"ctx=NULL; ctx must be non-NULL", __LINE__);
	  return(ERR_BADARGS);
     }
     else if (npages >= HA7NET_MAX_RESULTS)
     {
	  debug("ha7net_readpages(%d): Too many pages requested; this code is "
		"presently compiled to only allow %d pages to be returned; "
		"sorry", __LINE__, (HA7NET_MAX_RESULTS - 1));
	  return(ERR_BADARGS);
     }

     /*
      *  For the time being
      */
     if (data)
	  *data = NULL;
     if (dlen)
	  *dlen = 0;

     /*
      *  Flag indicating whether or not we have results to free
      */
     dispose = 0;

     /*
      *  Lock's and device selection
      *  ReadPages.html will do a bus reset and device select when
      *  dev != NULL.  As such, we could just pass NULL for dev
      *  below and dispense with "| HA7NET_FLAGS_NOSELECT".  However,
      *  at some point in the future it's possible (but unlikely)
      *  that we may want to use dev for some other purpose in
      *  ha7net_prelim().
      */
     istat = ha7net_prelim(ctx, dev, flags | HA7NET_FLAGS_NOSELECT);
     if (istat != ERR_OK)
     {
	  detail("ha7net_readpages(%d): Unable to obtain a bus lock from the "
		 "1-Wire bus master; ha7net_prelim() returned %d; %s", 
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/ReadPages.html
      *
      *  Optional parameters:
      *
      *     LockID      = <10 digit lock id>
      *     Address     = <ROM code>
      *     StartPage   = <start-page>       <!-- default = 0 -->
      *     PagesToRead = <pages-to-read>    <!-- default = 1 -->
      */
     if (!npages)
	  npages = 1;
     if (dev && !(flags & HA7NET_FLAGS_NOSELECT))
	  istat = our_snprintf("ha7net_readpages", __LINE__,
			       url, sizeof(url),
"/1Wire/ReadPages.html?LockID=%.*s&Address=%s&StartPage=%d&PagesToRead=%d",
			       ctx->lockid_len, ctx->lockid, dev->romid,
			       start_page, npages);
     else
	  istat = our_snprintf("ha7net_readpages", __LINE__,
			       url, sizeof(url),
"/1Wire/ReadPages.html?LockID=%.*s&StartPage=%d&PagesToRead=%d",
			       ctx->lockid_len, ctx->lockid,
			       start_page, npages);
	  
     if (istat != ERR_OK)
	  /*
	   *  Debug output handled by our_snprintf()
	   */
	  return(istat);

     /*
      *  Send the request
      */
     nresults = 0;
     istat = ha7net_getstuff(ctx, results, reslens, &nresults,
			     HA7NET_MAX_RESULTS, url, &bm_info_readpages,
			     dev ? &dev->lastcmd : NULL);
     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_readpages", __LINE__);

     /*
      *  Now handle the ha7net_getstuff() results
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_readpages(%d): An error was encountered while "
		 "searching the 1-Wire bus for devices; ha7net_getstuff() "
		 "returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     dispose = 1;

     /*
      *  Return an error if no pages of data were returned
      */
     if (!nresults || !results[0])
     {
	  debug("ha7net_readpages(%d): Although ha7net_getstuff() returned a "
		"success code, no pages of data were returned; most odd",
		__LINE__);
	  istat = ERR_NO;
	  goto done;
     }
     /*
      *  Assume the device to be correctly selected
      */
     if (dev)
	  ctx->current_device = dev;

     /*
      *  Now prepare to return the results
      */
     len = 0;
     for (n = 0; n < nresults; n++)
	  if (results[n])
	       len += reslens[n];

     if (data)
     {
	  char *ptr;
	  ptr = (char *)malloc(len + 1);
	  if (!ptr)
	  {
	       debug("ha7net_readpages(%d): Insufficient virtual memory",
		     __LINE__);
	       istat = ERR_NOMEM;
	       goto done;
	  }
	  *data = ptr;
	  for (n = 0; n < nresults; n++)
	  {
	       if (results[n])
	       {
		    memcpy(ptr, results[n], reslens[n]);
		    ptr += reslens[n];
		    free(results[n]);
	       }
	  }
	  *ptr = '\0';
	  /*
	   *  We freed it already
	   */
	  dispose = 0;
     }
     if (dlen)
	  *dlen = len;

     istat = ERR_OK;

done:
     /*
      *  And return a result
      */
     if (dispose)
	  ha7net_freestuff(results);

     /*
      *  All done
      */
     return(istat);
}


int
ha7net_readpages_ex(ha7net_t *ctx, device_t *dev, unsigned char *data,
		    size_t minlen, size_t start_page, size_t npages, int flags)
{
     char *cdata;
     size_t clen;
     int istat;

     if (do_trace)
	  trace("ha7net_readpages_ex(%d): Called with ctx=%p, dev=%p, "
		"data=%p, minlen=%d, start_page=%u, npages=%u, flags=0x%x",
		__LINE__, ctx, dev, data, minlen, start_page, npages, flags);

     /*
      *  Test our inputs
      */
     if (!ctx || !data)
     {
	  debug("ha7net_readpages_ex(%d): Invalid call arguments supplied; "
		"ctx=%p, data=%p; all of the preceding must be non-zero",
		__LINE__, ctx, data);
	  return(ERR_BADARGS);
     }

     cdata = NULL;
     clen  = 0;
     istat = ha7net_readpages(ctx, dev, &cdata, &clen, start_page, npages,
			      flags);
     if (istat != ERR_OK)
     {
	  if (cdata)
	       ha7net_readpages_free(cdata);
     }
     else if (!cdata)
     {
	  debug("ha7net_readpages_ex(%d): Although ha7net_readpages() "
		"returned a success, it returned no data; that should "
		"never happen", __LINE__);
	  return(ERR_NO);
     }
     else if (clen < (2 * minlen))
     {
	  debug("ha7net_readpages_ex(%d): ha7net_readpages() did not "
		"return the expected number of bytes; it was expected to "
		"return 2 * %u bytes, but instead only returned %u",
		__LINE__, minlen, clen);
	  ha7net_writeblock_free(cdata);
	  return(ERR_NO);
     }

     Hex2Byte(data, cdata, clen);

     ha7net_readpages_free(cdata);

     return(ERR_OK);
}


void
ha7net_writeblock_free(char *data)
{
     if (data)
	  free(data);
}


int
ha7net_writeblock(ha7net_t *ctx, device_t *dev, char **data, size_t *dlen,
		  const char *cmd, int flags)
{
     int dispose, istat;
     size_t len, nresults, reslens[2];
     char *results[2], *url, urlbuf[128];

     if (do_trace)
	  trace("ha7net_writeblock(%d): Called with ctx=%p, dev=%p, data=%p, "
		"dlen=%d, cmd=\"%s\" (%p), flags=0x%x",
		__LINE__, ctx, dev, data, dlen, cmd ? cmd : "(null)", cmd,
		flags);

     /*
      *  Bozo check
      */
     if (!ctx || !cmd)
     {
	  debug("ha7net_writeblock(%d): Invalid call arguments supplied; "
		"ctx=%p, cmd=%p; all of the preceding must be non-zero",
		__LINE__, ctx, cmd);
	  return(ERR_BADARGS);
     }

     /*
      *  For the time being
      */
     if (data)
	  *data = NULL;
     if (dlen)
	  *dlen = 0;

     /*
      *  Flag indicating whether or not we have results to free
      */
     dispose = 0;
     url     = NULL;

     /*
      *  Obtain a lock on the 1-Wire bus if we don't have one already.
      *  Since WriteBlock.html will reset the bus and select the device,
      *  we don't need to pass "dev" to ha7net_prelim().  However, to
      *  future-proof the code, we go ahead and do so and set
      *  HA7NET_FLAGS_NOSELECT.
      */
     istat = ha7net_prelim(ctx, dev, flags | HA7NET_FLAGS_NOSELECT);
     if (istat != ERR_OK)
     {
	  detail("ha7net_writeblock(%d): Unable to obtain a bus lock from the "
		 "1-Wire bus master; ha7net_prelim() returned %d; %s", 
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }

     /*
      *  Build the URL for the HTTP request:
      *
      *     GET /1Wire/WriteBlock.html?Address=<ROM-id>&LockID=<lock-id> \
      *              &Data=<cmd>
      */
     len = ctx->lockid_len + strlen(cmd) + 64;
     if (dev && !(flags & HA7NET_FLAGS_NOSELECT))
	  /* &Address= */
	  len += strlen(dev->romid) + 9;
     if (len >= sizeof(urlbuf))
     {
	  len++;
	  url = (char *)malloc(len);
	  if (!url)
	  {
	       debug("ha7net_writeblock(%d): Insufficient virtual memory",
		     __LINE__);
	       istat = ERR_NOMEM;
	       goto done;
	  }
     }
     else
     {
	  url = urlbuf;
	  len = sizeof(urlbuf);
     }
     if (dev && !(flags & HA7NET_FLAGS_NOSELECT))
	  istat = our_snprintf("ha7net_writeblock", __LINE__, url, len,
			       "/1Wire/WriteBlock.html?Address=%s&LockID=%.*s"
			       "&Data=%s",
			       dev->romid, ctx->lockid_len, ctx->lockid, cmd);
     else
	  istat = our_snprintf("ha7net_writeblock", __LINE__, url, len,
			       "/1Wire/WriteBlock.html?LockID=%.*s&Data=%s",
			       ctx->lockid_len, ctx->lockid, cmd);
     if (istat != ERR_OK)
	  return(istat);

     /*
      *  Send the request
      */
     nresults = 0;
     istat = ha7net_getstuff(ctx, results, reslens, &nresults, 2, url,
			     &bm_info_writeblock, dev ? &dev->lastcmd : NULL);
     if (url && url != urlbuf)
     {
	  free(url);
	  url = NULL;
     }

     /*
      *  Clear the lock?
      */
     if (flags & HA7NET_FLAGS_RELEASE)
	  our_releaselock(ctx, "ha7net_writeblock", __LINE__);

     /*
      *  Now handle the ha7net_getstuff() results
      */
     if (istat != ERR_OK)
     {
	  detail("ha7net_writeblock(%d): An error was encountered; "
		 "ha7net_getstuff() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     dispose = 1;

     /*
      *  Return an error if no data was returned
      */
     if (!nresults || !results[0])
     {
	  debug("ha7net_writeblock(%d): Although ha7net_getstuff() returned a "
		"success code, no pages of data were returned; most odd",
		__LINE__);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Consider the device selected
      */
     if (dev)
	  ctx->current_device = dev;

     /*
      *  Return our results
      */
     if (data)
     {
	  *data = results[0];
	  results[0] = NULL;
	  dispose = 0;
     }
     if (dlen)
	  *dlen = reslens[0];

     istat = ERR_OK;

done:
     if (url && url != urlbuf)
	  free(url);
     if (dispose)
	  ha7net_freestuff(results);

     /*
      *  All done
      */
     return(istat);
}


int
ha7net_writeblock_ex(ha7net_t *ctx, device_t *dev, unsigned char *data,
		     size_t minlen, const char *cmd, ha7net_crc_t *crc_info,
		     int flags)
{
     int attempts, istat;
     char *cdata;
     size_t clen, crc_width, end_byte;
     unsigned char pdata[HA7NET_WRITEBLOCK_MAX];

     if (do_trace)
	  trace("ha7net_writeblock_ex(%d): Called with ctx=%p, dev=%p, "
		"data=%p, minlen=%u, cmd=\"%s\" (%p), crc=%p, flags=0x%x",
		__LINE__, ctx, dev, data, minlen, cmd ? cmd : "(null)", cmd,
		crc_info, flags);

     /*
      *  Test our inputs
      */
     if (!ctx || !cmd)
     {
	  debug("ha7net_writeblock_ex(%d): Invalid call arguments supplied; "
		"ctx=%p, cmd=%p; all of the preceding must be non-zero",
		__LINE__, ctx, cmd);
	  return(ERR_BADARGS);
     }

     attempts = (flags & HA7NET_FLAGS_NORESEND) ? 0xffffff : 1;
loop:
     cdata = NULL;
     clen  = 0;
     istat = ha7net_writeblock(ctx, dev, &cdata, &clen, cmd, flags);
     if (istat != ERR_OK)
	  return(istat);
     else if (!cdata)
     {
	  debug("ha7net_writeblock_ex(%d): Although ha7net_writeblock() "
		"returned a success, it returned no data; that should "
		"never happen", __LINE__);
	  return(ERR_NO);
     }
     else if (clen < (2*minlen))
     {
	  debug("ha7net_writeblock_ex(%d): ha7net_writeblock() did not "
		"return the expected number of bytes; it was expected to "
		"return 2 * %u bytes, but instead only returned %u",
		__LINE__, minlen, clen);
	  ha7net_writeblock_free(cdata);
	  return(ERR_NO);
     }

     if (!data)
     {
	  if (!crc_info || crc_info->algorithm == HA7NET_CRC_NONE)
	  {
	       ha7net_writeblock_free(cdata);
	       return(ERR_OK);
	  }
	  data = pdata;
	  if (!minlen)
	       minlen = strlen(cmd) >> 1;
	  if (minlen > HA7NET_WRITEBLOCK_MAX)
	       minlen = HA7NET_WRITEBLOCK_MAX;
     }
     Hex2Byte(data, cdata, 2 * minlen);

     ha7net_writeblock_free(cdata);

     if (!crc_info || crc_info->algorithm == HA7NET_CRC_NONE)
	  return(ERR_OK);

     crc_width = (crc_info->algorithm == HA7NET_CRC_16) ? 2 : 1;
     end_byte = crc_width + (crc_info->start_byte + crc_info->nbytes - 1);

     if ((end_byte + 1) > minlen)
     {
	  debug("ha7net_writeblock_ex(%d): Insufficient data to perform the "
		"requested CRC check; read %u bytes but need %u",
		__LINE__, minlen, 1 + end_byte);
	  return(ERR_OK);
     }

     if (crc_info->algorithm == HA7NET_CRC_16)
     {
	  /*
	   *  Compute the CRC16 over the data
	   *  The CRC check passes if the resulting CRC is 0xB001
	   */
	  int crc;
	  size_t i, j, k;

	  crc = 0;
	  for (i = crc_info->start_byte; i <= end_byte; i++)
	       crc = crc16(crc, data[i]);
	  if (crc != 0xB001)
	       goto crc_fail;
	  if (crc_info->repeat_every > 0)
	  {
	       k   = crc_info->repeat_every + 2;
	       j   = 0;
	       crc = 0;
	       for (i = end_byte + 1; i < minlen; i++)
	       {
		    crc = crc16(crc, data[i]);
		    if (++j == k)
		    {
			 if (crc != 0xB001)
			      goto crc_fail;
			 crc = 0;
			 j   = 0;
		    }
	       }
	  }
	  return(ERR_OK);
     }
     else if (crc_info->algorithm == HA7NET_CRC_8)
     {
	  /*
	   *  Compute the CRC8 over the data *and* the CRC itself
	   *  The CRC check passes if the resulting CRC is 0x00
	   */
	  unsigned char crc;
	  size_t i, j, k;

	  crc = 0;
	  for (i = crc_info->start_byte; i <= end_byte; i++)
	       crc = crc8(crc, data[i]);
	  if (crc != 0x00)
	       goto crc_fail;
	  if (crc_info->repeat_every > 0)
	  {
	       k   = crc_info->repeat_every + 1;
	       j   = 0;
	       crc = 0;
	       for (i = end_byte + 1; i < minlen; i++)
	       {
		    crc = crc8(crc, data[i]);
		    if (++j == k)
		    {
			 if (crc != 0x00)
			      goto crc_fail;
			 crc = 0;
			 j   = 0;
		    }
	       }
	  }
	  return(ERR_OK);
     }
     else
     {
	  debug("ha7net_writeblock_ex(%d): Invalid value supplied for the "
		"CRC algorithm; crc->algorithm=%d",
		__LINE__, crc_info->algorithm);
	  return(ERR_BADARGS);
     }

     /*
      *  CRC test failed
      */
crc_fail:
     detail("ha7net_writeblock_ex(%d): CRC check failed", __LINE__);

     /*
      *  Try an additional write/read
      */
     if (++attempts <= 2)
	  goto loop;

     return(ERR_CRC);
}


const char *
ha7net_last_response(ha7net_t *ctx, size_t *len)
{
     if (!ctx || !ctx->hresp_dispose)
     {
	  if (len)
	       *len = 0;
	  return(NULL);
     }
     if (len)
	  *len = ctx->hresp.bdy_len;
     return(ctx->hresp.bdy);
}
