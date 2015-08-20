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
#if !defined(_WIN32)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#endif

#include "err.h"
#include "os.h"
#include "ha7netd.h"

/*
 *  Shutdown signalling
 */
static os_shutdown_t *shutdown_info = NULL;
static int            shutdown_flag = 0;

/*
 *  Flag indicating whether or not we're running in the background (as a
 *  daemon) or not.  Used to direct debug and error output: goes to stderr
 *  when running in the foreground and to syslog [the Windows event log]
 *  when running in the background [as a service].
 */
static int ha7netd_inbg = 0;

/*
 *  Default facility name to associate with event log records
 */
#if defined(_WIN32)
const char default_facility[] = "ha7netd";
#else
const char default_facility[] = "local3";
#endif

/*
 *  Event log context: pointer to the facility name string for Windows,
 *  and a pointer to an integer containing the syslog LOG_facility-name
 *  bit mask for Unix.
 */
static void *log_ctx = NULL;

/*
 *  Shut down the event log
 */
void
ha7netd_dbglog_close(void)
{
     if (log_ctx)
	  os_log_close(log_ctx);
     log_ctx = NULL;
}


/*
 *  Open the event log
 */

static const char *log_facility = default_facility;

void
ha7netd_dbglog_open(void)
{
     if (!log_ctx)
	  log_ctx = os_log_open(log_facility);
}


/*
 *  Emit an event log record
 */
void
ha7netd_dbglog(void *ctx, int reason, const char *fmt, va_list ap)
{
     (void)ctx;

     if (!ha7netd_inbg)
     {
	  /* os_mutex_lock(&mutex_debug); */
	  vfprintf(stderr, fmt, ap);
	  fputc('\n', stderr);
	  fflush(stderr);
	  /* os_mutex_unlock(&mutex_debug); */
     }
     else
     {
	  /*
	   *  Trust these routines to have their own mutices
	   */
	  if (!log_ctx)
	       log_ctx = os_log_open(log_facility);
	  os_log(log_ctx, reason, fmt, ap);
     }
}


/*
 *  Used by ha7netd to incrementally set our background/foreground
 *  state as well as our event logging facility name.
 */

void
ha7netd_dbglog_set(int inbg, const char *facility, int flags)
{
     if (flags & 1)
	  ha7netd_inbg = inbg;
     if (flags & 2)
	  log_facility = facility ? facility : default_facility;
}


#include <signal.h>

static void
ha7netd_signal_handler(int sig)
{
     if (sig != SIGTERM)
	  return;

     shutdown_flag = -1;
     dbglog("ha7netd_signal_handler(%d): SIGTERM received; initiating a "
	    "shutdown", __LINE__);
     os_shutdown_begin(shutdown_info);
}


void
ha7netd_shutdown_wait(void)
{
     /*
      *  Wait until we are awakened by ha7netd_signal_handler()
      */
     while (!os_shutdown_wait(shutdown_info))
	  ;

     /*
      *  This call is merely to release resources
      */
     os_shutdown_finish(shutdown_info, 0);
}


int
ha7netd_shutdown_create(void)
{
     int istat;

     shutdown_info = NULL;
     shutdown_flag = -1;
     istat = os_shutdown_create(&shutdown_info);
     if (istat != ERR_OK || !shutdown_info)
     {
	  dbglog("ha7netd_shutdown_create(%d): Unable to create a shutdown "
		 "resource", __LINE__);
	  return(istat);
     }

     if (SIG_ERR == signal(SIGTERM, ha7netd_signal_handler))
     {
	  dbglog("ha7netd_shutdown_create(%d): Unable to establish a signal "
		 "handler to handle shutdown requests", __LINE__);
	  os_shutdown_finish(shutdown_info, 0);
	  return(ERR_NO);
     }

     return(ERR_OK);
}


#if defined(_WIN32)

/*
 *  On Windows we run as a service and leave it to Windows
 *  to ensure that only one copy of ha7netd is running.
 *  Thus the next two routines are NOOPs on Windows.
 */
void
ha7netd_allow_others(void)
{
}

void
ha7netd_exclude_others(void)
{
}

#else

