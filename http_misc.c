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
static void
our_debug_ap(void *ctx, int reason, const char *fmt, va_list ap)
{
     (void)ctx;
     (void)reason;

     vfprintf(stderr, fmt, ap);
     fputc('\n', stderr);
     fflush(stderr);
}


static void
debug(const char *fmt, ...)
{
     va_list ap;

     if (do_debug && debug_proc)
     {
	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


static void
tdebug(const char *fmt, ...)
{
     va_list ap;

     if (do_debug && debug_proc)
     {
	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}


static void
trace(const char *fmt, ...)
{
     va_list ap;

     if (do_trace && debug_proc)
     {
	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_DEBUG, fmt, ap);
	  va_end(ap);
     }
}


void
http_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     dbglvl     = flags;
     do_debug   = ((dbglvl & DEBUG_ERRS) && debug_proc) ? 1 : 0;
     do_trace   = ((dbglvl & DEBUG_TRACE_HTTP) && debug_proc) ? 1: 0;    
}


int
http_isopen(http_conn_t *hconn)
{
     if (!hconn)
	  return(0);
     return(hconn->sd == INVALID_SOCKET ? 0 : -1);
}
