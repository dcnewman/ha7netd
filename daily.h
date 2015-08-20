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

/*
 *  daily.h
 *
 *  Launch and shutdown the nightly thread which runs once a day
 *  to do daily data management (e.g., reset the daily highs and lows
 *  at midnight).
 */

#if !defined(__DAILY_H__)

#define __DAILY_H__

#include "device.h"  /* for device_t */

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  Initialize the daily_ subroutine library.  Must be called before
 *  calling any other daily_ routines.  Be sure to call daily_lib_done()
 *  when done with the daily_ library.
 */
int daily_lib_init(void);


/*
 *  De-initialize the daily_ subroutine library, releasing any resources
 *  allocated by the library (e.g., mutices).
 */
void daily_lib_done(void);


/*
 *  Create the self-managed, nightly worker thread.  As only one thread is
 *  is needed, this routine should only be called once.  Use
 *  daily_shutdown_begin() and daily_shutdown_finish() to destroy the thread.  
 */
int daily_start(void);


/*
 *  Add a list of devices to those being managed by the nightly thread.
 */
int daily_add_devices(device_t *devices);


/*
 *  Asynchronously begin a graceful shutdown of the nightly thread, and
 *  then subsequently call daily_shutdown_finish() to wait for the thread
 *  to actually shutdown and exit.
 */
void daily_shutdown_begin(void);


/*
 *  Complete a shutdown of the nightly thread, waiting at most a designated
 *  number of seconds.  The minimum wait time is 0.2 seconds.  Passing a
 *  value of zero will therefore result in waiting upwards of 0.2 seconds.
 *  Note, however, that the nightly thread is usually very prompt to shutdown.
 *  Delays are only likely when the shutdown is initiated at 00:00 precisely
 *  and the nightly thread is stuck waiting on a device mutex for one of the
 *  managed devices.
 */
int daily_shutdown_finish(unsigned int seconds);


/*
 *  This routine combines in one call both daily_shutdown_begin() and
 *  daily_shutdown_finish().   Therefore, the call
 *
 *      ires = daily_shutdown(timeout);
 *
 *  is equivalent to the two calls
 *
 *      ires = daily_shutdown_begin();
 *      ires = daily_shutdown_finish(timeout);
 */
int daily_shutdown(unsigned int seconds);

void daily_debug_set(debug_proc_t *proc, void *ctx, int flags);

#if defined(__cplusplus)
}
#endif

#endif
