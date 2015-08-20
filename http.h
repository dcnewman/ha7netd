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
#if !defined(__HTTP_H_)

#define __HTTP_H_ 1

#include "err.h"
#include "debug.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  HTTP Methods as per HTTP/1.1 specification
 */
#define HTTP_UNKNOWN      0
#define HTTP_CONNECT      1
#define HTTP_DELETE       2
#define HTTP_GET          3
#define HTTP_HEAD         4
#define HTTP_OPTIONS      5
#define HTTP_POST         6
#define HTTP_PUT          7
#define HTTP_TRACE        8

/*
 *  http_conn_t
 *  Information about a TCP connection
 */
typedef struct {
     /* sd == (os.h)INVALID_SOCKET => no connection */
#if defined(_WIN32)
     unsigned int   sd; /* Socket descriptor */
#else
     int            sd; /* Socket descriptor */
#endif
     unsigned short port;      /* TCP port                                   */
     unsigned int   recv_tmo;  /* Read timeout in milliseconds               */
     char           host[128]; /* Destination host name                      */
     size_t         hlen;      /* Destination host name length               */
} http_conn_t;


/*
 *  http_info_t
 *  Parsed information from an HTTP Request or Response as per HTTP/1.1
 */
typedef struct {

     int         ver_major;      /* HTTP-Version major version              */
     int         ver_minor;      /* HTTP-Version minor version              */

     char       *req;            /* HTTP Request-Line [NUL term]            */
     size_t      req_len;        /* Length of Request-Line (bytes)          */
     int         method;         /* HTTP Method                             */
     char       *req_uri;        /* HTTP Request-URI [not NUL term]         */
     size_t      req_uri_len;    /* Length of Request-URI (bytes)           */

     char       *sta;            /* HTTP Status-Line [NUL term]             */
     size_t      sta_len;        /* Length of Status-Line (bytes)           */
     int         sta_code;       /* HTTP Status-Code                        */
     char       *reason;         /* HTTP Reason-Phrase [NUL term]           */
     size_t      reason_len;     /* Length of Reason-Phrase (bytes)         */

     char       *hdr;            /* HTTP message-header [NUL term]          */
     size_t      hdr_len;        /* Length of HTTP message-header (bytes)   */
     const char *ctype;          /* HTTP media-type [not NUL term]          */
     size_t      ctype_len;      /* Length of media-type (bytes)            */

     char       *bdy;            /* HTTP message-body [NUL term]            */
     size_t      bdy_len;        /* Length of HTTP message-body (bytes)     */

} http_msg_t;


/*
 *  http_debug_set()
 *  Call this routine to set debug output flags or supply a debug output
 *  procedure or both.
 *
 *    debug_proc_t *proc
 *      Caller supplied debug output procedure.  See the description
 *      of debug_proc_t above for details.  Pass NULL to use the
 *      default, built in procedure which employs vsfprintf(stdout, fmt, ...).
 *      Used for input only.
 *
 *    void *ctx
 *      Pointer to a caller-supplied context to pass to the caller-supplied
 *      debug output procedure, proc.  Pass NULL when supplying a value of
 *      NULL for the proc call argument.  Used for input only.
 *
 *    int flags
 *      Bit mask of HTTP_DEBUG_x values to control the types of debug output
 *      generated.  Specify a value of zero to disable all debug output.
 *      Used for input only.
 */

void http_debug_set(debug_proc_t *proc, void *ctx, int flags);


/*
 *  Initialize the HTTP library
 *  Primarily used to invoke os_sock_init().
 */

int http_lib_init(void);


/*
 *  Release global resources used by the HTTP library
 */

void http_lib_done(void);


/*
 *  Initialize an http_conn_t structure.  Use of this routine is not
 *  required.  It exists to support callers who wish to do a lazy open
 *  of the HTTP connection.  By calling this routine to initialize an
 *  http_conn_t structure, the caller can then use http_isopen() prior
 *  to sending an HTTP request.  If http_isopen() returns 0, then the
 *  caller knows that http_open() needs to be called.
 *
 *  Returns -1 when the connection is currently open and 0 otherwise.
 */

int http_init(http_conn_t *hconn);


/*
 *  Open a connection to the designated host on the designated TCP port.
 *  The default HTTP port, port 80, is used when a value of zero is passed
 *  for the port argument.  The host name may be either a DNS host name or
 *  an IPv4 address in dotted decimal format (a.b.c.d).
 *
 *  The timeout value is used to control read timeouts on the underlying
 *  socket.  The timeout value should be specified in units of seconds.
 *  a value of zero indicates that read requests should wait indefinitely
 *  for the read to complete.
 *
 *  http_init() does not need to be called prior to http_open().
 */

int http_open(http_conn_t *hconn, const char *host, unsigned short port,
  unsigned int timeout);


/*
 *  Close an HTTP session, releasing any associated resources.  If the
 *  underlying socket is still open, it will first be closed.
 */

int http_close(http_conn_t *hconn);


/*
 *  Returns 1 if the socket is currently opened to the remote HTTP server.
 *  Otherwise, 0 is returned.
 */

int http_isopen(http_conn_t *hconn);


/*
 *  Read and parse an HTTP Request from a remote HTTP client.
 *  Call http_dispose() to free up virtual memory associated with
 *  the http_msg_t structure "hmsg".
 */

int http_read_request(http_conn_t *hconn, http_msg_t *hmsg);


/*
 *  Send an HTTP request over the socket/connection represented by
 *  hconn.  The request will have the form
 *
 *     Method SP Request-URI SP "HTTP/1.1" CRLF
 *     "Host" ":" SP host CRLF
 *     CRLF
 *
 *  Where
 *
 *     Method is the HTTP request method and is specified by the "method" call
 *       argument.  If NULL, "GET" is assumed.
 *
 *     Request-URI is the HTTP request URI and is specified by the "uri" call
 *       argument.  If NULL, then "/" is assumed.
 *
 *     host is the host name specified when the hconn structure was initialized
 *       via http_open().
 *
 */

int http_send_request(http_conn_t *hconn, const char *method,
  const char *uri);


/*
 *  Read and parse an HTTP Response from an HTTP server.  Call this
 *  routine after sending a response with http_send_request().
 *  Call http_dispose() to free up virtual memory associated with
 *  the http_msg_t structure "hmsg".
 */

int http_read_response(http_conn_t *hconn, http_msg_t *hmsg);


/*
 *  Dispose of virtual memory associated with an http_msg_t structure
 *  filled out by http_read_response() and http_read_request().
 */

void http_dispose(http_msg_t *hmsg);

#if defined(__cpluplus)
}
#endif

#endif /* __HTTP_H_ */
