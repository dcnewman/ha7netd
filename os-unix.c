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
#include <time.h>
#define SYSLOG_NAMES
#include <syslog.h>
#undef SYSLOG_NAMES
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <strings.h>
#include <errno.h>
#include <pwd.h>
#if defined(__sun)
#include <signal.h>
#endif

#include "err.h"
#include "os.h"

#if defined(__osf__)
#  define FORK fork
#  define GETPWNAM_R getpwnam_r
#elif defined(__APPLE__)
#  define FORK fork
#  define GETPWNAM_R getpwnam_r
#elif defined(__sun)
#  define FORK fork1
#  define GETPWNAM_R(n,r,b,l,nr) ((getpwnam_r(n,r,b,l) == NULL) ? -1 : -0)
#elif defined(__linux__)
#  define FORK fork
#  define GETPWNAM_R getpwnam_r
#endif

#if !defined(SIABUFSIZ)
#  define SIABUFSIZ 1024
#endif
#define UMASK_BITS S_ISUID | S_ISGID | S_ISVTX | S_IRWXO  /* 07007 */

#if defined(__MUST_EXEC)
extern char **environ;
#endif

/*
 *  Issue a SIG_IGN for SIGCHLD
 */
static int did_ignore = 0;

int
os_sleep(unsigned int milliseconds)
{
     struct timespec tv;

     /*
      *  Note: if milliseconds == 0, we still want to call
      *  nanosleep() as the caller's intent may be to cause
      *  a thread to yield.
      */

     if (milliseconds >= 1000)
     {
	  int s = (int)(milliseconds / 1000);
	  milliseconds = milliseconds - (s * 1000);
	  tv.tv_sec = s;
     }
     else
	  tv.tv_sec = 0;
     tv.tv_nsec = milliseconds * 1000000;
     return(nanosleep(&tv, NULL));
}


int
os_fexists(const char *fname)
{
     if (!fname)
	  return(0);
     return((0 == access(fname, F_OK)) ? 1 : 0);
}


const char *
os_tzone(long *gmt_offset, const char **tzone, char *buf, size_t buflen)
{
     long tm_gmtoff;
     const char *tm_zone;
     struct tm tmbuf;
     time_t t;

     t = time(NULL);
     localtime_r(&t, &tmbuf);

#if defined(__APPLE__)
     tm_gmtoff = tmbuf.tm_gmtoff;
     tm_zone   = tmbuf.tm_zone;
#elif defined(__sun)
     if (tmbuf.tm_isdst > 0)
     {
	  tm_gmtoff = -1 * altzone;
	  tm_zone   = tzname[1];
     }
     else
     {
	  tm_gmtoff = -1 * timezone;
	  tm_zone   = tzname[0];
     }
#else
     {
	  struct tm gtmbuf;

	  gmtime_r(&t, &gtmbuf);
	  tm_gmtoff = (tmbuf.tm_hour - gtmbuf.tm_hour) * 3600 +
	       (tmbuf.tm_min - gtmbuf.tm_min) * 60;
	  if (tmbuf.tm_year < gtmbuf.tm_year)
	       tm_gmtoff -= 24 * 3600;
	  else if (tmbuf.tm_year > gtmbuf.tm_year)
	       tm_gmtoff += 24 * 3600;
	  else if (tmbuf.tm_yday < gtmbuf.tm_yday)
	       tm_gmtoff -= 24 * 3600;
	  else if (tmbuf.tm_yday > gtmbuf.tm_yday)
	       tm_gmtoff += 24 * 3600;
     }
     if (tmbuf.tm_isdst > 0)
	  tm_zone = tzname[1];
     else
	  tm_zone = tzname[0];
#endif

     /*
      *  Return our results
      */
     if (gmt_offset)
	  *gmt_offset = tm_gmtoff;
     if (tzone)
	  *tzone = tm_zone;

     return(tm_zone);
}


static int
os_uinfo(uid_t *uid, gid_t *gid, const char *user)
{
     char buf[SIABUFSIZ];
     int istat;
     struct passwd pwinfo, *pwinfo2;

     if (!user)
     {
	  errno = EINVAL;
	  return(-1);
     }
     else if (!(*user))
     {
	  if (uid)
	       *uid = getuid();
	  if (gid)
	       *gid = getgid();
     }

     istat = GETPWNAM_R(user, &pwinfo, buf, sizeof(buf), &pwinfo2);
     if (istat)
	  return(-1);

     if (uid)
	  *uid = pwinfo.pw_uid;
     if (gid)
	  *gid = pwinfo.pw_gid;
     return(0);
}


