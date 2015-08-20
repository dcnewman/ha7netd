#include <stdio.h>
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

#if defined(_WIN32)
#include <Winsock2.h>
#else
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#if !defined(__APPLE__) && !defined(__linux__)
#include <sys/select.h>
#endif
#if defined(__USE_SELECT)
#include <sys/select.h>
#else
#include <poll.h>
#endif
#endif

#include "os_socket.h"

static int init_done = 0;

int
os_sock_init(void)
{
     if (init_done)
	  return(ERR_OK);
#if defined(_WIN32)
     {
	  WORD wVersionRequested;
	  WSADATA wsaData;
	  wVersionRequested = MAKEWORD(1, 1);
	  if (WSAStartup(wVersionRequested, &wsaData))
	       return(ERR_NO);
     }
#else
#endif
     init_done = 1;
     return(ERR_OK);
}


int
os_sock_close(SOCKET sd)
{
     return(closesocket(sd));
}


/*
 *  Squirt some bytes down the socket
 */
ssize_t
os_send(SOCKET sd, const char *buf, size_t buflen)
{
     int again;
     ssize_t nbytes, total;

     /*
      *  Sanity checks
      */
     if (sd == INVALID_SOCKET)
     {
	  SET_SOCK_ERRNO(EBADF);
	  return(-1);
     }
     else if (!buf)
     {
	  SET_SOCK_ERRNO(EFAULT);
	  return(-1);
     }

     /*
      *  Now send the data
      */
     total = 0;
     again = 0;
     while (buflen)
     {
	  nbytes = send(sd, buf, buflen, 0);
	  if (nbytes >= buflen)
	       return(total + nbytes);
	  else if (nbytes > 0)
	  {
	       total  += nbytes;
	       buf    += nbytes;
	       buflen -= nbytes;
	  }
	  else if (!(ISTEMPERR(SOCK_ERRNO)) || ++again > 2)
	       return(-1);
     }
     /* Only reached when buflen == 0 on input */
     return(total);
}


ssize_t
os_writev(SOCKET sd, const struct iovec *iov, int iovcnt)
{
#if !defined(__NO_WRITEV)
     int again, iovmax;
     struct iovec iov_saved;
     ssize_t nbytes, total;
     static int iovmax_static = 20;

     again  = 0;
     iovmax = iovmax_static;
     iov_saved.iov_base = 0;

     /*
      *  Sanity checks
      */
     if (sd == INVALID_SOCKET)
     {
	  SET_SOCK_ERRNO(EBADF);
	  return(-1);
     }
     else if (!iov)
     {
	  SET_SOCK_ERRNO(EFAULT);
	  return(-1);
     }

     total = 0;
     again = 0;
     while (iovcnt)
     {
	  /*
	   *  Skip no-op entries
	   */
	  if (!iov[0].iov_base || !iov[0].iov_len)
	  {
	       iov++;
	       iovcnt--;
	       continue;
	  }

	  /*
	   *  Write the data
	   */
     retry:
#if defined(_WIN32)
	  {
	       DWORD dw = (DWORD)-1;
	       WSASend(sd, (WSABUF *)iov, iovcnt <= iovmax ? iovcnt : iovmax,
		       &dw, 0, NULL, NULL);
	       nbytes = (ssize_t)sz;
	  }
#else
	  nbytes = writev(sd, iov, (iovcnt <= iovmax) ? iovcnt : iovmax);
#endif
	  if (nbytes < 0)
	  {
	       /*
		*  Try writting fewer IOVs?
		*/
	       if (SOCK_ERRNO == EINVAL && iovmax > 10)
	       {
		    iovmax >>= 1;
		    iovmax_static = iovmax;
		    goto retry;  /* continue; would also work */
	       }

	       /*
		*  If a temporary error, then try again
		*/
	       if (ISTEMPERR(SOCK_ERRNO) && ++again <= 10)
		    continue;

	       /*
		*  Give up
		*/

	       /*
		*  Restore any saved data
		*/
	       if (iov_saved.iov_base)
		    ((struct iovec *)iov)[0] = iov_saved;

	       /*
		*  And return an error
		*/
	       return(-1);
	  }

	  /*
	   *  See if there's still more to write
	   */
	  total += nbytes;
	  while (nbytes)
	  {
	       if (nbytes < iov[0].iov_len)
	       {
		    /*
		     *  We wrote only some of the first entry
		     */
		    struct iovec *iov2;

		    /*
		     *  Save this first entry
		     */
		    if (!iov_saved.iov_base)
			 iov_saved = iov[0];

		    /*
		     *  Now tweak the first entry: advance the
		     *  data pointer by nbytes and decrement the
		     *  length by nbytes.
		     */

		    /*
		     *  iov is a const pointer: make non const copy
		     */
		    iov2 = (struct iovec *)iov;
		    iov2->iov_base = (char *)iov2->iov_base + nbytes;
		    iov2->iov_len -= nbytes;
		    nbytes = 0;
	       }
	       else
	       {
		    /*
		     *  We wrote all of that IOV entry
		     */

		    /*
		     *  Restore any saved data
		     */
		    if (iov_saved.iov_base)
		    {
			 ((struct iovec *)iov)[0] = iov_saved;
			 iov_saved.iov_base = 0;
		    }

		    /*
		     *  And move on to the next IOV
		     */
		    nbytes -= iov[0].iov_len;
		    iov++;
		    iovcnt--;
	       }
	  }
     }

     /*
      *  All done
      */
     return(total);
#else
     int i;
     ssize_t nbytes, total;

     total = 0;
     for (i = 0; i < iovcnt; i++)
     {
	  if (!iov[i].iov_base || !iov[i].iov_len)
	       continue;
	  nbytes = os_send(sd, iov[i].iov_base, iov[i].iov_len);
	  if (nbytes < 0)
	       return(-1);
	  else
	       total += nbytes;
     }
     return(total);
#endif
}


