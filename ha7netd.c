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
#include <stdio.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#include "debug.h"
#include "err.h"
#include "os.h"
#include "weather.h"
#include "ha7netd.h"

static const char *default_dir    = "./";
static const char *default_config = "ha7netd.conf";

typedef void *(*pthread_startroutine_t)(void *);

void
dbglog(const char *fmt, ...)
{
     va_list ap;

     va_start(ap, fmt);
     ha7netd_dbglog(NULL, ERR_LOG_ERR, fmt, ap);
     va_end(ap);
}


static void
version(FILE *fp, const char *prog)
{
     const char *bn;

     if (!prog)
	  prog = "ha7netd";
     bn = os_basename((char *)prog);
     if (!bn || !(*bn))
	  bn = prog;
     fprintf(fp,
"%s version %d.%d.%d, built " __DATE__ " " __TIME__ "\n"
"%s\n",
	     bn, WEATHER_VERSION_MAJOR, WEATHER_VERSION_MINOR,
	     WEATHER_VERSION_REVISION, WEATHER_COPYRIGHT);
}


static void
usage(FILE *fp, const char *prog)
{
     const char *bn;
     size_t len;
     ha7netd_opt_t dummy1;
     ha7netd_gopt_t dummy2;
     static const char pad[33] = "                                ";

     ha7netd_opt_defaults(&dummy1, &dummy2);
     if (!prog)
	  prog = "ha7netd";
     bn = os_basename((char *)prog);
     if (!bn || !(*bn))
	  bn = prog;
     len = strlen(bn);
     if (len >= sizeof(pad))
	  len = sizeof(pad) - 1;

     fprintf(fp,
"Usage: %s [-d [debug-level]] [-D [debug_level]] [-c config-file]\n"
"       %.*s [-H ha7net-host] [-p port] [-w working-dir] [-v] [-u user]\n"
"\n"
"where:\n"
" -c config-file   - Configuration file (default \"-c %s\")\n"
" -d [debug-level] - Run in the foreground in debug mode\n"
" -D [debug-level] - Run as a daemon process (default \"-D %d\")\n"
" -f               - Run in the foreground but use syslog\n"
" -H ha7net-host   - HA7Net's host name or IP address (default \"-H %s\")\n"
" -p port          - TCP port the HA7Net listens on (default %u)\n"
" -u user          - Username to run as (default \"-u %s\")\n"
" -v               - Write version information and then exit\n"
" -w working-dir   - Working directory (default \"-w %s\")\n",
			 bn, (int)len, pad,
			 default_config ? default_config : "ha7netd.conf",
			 dummy2.debug, dummy1.host, dummy1.port, dummy2.user,
			 default_dir ? default_dir : "./");
}


