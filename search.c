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
#include <stdarg.h>

#include "err.h"
#include "os.h"
#include "debug.h"
#include "ha7net.h"
#include "weather.h"

static int             debug   = DEBUG_ERRS;
static const char     *host    = "192.168.0.250";
static unsigned short  port    = 80;
static unsigned int    timeout = 30;

extern int dev_show(ha7net_t *ctx, device_t *dev, unsigned int flags,
  device_proc_out_t *out, void *out_ctx);

static device_proc_out_t out_proc;
static void version(FILE *fp, const char *prog);
static void usage(FILE *fp, const char *prog);
static void show(device_t *dev, size_t index, int resolve, size_t npad,
  int flag);


static void
out_proc(void *ctx, const char *fmt, ...)
{
     va_list ap;

     if (ctx)
	  *((int *)ctx) = 1;
     fputc('\n', stdout);
     va_start(ap, fmt);
     vfprintf(stdout, fmt, ap);
     va_end(ap);
}


static void
version(FILE *fp, const char *prog)
{
     const char *bn;

     if (prog)
     {
	  bn = os_basename((char *)prog);
	  if (!bn)
	       bn = prog;
     }
     else
	  bn = "search";
     fprintf(fp,
"%s version %d.%d.%d, built " __DATE__ " " __TIME__ "\n"
"%s\n",
	     bn, WEATHER_VERSION_MAJOR, WEATHER_VERSION_MINOR,
	     WEATHER_VERSION_REVISION, WEATHER_COPYRIGHT);
}


static void
usage(FILE *fp, const char *prog)
{
     size_t l;
     const char *bn;
     static const char *pad = "                                    ";

     if (prog)
     {
	  bn = os_basename((char *)prog);
	  if (!bn || !(*bn))
	       bn = prog;
     }
     else
	  bn = "search";
     l = strlen(bn);

     fprintf(fp,
"Usage: %s [-d dbg-level] [-h] [-p port] [-r[r]] [-t s]\n"
"       %.*s [-v] [-V] [host-name]\n"
" hostname     - HA7Net's hostname or IP address (default \"%s\")\n"
" -d dbg-level - Set debug level to the specified value (default \"-d 0x%x\")\n"
" -h, -?       - This usage message\n"
" -p port      - TCP port to connect to (default \"-p %u\")\n"
" -r           - Resolve the relationships amongst the 1-Wire devices\n"
" -rr          - -r and read measurements from each 1-Wire device\n"
" -t seconds   - Read timeout in seconds (default \"-t %u\")\n"
" -v           - Write version information and then exit\n",
			 bn, (l <= 20) ? (int)l : 20, pad,
			 host ? host : "(none)", debug, port, timeout);
}


#define CHECK(a,b) \
     { if (ERR_OK != (istat = (b))) \
	  { fprintf(stderr, "Error: %s() returned %d; %s\n", (a), istat, \
		    err_strerror(istat)); goto done; } \
     }

static void
show(device_t *dev, size_t index, int resolve, size_t npad, int flag)
{
     const char *state1, *state2;
     static const char *pad = "      ";

     if (!dev)
	  return;

     if (npad >= sizeof(pad))
	  npad = sizeof(pad) - 1;

     if (resolve)
     {
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE))
	       state1 = "; ignored";
	  else if (dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       state1 = "; init'd";
	  else
	       state1 = "; unknown";

	  if (dev_flag_test(dev, DEV_FLAGS_ISSUB))
	       state2 = "; subdev";
	  else
	       state2 = "";
     }
     else
     {
	  state1 = state2 = "";
     }

     if (!flag)
		 fprintf(stdout, "%zu.%.*s %s", index, (int)npad, pad, dev_romid(dev));
     fprintf(stdout, ": %s (0x%02x%s%s)\n",
	     dev_strfcode(dev_fcode(dev)), dev_fcode(dev), state1, state2);
}