int
os_get_connected(const char *host, unsigned short port, int *res_errno,
		 SOCKET *sd)
{
     size_t len;
     SOCKET our_sd;
     int res;
     struct sockaddr_in sock;

     /*
      *  Initialize res_errno to indicate that errno should be examined
      */
     if (res_errno)
	  *res_errno = NETDB_INTERNAL;
     if (sd)
	  *sd = INVALID_SOCKET;
     else
	  return(ERR_BADARGS);

     /*
      *  Sanity checks
      */
     if (!host)
	  host = ""; /* Let resolver library generate an error */

     /*
      *  Initialize our sock structure
      */
     memset(&sock, 0, sizeof(sock));
     sock.sin_family = AF_INET;
     sock.sin_port   = htons(port);

     /*
      *  Set our_sd = -1 to indicate that we don't have a connection
      */
     our_sd = INVALID_SOCKET;

     /*
      *  The host name may be a traditional host name (e.g., acme.com)
      *  or a domain literal (e.g., [127.0.0.1]).  We need to determine
      *  which and then act accordingly.
      */
     if (strcspn(host, ".0123456789"))
     {
	  struct hostent *hp;
	  int i;

	  /*
	   *  We appear to have an ordinary host name
	   *  and not a domain literal.  Do the usual
	   *  gethostbyname mumbo jumbo.
	   */
	  hp = gethostbyname(host);
	  if (!hp)
	  {
	       if (res_errno)
		    *res_errno = h_errno;
	       return(ERR_RESOLV);
	  }
	  else if (!hp->h_addr_list[0])
	  {
	       if (res_errno)
		    *res_errno = NO_DATA;
	       return(ERR_RESOLV);
	  }
	  for (i = 0; hp->h_addr_list[i]; i++)
	  {
	       our_sd = socket(AF_INET, SOCK_STREAM, 0);
	       if (our_sd == INVALID_SOCKET)
		    return(ERR_SOCK);
	       memcpy(&sock.sin_addr, hp->h_addr_list[i], hp->h_length);
	       res = connect(our_sd, (struct sockaddr *)&sock, sizeof(sock));
	       if (res >= 0)
		    break;
	       {
		    int save_errno = SOCK_ERRNO;
		    closesocket(our_sd);
		    SET_SOCK_ERRNO(save_errno);
	       }
	       our_sd = INVALID_SOCKET;
	  }
	  /*
	   *  Return an error if we did not get a connection
	   */
	  if (our_sd == INVALID_SOCKET)
	       /*
		*  Error with socket() would have bombed us out earlier
		*  So, the problem must have been with connect
		*/
	       return(ERR_CONNECT);
     }
     else
     {
	  /*
	   *  Just an IP address
	   */
	  sock.sin_addr.s_addr = inet_addr(host);
	  if (sock.sin_addr.s_addr == -1)
	       return(ERR_BADARGS);
	  our_sd = socket(AF_INET, SOCK_STREAM, 0);
	  if (our_sd == INVALID_SOCKET)
	       return(ERR_SOCK);
	  res = connect(our_sd, (struct sockaddr *)&sock, sizeof(sock));
	  if (res < 0)
	  {
	       int save_errno = SOCK_ERRNO;
	       closesocket(our_sd);
	       SET_SOCK_ERRNO(save_errno);
	       return(ERR_CONNECT);
	  }
     }
 
     /*
      *  All done
      */
     if (res_errno)
	  *res_errno = 0;
     *sd = our_sd;
     return(ERR_OK);
}


