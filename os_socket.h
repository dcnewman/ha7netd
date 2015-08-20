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
 *  O/S dependent routines for opening, closing, and reading w/timeouts
 *  over sockets.
 */

#if !defined(__OS_SOCKET_H__)

#define __OS_SOCKET_H__

#if defined(_WIN32)
#include <Winsock2.h>
#else
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "err.h"
#include "debug.h"
#include "os.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_WIN32)

/*
 *  It's easier to map the set-in-stone Unix conventions over to the
 *  slightly more fluid (and less worthy of our trust) Windows conventions
 */

typedef int SOCKET;
#define ioctlsocket ioctl
#define closesocket close
#define INVALID_SOCKET ((SOCKET)-1)
#define SOCKET_ERROR   -1

#endif

/*
 *  For os_writev()
 */
#if !defined(MAXIOV)
#if defined(IOV_MAX)
#define MAXIOV IOV_MAX
#else
#define MAXIOV 32  /* Conservative value */
#endif
#endif


/*
 *  Initialize the socket library: res_init() on Unix and WSAStartup() on
 *  Windows.
 */

int os_sock_init(void);


/*
 *  Write data to a socket.  os_sock_timeout() setting applied on
 *  Windows, OS X, and Linux.
 */

ssize_t os_send(SOCKET sd, const char *buf, size_t buflen);
ssize_t os_writev(SOCKET sd, const struct iovec *iov, int iovcnt);


/*
 *  Read data from a socket with a read timeout (in milliseconds).
 *  Timeout applied on all platforms via either select(), poll(),
 *  or setsockopt().
 */

ssize_t os_recv(SOCKET sd, void *buf, size_t buflen, int flags,
  unsigned int milliseconds);


/*
 *  Open a TCP connection to the specified host and TCP port. The hostname
 *  may be either a DNS host name (e.g., ha7net-1.sample.com) or an IPv4
 *  address in dotted decimal format (e.g., 192.168.0.250).
 */

int os_get_connected(const char *host, unsigned short port, int *res_errno,
  SOCKET *sd);


/*
 *  Set a read/write timeout on a socket.  On platforms whose setsockopt()
 *  supports SOL_SOCKET + SO_SNDTIMEO + SO_RCVTIMEO, this setting will
 *  stick (Windows, OS X, Linux).  On other platforms, the socket is set to
 *  be non-blocking and a timeout must also be supplied to os_recv() for use
 *  with select() or poll() (e.g., Solaris).
 */

int os_sock_timeout(SOCKET sd, unsigned int milliseconds);


/*
 *  Close the specified socket.
 */

int os_sock_close(SOCKET sd);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__OS_SOCKET_H__) */