int
main(int argc, const char *argv[])
{
     int close, istat, ival, read, resolve, unload, verbose;
     device_t *dev, *devices;
     ha7net_t ha7net;
     size_t i, j, k, n, ndevices;
     char *ptr;
     unsigned long ulong;

     devices = NULL;
     close   = 0;
     unload  = 0;
     read    = 0;
     resolve = 0;
     verbose = 0;

     for (i = 1; i < argc; i++)
     {
	  switch(argv[i][0])
	  {
	  default :
	       host = argv[i];
	       break;

	  case '?' :
	       usage(stdout, argv[0]);
	       return(0);

	  case '-' :
	       switch(argv[i][1])
	       {
	       case 'd' :
		    if (++i >= argc)
		    {
			 usage(stderr, argv[0]);
			 return(1);
		    }
		    ptr = NULL;
		    ival = strtol(argv[i], &ptr, 0);
		    if (!ptr || ptr == argv[i])
		    {
			 fprintf(stderr, "Unable to convert \"%s\" to a "
				 "numeric value\n", argv[i] ? argv[i] : "");
			 return(1);
		    }
		    debug = ival;
		    break;

	       case 'p' :
		    if (++i >= argc)
		    {
			 usage(stderr, argv[0]);
			 return(1);
		    }
		    ptr = NULL;
		    ulong = strtoul(argv[i], &ptr, 0);
		    if (!ptr || ptr == argv[i] || ulong != (0xffff & ulong))
		    {
			 fprintf(stderr, "Unable to convert \"%s\" to an "
				 "TCP port number in the range [1,%u]\n",
				 argv[i] ? argv[i] : "", 0xffff);
			 return(1);
		    }
		    port = (unsigned short)(0xffff & ulong);
		    break;

	       case 'h' :
	       case '?' :
		    usage(stdout, argv[0]);
		    return(0);

	       case 'r' :
		    resolve = 1;
		    if (argv[i][2] == 'r')
			 read = 1;
		    break;

	       case 't' :
		    if (++i >= argc)
		    {
			 usage(stderr, argv[0]);
			 return(1);
		    }
		    ptr = NULL;
		    ulong = strtoul(argv[i], &ptr, 0);
		    if (!ptr || ptr == argv[i])
		    {
			 fprintf(stderr, "Unable to convert \"%s\" to a "
				 "numeric value\n", argv[i] ? argv[i] : "");
			 return(1);
		    }
		    timeout = (unsigned int)ulong;
		    break;

	       case 'v' :
		    version(stdout, argv[0]);
		    return(0);

	       case 'V' :
		    verbose = 1;
		    resolve = 1;
		    break;

	       default :
		    usage(stderr, argv[0]);
		    return(1);
	       }
	  }
     }

     ha7net_debug_set(0, 0, debug);
     dev_debug_set(0, 0, debug);
     ha7net_lib_init();

     /*
      *  Load the device drivers if asked to resolve inter-relationships
      */
     if (resolve)
     {
	  CHECK("dev_lib_init", dev_lib_init());
	  unload = 1;
     }

     CHECK("ha7net_open", ha7net_open(&ha7net, host, port ? port : 80,
				      timeout * 1000, 0));
     close = 1;
     CHECK("ha7net_search",
	   ha7net_search(&ha7net, &devices, &ndevices, 0, 0,
			 resolve ? 0 : HA7NET_FLAGS_RELEASE));

     /*
      *  Resolve relationships amongst the individual 1-Wire devices
      */
     if (resolve)
     {
	  CHECK("dev_list_init", dev_list_init(&ha7net, devices));
	  ha7net_releaselock(&ha7net);
     }

     /*
      *  Now show what we've found
      */
     n = 0;
     for (i = 0; i < ndevices; i++)
     {
	  dev = &devices[i];
	  if (verbose)
	  {
	       n++;
	       fprintf(stdout,
		       "--------------------------------------------------\n"
		       "%zu. %s",
		       n, dev_romid(dev));
	       istat = 0;
	       dev_show(&ha7net, dev, 0, out_proc, (void *)&istat);
	       if (!istat)
		    show(dev, 0, 0, 0, 1);
	  }
	  else
	  {
	       if (!resolve)
		    show(dev, ++n, resolve, 0, 0);
	       else if (dev_flag_test(dev, DEV_FLAGS_ISSUB))
		    continue;
	       else
	       {
		    int first = 0;
		    device_t *subdevs;

		    subdevs = dev_group_get(dev);
		    if (!subdevs)
			 subdevs = dev;
		    while (subdevs)
		    {
			 show(subdevs, ++n, resolve, first, 0);
			 subdevs = dev_group_next(subdevs);
			 first = 2;
		    }
	       }
	  }
     }

     if (verbose && ndevices)
	  fprintf(stdout,
		  "--------------------------------------------------\n");

     if (!read)
	  goto done;

     /*
      *  Read each device
      */
     for (i = 0; i < ndevices; i++)
     {
	  /*
	   *  Ignore devices which should not be probed
	   */
	  dev = &devices[i];
	  if (dev_flag_test(dev, DEV_FLAGS_IGNORE | DEV_FLAGS_ISSUB) ||
	      !dev_flag_test(dev, DEV_FLAGS_INITIALIZED))
	       continue;
	  fprintf(stdout, "%s: ", dev_romid(dev));
	  istat = dev_read(&ha7net, dev, 0);
	  if (istat == ERR_OK)
	  {
	       k = 0;
	       for (j = 0; j < NVALS; j++)
	       {
		    if (!dev->data.fld_used[j])
			 continue;
		    if (k)
			 fputc(',', stdout);
		    fprintf(stdout, " %s = ",
			    dev_dtypestr(dev->data.fld_dtype[j]));
		    fprintf(stdout, 
			   dev->data.fld_format[j] ? dev->data.fld_format[j] :
			   "%f", dev->data.val[j][dev->data.n_current]);
		    fprintf(stdout, " %s",
			    dev_unitstr(dev->data.fld_units[j]));
		    k = 1;
	       }
	       fputc('\n', stdout);
	  }
	  else
	       fprintf(stdout, "unable to read; %s\n", err_strerror(istat));
     }

done:
     if (unload)
     {
	  if (devices)
	       dev_list_done(&ha7net, devices);
	  dev_lib_done();
     }
     if (devices)
	  ha7net_search_free(devices);
     if (close)
	  ha7net_done(&ha7net, HA7NET_FLAGS_POWERDOWN);

     return(istat == ERR_OK ? 0 : 1);
}