static const char *lockfile    = "/tmp/.ha7netd.lock";
static int         lockfile_fd = -1;

/*
 *  Remove our flock() lock on a lock file and then delete the file itself.
 */

static void
lockfile_remove(int fd)
{
     struct flock lb;

     if (fd < 0)
	  return;

     /*
      *  Unlock the file.  Yeah, this should be superfluous given
      *  the fact that we will next close the file.  However, we've
      *  seen bugs from time to time with HP/UX and AIX whereby
      *  the lock doesn't go away at process exit...
      */
     lb.l_type   = F_UNLCK;
     lb.l_whence = SEEK_SET;
     lb.l_start  = 0;
     lb.l_len    = 0;
     fcntl(fd, F_SETLK, &lb);

     /*
      *  Close the file
      */
     close(fd);

     /*
      *  And delete it
      */
     unlink(lockfile);
}


/*
 *  Create a lock file and then put an advisory flock() lock on it.
 *  If the file already exists, that's okay: it's only a problem if
 *  it's currently locked.  If it is currently locked, then we can
 *  read pid out of it to present the pid of the other running
 *  process.
 */

static int
lockfile_create(int *fd, pid_t *pid)
{
     char buf[64];
     int fdd;
     struct flock lb;
     size_t len;

     /*
      *  Initializations
      */
     if (fd)
	  *fd = -1;
     if (pid)
	  *pid = (pid_t)0;

     /*
      *  Attempt to open the lock file
      */
     fdd = open(lockfile, O_RDWR | O_CREAT, 0644);
     if (fdd < 0)
	  /*
	   *  This isn't good news folks.... Someone else
	   *  owns the file and it's protected against our
	   *  having write permission....
	   */
	  return(-1);

     /*
      *  We successfully opened the file.  See if we can lock it.
      */
     lb.l_type   = F_WRLCK;
     lb.l_whence = SEEK_SET;
     lb.l_start  = 0;
     lb.l_len    = 0;
     if (-1 == fcntl(fdd, F_SETLK, &lb))
     {
	  /*
	   *  Locked by another pid
	   */
	  if (pid)
	  {
	       /*
		*  Attempt to obtain the pid stored in the file
		*/	
	       ssize_t bytes;
	       unsigned u1, u2;

	       u1 = u2 = 0;
	       bytes = read(fdd, buf, sizeof(buf));
	       if (0 < sscanf(buf, "%u %u", &u1, &u2))
		    *pid = (pid_t)u1;
	  }
	  close(fdd);
	  return(1);
     }

     /*
      *  Woo-who!  We're king of the file!  We got the lock!
      *  Update the file with our pid!
      */
     len = sprintf(buf, "%u %u\n", (unsigned)getpid(), (unsigned)time(NULL));
     write(fdd, buf, len);

     /*
      *  Return the file descriptor for later use to unlock the file
      */
     if (*fd)
	  *fd = fdd;

     return(0);
}


/*
 *  We're shutting down; allow another instance of ourself to
 *  run by removing our lock file.
 */

void
ha7netd_allow_others(void)
{
     lockfile_remove(lockfile_fd);
     lockfile_fd = -1;
}


/*
 *  We don't want multiple copies of ourself running: use a lock file
 *  in /tmp/ to prevent multiple copies from running concurrently.
 */
void
ha7netd_exclude_others(void)
{
     int istat;
     pid_t pid;

     /*
      *  Already have a lock file?  If so, then remove it first
      */
     if (lockfile_fd >= 0)
     {
	  lockfile_remove(lockfile_fd);
	  lockfile_fd = -1;
     }

     if ((istat = lockfile_create(&lockfile_fd, &pid)))
     {
	  if (istat == 1 && pid != (pid_t)0)
	       dbglog("There appears to be another ha7netd daemon running "
		      "with a pid of %d as determined from the lock file "
		      "%s", pid, lockfile);
	  else
	       dbglog("Unable to open the lockfile, \"%s\", for reading and "
		      "writing; perhaps there's another ha7netd daemon "
		      "running or the file is incorrectly owned or protected",
		      lockfile);
	  _EXIT(1);
     }
}

#endif
