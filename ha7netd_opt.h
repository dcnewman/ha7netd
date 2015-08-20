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
#if !defined(__HA7NETD_OPT_H__)

#define __HA7NETD_OPT_H__

#include "opt.h"
#include "device.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(OPT_VAL_LEN)
#define MAX_OPT_LEN OPT_VAL_LEN
#else
#define MAX_OPT_LEN 256
#endif

/*
 *  General options for ha7netd itself.  These options do not live
 *  in an option group: they are global options in the configuration file.
 */
typedef struct {
     const char *debug_arg;
     const char *host_arg;
     const char *port_arg;
     int         debug;
     char        facility[32];
     char        user[32];
     const char *user_arg;
} ha7netd_gopt_t;


/*
 *  Options specific to a [ha7net] option group
 */
typedef struct ha7netd_opt_s {
     struct ha7netd_opt_s *next;
     int                   altitude;     /* Altitude (meters)               */
     int                   period;       /* Interval between samples [secs] */
     int                   max_fails;    /* Max. consecutive failures       */
     unsigned short        port;         /* HA7Net TCP port number          */
     unsigned int          tmo;          /* I/O timeout, milliseconds       */
     device_period_array_t periods;      /* Parsed averaging perionds       */
     char           avgs[MAX_OPT_LEN];   /* Averaging periods               */
     char           dpath[MAX_OPT_LEN];  /* Directory for data & XML files  */
     char           cmd[MAX_OPT_LEN];    /* XML -> HTML command             */
     char           host[MAX_OPT_LEN];   /* HA7Net host name                */
     char           loc[MAX_OPT_LEN];    /* Main location for HTML titles   */
     char           lat[MAX_OPT_LEN];    /* Latitude                        */
     char           lon[MAX_OPT_LEN];    /* Longitude                       */
     char           gname[MAX_OPT_LEN];  /* Group name or host name         */
} ha7netd_opt_t;


/*
 *  Options relevant to a [device] option group
 */
typedef struct {
     device_period_array_t periods;      /* Parsed averaging perionds       */
     unsigned int   flags;               /* Device flags (e.g., outdoors)   */
     float          gain;                /* Correction gain                 */
     float          offset;              /* Correction offset               */
     char           avgs[MAX_OPT_LEN];   /* Averaging periods               */
     char           loc[MAX_OPT_LEN];    /* Device location/description     */
     char           spec[MAX_OPT_LEN];   /* Device specific data            */
     char           hint[MAX_OPT_LEN];   /* Device driver hint              */
} ha7netd_dopt_t;


void ha7netd_opt_defaults(ha7netd_opt_t *opts, ha7netd_gopt_t *gblopts);

int ha7netd_config_load(ha7netd_opt_t **ha7netd_list, device_loc_t **dev_list,
  device_ignore_t **ign_list, ha7netd_gopt_t *gblopts, const char *fname);
void ha7netd_config_unload(ha7netd_opt_t *ha7netd_list,
  device_loc_t *dev_list, device_ignore_t *ign_list);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__HA7NETD_OPT_H__) */