int
os_daemonize(int argc, char **argv, const char *extra_arg)
{
     pid_t pid;

     /*
      *  The default is to ignore SIGCHLD signals.  However, unless
      *  this call is *explicitly* made, child processes will become
      *  zombies ("defunct") when they exit.
      */
     if (!did_ignore)
     {
	  did_ignore = 1;
	  signal(SIGCHLD, SIG_IGN);
     }

     if ((pid = FORK()) < 0)
	  return(-1);
     else if (pid)
	  /*
	   *  Parent goes bye-bye
	   */
	  _EXIT(0);

     /*
      *  Daemon child continues
      */
#if !defined(__MUST_EXEC)
     return(0);
#else
     {
	  char **argv2;
	  int i, istat;

	  /*
	   *  Need to do an exec*() call
	   */

	  /*
	   *  Build a new argument list which contains an extra argument
	   */
	  argv2 = (char **)malloc(sizeof(char *) * (argc + 2));
	  if (!argv2)
	       return(-1);

	  /*
	   *  Copy the original argument list
	   */
	  for (i = 0; i < argc; i++)
	       argv2[i] = (char *)argv[i];

	  /*
	   *  Add in the extra argument
	   */
	  argv2[argc] = (char *)extra_arg;

	  /*
	   *  And terminate the list
	   */
	  argv2[argc+1] = NULL;

	  /*
	   *  And exec ourself
	   */
	  istat = execve(argv[0], argv2, environ);
	  if (istat != -1)
	       _EXIT(0);

	  /*
	   *  If we're here, there's been an error-o-la
	   */
	  {
	       int save_errno = errno;
	       free(argv2);
	       errno = save_errno;
	       return(-1);
	  }
     }
#endif
}


void
os_server_start_1(const char *wdir, int bg)
{
     /*
      *  Step 2 of daemonizing: to be done by the daemon child
      *  Note that we don't change the uid and gid until
      *  after we've opened our configuration file.  That way
      *  we can handle the Good Case whereby the daemonized
      *  server runs under a UID and GID which cannot write
      *  to its own configuration file.
      */
     if (bg)
	  setsid();
     umask(07007);
     if (wdir)
	  chdir(wdir);
}


int
os_server_start_2(const char *user, int close_stdfiles)
{
     int istat;
     gid_t gid, old_egid, old_gid;
     uid_t uid, old_euid, old_uid;

     /*
      *  Close all open files but stdout & stderr
      */
     if (close_stdfiles)
     {
	  if (stderr)
	  {
	       fflush(stderr);
	       fclose(stderr);
	  }
	  else
	  {
	       fsync(2);
	       close(2);
	  }
	  if (stdout)
	  {
	       fflush(stdout);
	       fclose(stdout);
	  }
	  else
	  {
	       fsync(1);
	       close(1);
	  }
	  if (stdin)
	       fclose(stdin);
	  else
	       close(0);
	  os_close_files(3, 20);
     }

     /*
      *  Change our uid & gid
      */
     if (user && *user)
     {
	  if (os_uinfo(&uid, &gid, user))
	       return(-1);

	  /*
	   *  In case things fail....
	   */
	  old_euid = geteuid();
	  old_uid  = getuid();
	  old_egid = getegid();
	  old_gid  = getgid();

	  /*
	   *  On some platforms you first have to elevate the effective UID to
	   *  root before changing real & effective UID to a non-root UID.
	   */
	  seteuid(0);

	  /*
	   *  Now do an irreversible change of our UID and GID
	   */
	  if (setgid(gid))
	  {
	       int save_errno = errno;
	       seteuid(old_euid);
	       errno = save_errno;
	       return(-1);
	  }

	  if (setuid(uid))
	  {
	       int save_errno = errno;
	       seteuid(old_euid);
	       setgid(old_gid);
	       setegid(old_egid);
	       errno = save_errno;
	       return(-1);
	  }
     }
     return(0);
}


#if !defined(__APPLE__)
typedef struct _code {
     char *c_name;
     int   c_val;
} CODE;