int

os_sock_timeout(SOCKET sd, unsigned int milliseconds)
{
#if defined(_WIN32)
     int tmo;

     tmo = (int)(0x7fffffff & milliseconds)
     if (!setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tmo, sizeof(int)) &&
	 !setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(int)))
	  return(0);
     else
	  return(-1);
#elif defined(__APPLE__) || defined(__linux__)
     struct timeval tv;

     tv.tv_sec  = milliseconds / 1000;
     tv.tv_usec = (milliseconds - (tv.tv_sec * 1000)) * 1000;

     if (!setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &tv,
		     sizeof(struct timeval)) &&
	 !setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv,
		     sizeof(struct timeval)))
	  return(0);
     else
	  return(-1);
#else  /* including __sun */
     int flags;

     /*
      *  Set the socket to be non-blocking
      */
     if (0 > (flags = fcntl(sd, F_GETFL)))
	  return(-1);
     return ((0 <= fcntl(sd, F_SETFL, flags | O_NONBLOCK)) ? 0 : -1);
#endif
}


static int
os_readwait(SOCKET sd, unsigned int milliseconds)
{
#if defined(__USE_SELECT)
    struct fd_set fd;
    struct timeval tv;

    tv.tv_sec  = milliseconds / 1000;
    tv.tv_usec = (milliseconds - (tv.tv_sec * 1000)) * 1000;
    FD_ZERO(&fd);
    FD_SET(sd, &fd);
    return(select(sd + 1, &fds, 0, &fds, &tv));
#else
    struct pollfd fd;

    fd.fd     = sd;
    fd.events = POLLIN;
    return(poll(&fd, 1, milliseconds));
#endif
}

ssize_t
os_recv(SOCKET sd, void *buf, size_t buflen, int flags,
	unsigned int milliseconds)
{
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
     /*
      *  For these platforms, we use setsockopt() to set
      *  socket read and write timeouts
      */
     ssize_t nread;

retry:
     nread = recv(sd, buf, buflen, flags);
     if (nread < 0)
     {
	  if (ISTEMPERR(SOCK_ERRNO))
	       goto retry;
	  else if (ISWOULDBLOCK(SOCK_ERRNO))
	  {
	       if (os_readwait(sd, milliseconds))
		    goto retry;
	  }
     }
     return(nread);
#else
     /*
      *  For these platforms, we cannot set a socket timeout and
      *  instead have set the socket to be non-blocking.  We thus
      *  use select() and it's timeout capabilities.
      *
      *  Note that using alarm() is not an option as it does not
      *  play well with threads.
      */
     fd_set fds;
     int n;
     struct timeval tmo;

     tmo.tv_sec  = milliseconds / 1000;
     tmo.tv_usec = (milliseconds - (tmo.tv_sec * 1000)) * 1000;
     FD_ZERO(&fds);
     FD_SET(sd, &fds);
     n = select(sd + 1, &fds, NULL, &fds, &tmo);
     if (n <= 0)
     {
	  /*
	   *  Timeout or error
	   */
	  if (n == 0)
	  {
	       SET_SOCK_ERRNO(ETIMEDOUT);
	       return(0);
	  }
	  else
	       return(-1);
     }
     return(recv(sd, buf, buflen, 0));
#endif
}
