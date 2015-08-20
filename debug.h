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
#if !defined(__DEBUG_H__)

#define __DEBUG_H__

#include <stdarg.h>

/*
 *  The following constants are bit masks for use with
 *  http_debug() and specify which types of diagnostic
 *  debug output should be generated.
 */
#define DEBUG_ERRS          0x000001  /* Explanations for error returns   */
#define DEBUG_RECV          0x000002  /* TCP socket reads                 */
#define DEBUG_XMIT          0x000004  /* TCP socket writes                */
#define DEBUG_HA7NET_RECV   0x000008  /* Hex data received                */
#define DEBUG_HA7NET_XMIT   0x000010  /* Hex data transmitted             */
#define DEBUG_VERBOSE       0x000020  /* Verbose HTTP_*_RECV | _XMIT      */
#define DEBUG_TRACE_DAILY   0x000040  /* Call trace details               */
#define DEBUG_TRACE_DEV     0x000080  /* Call trace details               */
#define DEBUG_TRACE_HA7NET  0x000100  /* Call trace details               */
#define DEBUG_TRACE_HTTP    0x000200  /* Call trace details               */
#define DEBUG_TRACE_WEATHER 0x000400  /* Call trace details               */
#define DEBUG_TRACE_XML     0x000800  /* Call trace details               */

#define DEBUG_IO       (DEBUG_XMIT | DEBUG_RECV)

/*
 *  debug_proc_t
 *  Caller-supplied debug output routines have the call syntax
 *
 *    void proc(void *ctx, int reason, const char *fmt, va_list ap)
 *
 *  where
 *
 *    void *ctx
 *      Pointer to a caller-supplied context.  This is the pointer
 *      passed to http_debug() by the caller.
 *
 *    int reason
 *      Type of debug output being generated (e.g., error, informational,
 *      etc.).  Value will be one of ERR_LOG_ERR or ERR_LOG_DEBUG.
 *
 *    const char *fmt
 *      printf() style formatting string.
 *
 *    ...
 *      Variable length argument list of arguments to format with
 *      fmt.
 */
typedef void debug_proc_t(void *ctx, int reason, const char *fmt, va_list ap);

#endif
