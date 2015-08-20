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

#include "err.h"

static const char *errors[] = {
     /*  0 */   "Successful operation (ERR_OK)",
     /*  1 */   "End of the HTTP message reached (ERR_EOM)",
     /*  2 */   "Error returned by a caller-supplied callback procedure"
                   " (ERR_ABORT)",
     /*  3 */   "Invalid call arguments supplied (ERR_BADARGS)",
     /*  4 */   "Cyclic redundancy check failed (ERR_CRC)",
     /*  5 */   "Unable to perform the requested operation (ERR_NO)",
     /*  6 */   "Insufficient virtual memory available (ERR_NOMEM)",
     /*  7 */   "Specified option value is out of range (ERR_RANGE)",
     /*  8 */   "Invalid line in the option file (ERR_SYNTAX)",
     /*  9 */   "Specified option name or value is too long (ERR_TOOLONG)",
     /* 10 */   "Socket close error (ERR_CLOSE)",
     /* 11 */   "Unable to establish a TCP connection (ERR_CONNECT)",
     /* 12 */   "Socket read error (ERR_READ)",
     /* 13 */   "Unable to resolve the host name (ERR_RESOLV)",
     /* 14 */   "Unable to create a socket descriptor (ERR_SOCK)",
     /* 15 */   "Socket write error (ERR_WRITE)",
     /* 16 */   "Unknown error code (?)"
};

const char *
err_strerror(int err)
{
     if (err < ERR_OK || err > ERR_LAST)
	  return(errors[ERR_LAST+1]);
     else
	  return(errors[err]);
}