CODE facilitynames[] = {
     {"auth",     LOG_AUTH},
     {"cron",     LOG_CRON},
     {"daemon",   LOG_DAEMON},
     {"kern",     LOG_KERN},
     {"lpr",      LOG_LPR},
     {"mail",     LOG_MAIL},
     {"news",     LOG_NEWS},
     {"security", LOG_AUTH},
     {"syslog",   LOG_SYSLOG},
     {"user",     LOG_USER},
     {"uucp",     LOG_UUCP},
     {"local0",   LOG_LOCAL0},
     {"local1",   LOG_LOCAL1},
     {"local2",   LOG_LOCAL2},
     {"local3",   LOG_LOCAL3},
     {"local4",   LOG_LOCAL4},
     {"local5",   LOG_LOCAL5},
     {"local6",   LOG_LOCAL6},
     {"local7",   LOG_LOCAL7},
     {NULL,       -1}
};

#endif

int
os_facstr2int(const char *str)
{
     if (!str)
     {
	  errno = EINVAL;
	  return(-1);
     }
     else if (!(*str))
	  return(0);

     if ('0' <= *str && *str <= '9')
     {
	  /*
	   *  Interpret as a number
	   */
	  long lval;
	  char *ptr = NULL;

	  /*
	   *  Assume that the input buffer is NUL terminated
	   */
	  lval = strtol(str, &ptr, 0);
	  if (!ptr || ptr == str)
	  {
	       errno = EINVAL;
	       return(-1);
	  }
	  return((int)lval);
     }
     else
     {
	  /*
	   *  Interpret as a symbolic facility name
	   */
	  int found_it = 0;
	  CODE *names = facilitynames; /* From syslog.h */

	  while (names && names->c_name)
	  {
	       if (!strcasecmp(names->c_name, str))
		    return((int)names->c_val);
	       names++;
	  }
	  return(0);
     }
}


void
os_log_close(void *facility)
{
     (void)facility;
}


void *
os_log_open(const char *facility)
{
     int ifacility = os_facstr2int(facility);

     if (ifacility <= 0)
	  ifacility = LOG_LOCAL3;

     return((void *)(intptr_t)ifacility);
}


void
os_log(void *facility, int reason, const char *fmt, va_list ap)
{
     if (fmt)
     {
	  int prio = (reason == ERR_LOG_ERR) ? LOG_ERR : LOG_DEBUG;
	  vsyslog(prio | (int)facility, fmt, ap);
     }
}


os_pid_t
os_getpid(void)
{
     return((os_pid_t)getpid());
}


os_pid_t
os_spawn_nowait(const char *cmd, os_argv_t *argv, const char *new_env, ...)
{
     int istat;
     size_t len;
     pid_t pid;

     /*
      *  Sanity checks
      */
     if (!argv || !argv->argv[0] || !argv->argv[0][0] || !argv->argc)
     {
	  errno = EFAULT;
	  return((os_pid_t)-1);
     }

     if (!did_ignore)
     {
	  did_ignore = 1;
	  signal(SIGCHLD, SIG_IGN);
     }

     /*
      *  fork()  [fork1() on Solaris]
      */
     pid = FORK();
     if (pid < 0)
	  /*
	   *  Fork failed
	   */
	  return((os_pid_t)-1);
     else if (pid)
	  /*
	   *  We're the happy parent... return to our caller
	   */
	  return((os_pid_t)pid);

     /*
      *  We're the child
      */

     /*
      *  We'll just push the new environment variables into our
      *  copy of the environment
      */
     if (new_env)
     {
	  va_list ap;
	  char buf[4096];
	  size_t len1, len2;
	  const char *nam, *val;

	  va_start(ap, new_env);
	  nam = new_env;
	  while (nam)
	  {
	       val = va_arg(ap, const char *);
	       if (!val)
		    break;
	       len1 = strlen(nam);
	       len2 = strlen(val);
	       if ((len1 + len2 + 2) <= sizeof(buf))
	       {
		    char *p = buf;
		    memcpy(p, nam, len1);
		    p += len1;
		    *p++ = '=';
		    memcpy(p, val, len2+1);
	       }
	       putenv(buf);
	       nam = va_arg(ap, const char *);
	  }
	  va_end(ap);
     }

     /*
      *  Now exec
      */
     execv(argv->argv[0], argv->argv);

     /*
      *  Something bad happened if execution reaches this point
      */
     _EXIT(0);
}
