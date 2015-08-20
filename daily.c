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
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "err.h"
#include "debug.h"
#include "os.h"
#include "device.h"
#include "daily.h"

static time_t midnight(time_t now);
static int sleep_until_midnight(os_shutdown_t *sinfo);
static void daily_grind(void *ctx);

static debug_proc_t our_debug_ap;
static void debug(const char *fmt, ...);
static void info(const char *fmt, ...);

typedef struct daily_list_s {
     struct daily_list_s *next;
     device_t            *devices;
} daily_list_t;

static os_shutdown_t      *shutdown_info = NULL;
static os_pthread_mutex_t  mutex;
static int                 initialized = 0;
static daily_list_t       *daily_list = NULL;

static debug_proc_t  our_debug_ap;
static debug_proc_t *debug_proc = our_debug_ap;
static void         *debug_ctx  = NULL;
static int dbglvl     = 0;
static int do_debug   = 0;
static int do_trace   = 0;

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
daily_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     dbglvl     = flags;
     do_debug   = ((flags & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     do_trace   = ((flags & DEBUG_TRACE_XML) && debug_proc) ? 1 : 0;
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


void
daily_lib_done(void)
{
     daily_list_t *tmp;

     if (!initialized)
	  return;

     os_pthread_mutex_lock(&mutex);
     while (daily_list)
     {
	  tmp = daily_list;
	  daily_list = daily_list->next;
	  free(tmp);
     }
     initialized = 0;
     os_pthread_mutex_unlock(&mutex);
     os_pthread_mutex_destroy(&mutex);
}


int
daily_lib_init(void)
{
     if (initialized)
	  return(0);
     os_pthread_mutex_init(&mutex, NULL);
     initialized = 1;
     return(0);
}


int
daily_add_devices(device_t *devices)

{
     daily_list_t *tmp;

     /*
      *  Return now if there's nothing to do
      */
     if (!devices)
	  return(ERR_OK);

     /*
      *  Allocate a new node for the list of device lists
      */
     tmp = (daily_list_t *)malloc(sizeof(daily_list_t));
     if (!tmp)
	  return(ERR_NOMEM);
     tmp->devices = devices;

     /*
      *  And add this node into the list of device lists
      */
     os_pthread_mutex_lock(&mutex);
     tmp->next = daily_list;
     daily_list = tmp;
     os_pthread_mutex_unlock(&mutex);

     /*
      *  All done
      */
     return(ERR_OK);
}


static time_t
midnight(time_t now)
{
     struct tm tm_now;

     if (now == (time_t)0)
	  now = time(NULL);
     localtime_r(&now, &tm_now);

     tm_now.tm_sec = 0;
     tm_now.tm_min = 0;
     tm_now.tm_hour = 0;
     tm_now.tm_mday += 1;

     return(mktime(&tm_now));
}


static int
sleep_until_midnight(os_shutdown_t *sinfo)
{
     time_t now;

     if (!sinfo)
	  sinfo = shutdown_info;

     now = time(NULL);
     return(os_shutdown_sleep(sinfo, 1000 * (int)difftime(midnight(now),now)));
}


static void
daily_grind(void *ctx)
{
     int istat;
     os_shutdown_t *sinfo = (os_shutdown_t *)ctx;
     daily_list_t *tmp;

     if (!sinfo)
	  sinfo = shutdown_info;

     info("midnight_thread(%d): Midnight thread started; sinfo=%p",
	  __LINE__, sinfo);

     /*
      *  Let the world know that we are running
      */
     os_shutdown_thread_incr(sinfo);

     /*
      *  Now go into our endless loop, waking at midnight
      */
     while (!(istat = sleep_until_midnight(sinfo)))
     {
	  os_pthread_mutex_lock(&mutex);

	  info("midnight_thread(%d): Moving stats from \"today\" to "
	       "\"yesterday\"", __LINE__);

	  tmp = daily_list;
	  while (tmp)
	  {
	       dev_hi_lo_reset(tmp->devices);
	       tmp = tmp->next;
	  }
	  os_pthread_mutex_unlock(&mutex);

	  /*
	   *  Sleep for a few secnds: by 21 March 2010, systems had become
	   *  fast enough that the above easily finishes in well under a
	   *  second and thus sleep_until_midnight() can instantly return
	   *  rather than waiting 24:00:00.00 (a day).
	   */
	  os_sleep(10 * 1000); /* 10 seconds */
     }

     if (istat == -1)
	  debug("midnight_thread(%d): sleep_until_midnight() returned an "
		"error; errno=%d; %s", __LINE__, errno, strerror(errno));

     /*
      *  If we reach this point, then a shutdown has been requested
      */
     info("midnight_thread(%d): Shutdown requested; istat=%d",
	  __LINE__, istat);

     /*
      *  Remove ourselves from the ranks of the living
      */
     os_shutdown_thread_decr(sinfo);
}

typedef void *(*pthread_startroutine_t)(void *);

void
daily_shutdown_begin(void)
{
     if (shutdown_info)
	  os_shutdown_begin(shutdown_info);
}


int
daily_shutdown_finish(unsigned int seconds)
{
     int istat;

     if (!shutdown_info)
	  return(ERR_OK);

     istat = os_shutdown_finish(shutdown_info, seconds);
     shutdown_info = NULL;

     return(istat ? ERR_NO : ERR_OK);
}


int
daily_shutdown(unsigned int seconds)
{
     daily_shutdown_begin();
     return(daily_shutdown_finish(seconds));
}


int
daily_start(void)
{
     int istat;
     pthread_t t_dummy;
     pthread_attr_t t_stack;

     shutdown_info = NULL;
     istat = os_shutdown_create(&shutdown_info);
     if (istat)
     {
	  int save_errno = errno;
	  debug("midnight_thread_start(%d): Unable to create shutdown "
		"mutices and condition signals; os_shutdown_create() "
		"returned %d; %s", __LINE__, errno, strerror(errno));
	  if (save_errno == ENOMEM)
	       return(ERR_NOMEM);
	  else if (save_errno == EINVAL)
	       return(ERR_BADARGS);
	  else
	       return(ERR_NO);
     }
     else if (!shutdown_info)
     {
	  debug("midnight_thread_start(%d): Unable to create shutdown "
		"mutices and condition signals; os_shutdown_create() "
		"returned a success code of %d but NULL for the pointer to "
		"the context it created; coding error?", __LINE__);
	  return(ERR_NO);
     }

     /*
      *  64K of stack is well more than this little thread will ever need
      */
     pthread_attr_init(&t_stack);
     pthread_attr_setstacksize(&t_stack, 1024 * 64);

     /*
      *  Thread is a detached thread
      */
     pthread_attr_setdetachstate(&t_stack, PTHREAD_CREATE_DETACHED);

     /*
      *  Launch the thread
      */
     istat = pthread_create(&t_dummy, &t_stack,
			    (pthread_startroutine_t)daily_grind,
			    (void *)shutdown_info);
     if (!istat)
	  return(ERR_OK);

     debug("midnight_thread_start(%d): Failed to start the midnight thread; "
	   "pthread_create() returned %d; %s",
	   __LINE__, errno, strerror(errno));

     return(ERR_NO);
}