static int
daemonize(int argc, char **argv, ha7netd_opt_t **ha7net_list,
	  device_loc_t **device_list, device_ignore_t **ignore_list,
	  int *dbg_level)
{
     int bg, daemon_child, dosyslog, i, istat;
     const char *debug, *host, *opt_fname, *port, *user, *wd;
     ha7netd_gopt_t gbl_opts;
     static const char *extra_arg = "\001";

     if (!ha7net_list || !device_list)
     {
	  dbglog("daemonize(%d): Invalid call arguments supplied; "
		 "ha7net_list=%p, device_list=%p",
		 __LINE__, ha7net_list, device_list);
	  return(-1);
     }

     /*
      *  Initialize these now
      */
     *ha7net_list = NULL;
     *device_list = NULL;
     *ignore_list = NULL;

     /*
      *  For the time being, we set daemon_inbg = 0 so that
      *  error messages will go to stderr.  Once we've shutdown
      *  our I/O channels, we set it appropriately.
      */
     ha7netd_dbglog_set(0, 0, 1);

     /*
      *  First walk of our command line arguments is to locate our
      *  working directory and the path to the option file
      */
     bg           = 1;
     dosyslog     = bg;
     daemon_child = 0;
     opt_fname    = default_config;
     wd           = default_dir;
     ha7netd_opt_defaults(NULL, &gbl_opts);

#if defined(__MUST_EXEC)
     /*
      *  See if we are the product of a fork()/exec() sequence
      */
     if (argc > 1 && argv[argc-1][0] == extra_arg[0])
     {
	  daemon_child = 1;
	  argc--;
     }
#endif

     /*
      *  Process our command line arguments
      */
     for (i = 1; i < argc; i++)
     {
	  if (argv[i][0] == '?' || argv[i][0] == 'h' || argv[i][0] == 'H')
	  {
	       usage(stdout, argv[0]);
	       return(-2);
	  }
	  else if (argv[i][0] != '-')
	       goto bad_usage;
	  else switch(argv[i][1])
	  {
	  default :
	       goto bad_usage;
	       
	  case 'c' :
	       if ((i + 1) >= argc)
		    goto bad_usage;
	       opt_fname = argv[++i];
	       break;

	  case 'f' :
	       bg = 0;
	       break;

	  case 'd' :
	  case 'D' :
	       bg = (argv[i][1] == 'D') ? 1 : 0;
	       dosyslog = bg;
	       if ((i + 1) < argc &&
		   ('0' <= argv[i+1][0] && argv[i+1][0] <= '9'))
		    gbl_opts.debug_arg = argv[++i];
	       break;

	  case 'H' :
	       if ((i + 1) >= argc)
		    goto bad_usage;
	       gbl_opts.host_arg = argv[++i];
	       break;

	  case 'p' :
	       if ((i + 1) >= argc)
		    goto bad_usage;
	       gbl_opts.port_arg = argv[++i];
	       break;

	  case 'u' :
	       if ((i + 1) >= argc)
		    goto bad_usage;
	       gbl_opts.user_arg = argv[++i];
	       break;

	  case 'v' :
	       version(stdout, argv[0]);
	       return(0);

	  case 'w' :
	       if ((i + 1) >= argc)
		    goto bad_usage;
	       wd = argv[++i];
	       break;

	  case '?' :
	  case 'h' :
	       usage(stdout, argv[0]);
	       return(-2);
	  }
     }

     /*
      *  Daemonize our process
      */
     if (bg && !daemon_child)
     {
	  if (os_daemonize(argc, argv, extra_arg))
	  {
	       dbglog("daemonize(%d): Unable to daemonize the process; "
		      "errno=%d; %s", __LINE__, errno, strerror(errno));
	       return(-1);
	  }
     }

#if defined(__MUST_EXEC)
     /*
      *  We've done an execve() after the fork().
      *  As such, stderr isn't what it used to be.  So, flip
      *  the switch and cause any further debug output to go
      *  to syslog.
      */
     ha7netd_dbglog_set(daemon_child, 0, 1);
#endif

     /*
      *  Now do the setsid() and other stuff
      */
     os_server_start_1(wd, bg);

     /*
      *  Load the option file
      */
     istat = ha7netd_config_load(ha7net_list, device_list, ignore_list,
				 &gbl_opts, opt_fname);
     if (istat != ERR_OK)
     {
	  dbglog("daemonize(%d): Unable to load our options; "
		 "ha7net_option_load() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  Pass down the syslog facility code
      */
     ha7netd_dbglog_set(0, gbl_opts.facility, 2);

     /*
      *  And the final stage of daemonizing
      */
     if (os_server_start_2(gbl_opts.user, bg))
     {
	  dbglog("daemonize(%d): Unable to daemonize the process; errno=%d; "
		 "%s", __LINE__, errno, strerror(errno));

	  /*
	   *  Error information was output by ha7netd_server_start_2()
	   */
	  return(-1);
     }

#if !defined(__MUST_EXEC)
     /*
      *  Let the debugging system know our foreground/background status
      */
     ha7netd_dbglog_set(dosyslog, 0, 1);
#endif

     /*
      *  Return the debug level
      */
     if (dbg_level)
	  *dbg_level = gbl_opts.debug;

     /*
      *  All done
      */
     return(ERR_OK);

bad_usage:
     usage(stderr, argv[0]);
     return(-1);
}


int
main(int argc, char **argv)
{
     int debug, istat, weather_initialized;
     device_loc_t *device_list;
     ha7netd_opt_t *ha7net_list, *hl;
     device_ignore_t *ignore_list;
     weather_info_t *tinfo;
     pthread_t t_dummy;
     pthread_attr_t t_stack;

     /*
      *  First order of business is to daemonize ourselves
      */
     weather_initialized = 0;
     ha7net_list         = NULL;
     device_list         = NULL;
     ignore_list         = NULL;
     istat = daemonize(argc, argv, &ha7net_list, &device_list, &ignore_list,
		       &debug);
     if (istat == -2)
	  /*
	   *  Invocation was a help request
	   */
	  return(0);
     else if (istat != ERR_OK)
	  /*
	   *  Error of some sort; problem already reported
	   */
	  return(1);
     else if (!ha7net_list)
     {
	  dbglog("ha7netd(%d): Unable to start; insufficient configuration "
		 "information to run", __LINE__);
	  istat = ERR_NO;
	  goto done;
     }

     /*
      *  Instantiate our shutdown handler
      */
     istat = ha7netd_shutdown_create();
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd(%d): Unable to establish a shutdown handler",
		 __LINE__);
	  goto done;
     }

     /*
      *  Make sure that no other copies of ourself are running
      *  This call *will* _exit(1) if there's another ha7netd
      *  daemon already running!
      */
     ha7netd_exclude_others();

     /*
      *  Force the log file open, if it's not opened already
      *  Point here is to do this before we go multi-threaded
      */
     ha7netd_dbglog_open();

     /*
      *  Initialize the weather_ library
      */
     weather_debug_set(ha7netd_dbglog, NULL, debug);
     istat = weather_lib_init();
     if (istat != ERR_OK)
     {
	  dbglog("ha7netd(%d): Unable to initialize the weather_ library; "
		 "weather_lib_init() returned %d; %s",
		 __LINE__, istat, err_strerror(istat));
	  goto done;
     }
     weather_initialized = 1;

     /*
      *  Let the world know that we're alive and kicking
      */
     dbglog("ha7netd(%d): Running", __LINE__);

     /*
      *  Enter the read/record loop
      */

     /*
      *  The threads do not need a lot of stack.  128K is extreme overkill.
      */
     pthread_attr_init(&t_stack);
     pthread_attr_setstacksize(&t_stack, 1024 * 128);

     /*
      *  Run the threads detached
      */
     pthread_attr_setdetachstate(&t_stack, PTHREAD_CREATE_DETACHED);

     /*
      *  Create a thread for each HA7Net to be monitored
      */
     hl = ha7net_list;
     while (hl)
     {
	  size_t len;

	  tinfo = (weather_info_t *)calloc(1, sizeof(weather_info_t));
	  if (!tinfo)
	  {
	       dbglog("ha7netd(%d): Insufficient virtual memory", __LINE__);
	       goto done;
	  }

	  if (ha7net_list->altitude == HA7NETD_NO_ALTITUDE)
	  {
	       tinfo->wsinfo.altitude = WEATHER_NO_ALTITUDE;
	       tinfo->wsinfo.have_altitude = 0;
	  }
	  else
	  {
	       tinfo->wsinfo.altitude = ha7net_list->altitude;
	       tinfo->wsinfo.have_altitude = 1;
	  }

	  len = strlen(ha7net_list->lon);
	  if (len > WS_LEN)
	       len = WS_LEN - 1;
	  if (len)
	       memmove(tinfo->wsinfo.longitude, ha7net_list->lon, len);
	  tinfo->wsinfo.longitude[len] = '\0';

	  len = strlen(ha7net_list->lat);
	  if (len > WS_LEN)
	       len = WS_LEN - 1;
	  if (len)
	       memmove(tinfo->wsinfo.latitude, ha7net_list->lat, len);
	  tinfo->wsinfo.latitude[len] = '\0';

	  tinfo->host         = ha7net_list->host;
	  tinfo->port         = ha7net_list->port;
	  tinfo->timeout      = ha7net_list->tmo;
	  tinfo->max_fails    = ha7net_list->max_fails;
	  tinfo->period       = ha7net_list->period;
	  tinfo->cmd          = ha7net_list->cmd;
	  tinfo->title        = ha7net_list->loc;
	  tinfo->fname_path   = ha7net_list->dpath;
	  tinfo->fname_prefix = ha7net_list->gname;
	  tinfo->linfo        = device_list;
	  tinfo->ilist        = ignore_list;
	  memcpy(tinfo->avg_periods, ha7net_list->periods,
		 NPERS * sizeof(int));

	  /*
	   *  Spin the thread off
	   */
	  istat = pthread_create(&t_dummy, &t_stack,
				 (pthread_startroutine_t)weather_thread,
				 (void *)tinfo);
	  if (istat)
	  {
	       if (istat == EAGAIN)
		    dbglog("ha7netd(%d): Unable to create a thread; "
			   "insufficient system resources", __LINE__);
	       else
		    dbglog("ha7netd(%d): Unable to create a thread; %s",
			   __LINE__, strerror(istat));
	       free(tinfo);
	       goto done;
	  }

	  /*
	   *  Move on to the next HA7net device
	   */
	  hl = hl->next;
     }

     /*
      *  Now wait around indefinitely until we are told to shutdown
      */
     ha7netd_shutdown_wait();

done:
     /*
      *  Allow up to 10 seconds for worker threads to shutdown
      */
     if (weather_initialized)
	  weather_lib_done(10);

     /*
      *  Free memory up: do this before unlocking our lock file
      */
     ha7netd_config_unload(ha7net_list, device_list, ignore_list);

     /*
      *  Report our shutdown
      */
     dbglog("ha7netd(%d): Shutting down", __LINE__);

     /*
      *  Remove our hold on the lock file
      */
     ha7netd_allow_others();

     /*
      *  Shut down log access
      */
     ha7netd_dbglog_close();

     /*
      *  And exit gracefully
      */
     return((istat == ERR_OK) ? 0 : 1);
}
