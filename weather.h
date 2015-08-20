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
 *  OR TO.RT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
#if !defined(__WEATHER_H__)

#define __WEATHER_H__

#include "debug.h"
#include "device.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define WEATHER_VERSION_MAJOR    0
#define WEATHER_VERSION_MINOR    1
#define WEATHER_VERSION_REVISION 0
#define WEATHER_COPYRIGHT "Copyright (c) 2005, mtbaldy.us\nAll Rights Reserved"

/*
 *  No altitude provided when ha7net_t field altitude == WEATHER_NO_ALTITUDE
 */
#define WEATHER_NO_ALTITUDE -(0x7fffffff)

#define WS_LEN 64

typedef struct {
     int    altitude;
     int    have_altitude;
     char   longitude[WS_LEN];
     char   latitude[WS_LEN];
} weather_station_t;

typedef struct {
     const char            *host;
     unsigned short         port;
     unsigned int           timeout;
     size_t                 max_fails;
     int                    have_pcor;
     int                    period;
     int                    first;
     device_period_array_t  avg_periods;
     const char            *cmd;
     const char            *title;
     const char            *fname_path;
     const char            *fname_prefix;
     const device_loc_t    *linfo;
     const device_ignore_t *ilist;
     void                  *sinfo;
     weather_station_t      wsinfo;
} weather_info_t;

void weather_debug_set(debug_proc_t *proc, void *ctx, int flags);
int weather_main(weather_info_t *info);
void weather_thread(void *ctx);
int weather_lib_init(void);
int weather_lib_done(unsigned int seconds);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__WEATHER_H__) */
