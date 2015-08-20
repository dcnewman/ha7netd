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
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <resolv.h>
#include <netdb.h>
#include <errno.h>

#include "os_socket.h"
#include "debug.h"
#include "http.h"
#include "http_utils.h"
#include "http_misc.h"

/*
 *  Parsing states for handling an HTTP request
 *
 *  The value 0 is intentionally unused in an attempt to catch
 *  incorrectly initialized parsing states.
 */
#define HTTP_req               1
#define HTTP_sta               2
#define HTTP_fld_nam           3
#define HTTP_fld_val           4
#define HTTP_chunk_len         5
#define HTTP_chunk_skip2eol    6
#define HTTP_bdy_chunk         7
#define HTTP_bdy_chunkeol      8
#define HTTP_bdy_post          9
#define HTTP_bdy_put          10
#define HTTP_end              11
#define HTTP_done             12

typedef struct {

  /* Parsing state */
     int         state;                /* Parsing state                     */
     /* Content length handling                                             */
     int         chunked;              /* Transfer-encoding: chunked?       */
     size_t      clen;                 /* Content-length: or chunk size     */
     /* Header line processing                                              */
     size_t      hdr_len;              /* Approx. header bytes read         */
     size_t      hdr_fld_nam;          /* Offset to current hdr field name  */
     size_t      hdr_fld_val;          /* Offset to current hdr field value */
     /* Next three fields used to handle folding of header lines            */
     size_t      hdr_len_last;         /* Previous hdr_len                  */
     size_t      hdr_fld_nam_last;     /* Previous hdr_fld_nam              */
     size_t      hdr_fld_val_last;     /* Previous hdr_fld_val              */

  /* Read buffers */
     e_string    req;                  /* Request- Status-Line buffer       */
     e_string    header;               /* message-header                    */
     e_string    content;              /* message-content                   */

  /* Parsing results */
     size_t      ctype;                /* Content-type: field value, offset */
     size_t      ctype_len;            /* Content-type: field length        */
     int         ver_major;            /* Major version                     */
     int         ver_minor;            /* Minor version                     */
     int         method;               /* Method                            */
     size_t      req_uri;              /* Request-URI, offset to            */
     size_t      req_uri_len;          /* Request-URI length                */
     int         sta_code;             /* Status-Code                       */
     size_t      reason;               /* Reason-Phrase, offset to          */
     size_t      reason_len;           /* Reason-Phrase length              */

} http_parse_t;

/*
 *  Code living in external files.  Too small to be worth
 *  separate compilation at this point.
 */
#include "http_utils.c"
#include "http_misc.c"

/*
 *  Forward declarations
 */
static int parse_version(const char *str, int *major, int *minor);
static int parse_request_line(http_parse_t *info, const e_string *req);
static int parse_status_line(http_parse_t *info, const e_string *status);
static int parse_line(http_parse_t *pinfo, const char *data, size_t dlen);

/*
 *  For fast isspace() handling
 */
static const int arr_isspace[256] = {
     0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#define ISSPACE(x) (arr_isspace[(unsigned char)x])

/*
 *  Likewise, for fast isdigit() and ('0' <= x && x <= '9') handling
 */
static const int arr_isdigit[256] = {
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#define ISDIGIT(x) (arr_isdigit[(unsigned char)x])

static int
parse_version(const char *str, int *major, int *minor)
{
     char c;
     int i;
     size_t len;

     /*
      *  Return now if provided a bogus string pointer
      */
     if (!str)
     {
	  debug("parse_version(%d): Invalid call arguments supplied; passed "
		"string pointer is NULL", __LINE__);
	  return(-1);
     }

     /*
      *  Set the defaults
      */
     if (major)
	  *major = 0;
     if (minor)
	  *minor = 0;

     /*
      *  "HTTP" "/" 1*DIGIT "." 1*DIGIT
      *
      *  So, the minimum length is 8 bytes
      */
     len = strlen(str);
     if (len < 8 || memcmp(str, "HTTP/", 5))
     {
	  debug("parse_version(%d): Supplied string does not appear to be an "
		"HTTP-Version field; string has a length less than 8 bytes or "
		"does not begin with the 5 bytes \"HTTP/\"; the supplied "
		"string is \"%s\"", __LINE__, str ? str : "");
	  return(-1);
     }

     /*
      *  Move to what should be the first digit of the major version number
      */
     str += 5;

     /*
      *  Attempt to parse the major version number
      */
     i = 0;
     while ((c = *str) && ISDIGIT(c))
     {
	  str++;
	  i = i * 10 + c - '0';
     }
     if (*str++ == '.' && i > 0)
     {
	  if (major)
	       *major = i;

	  /*
	   *  Handle the minor version
	   */
	  i = 0;
	  while ((c = *str) && ISDIGIT(c))
	  {
	       str++;
	       i = i * 10 + c - '0';
	  }
	  if (minor)
	       *minor = i;
	  return(0);
     }
     else
     {
	  debug("parse_version(%d): Supplied string does not appear to be an "
		"HTTP-Version field; the supplied string is \"%s\"",
		__LINE__, str ? str : "(null)");
	  return(-1);
     }
}


static int
parse_request_line(http_parse_t *info, const e_string *req)
{
     size_t offset;
     const char *ptr;

     if (!info || !req)
     {
	  debug("parse_request_line(%d): Invalid call arguments supplied; "
		"info=%p, req=%p; both must be non-zero", __LINE__, info, req);
	  return(-1);
     }

     /*
      *  Set up shop
      */
     info->ver_major   = 0;
     info->ver_minor   = 0;
     info->method      = HTTP_UNKNOWN;
     info->req_uri     = 0;
     info->req_uri_len = 0;

     /*
      *  Request-Line = Method SP Request-URI SP HTTP-Version CRLF
      */
     switch(info->req.data[0])
     {
     case 'O' :
	  if (!estrncmp(req, "OPTIONS ", 8))
	  {
	       info->method = HTTP_OPTIONS;
	       offset = 8;
	  }
	  break;

     case 'G' :
	  if (!estrncmp(req, "GET ", 4))
	  {
	       info->method = HTTP_GET;
	       offset = 4;
	  }
	  break;

     case 'H' :
	  if (!estrncmp(req, "HEAD ", 5))
	  {
	       info->method = HTTP_HEAD;
	       offset = 5;
	  }
	  break;

     case 'P' :
	  if (!estrncmp(req, "POST ", 5))
	  {
	       info->method = HTTP_POST;
	       offset = 5;
	  }
	  else if (!estrncmp(req, "PUT ", 4))
	  {
	       info->method = HTTP_PUT;
	       offset = 4;
	  }
	  break;

     case 'D' :
	  if (!estrncmp(req, "DELETE ", 7))
	  {
	       info->method = HTTP_DELETE;
	       offset = 7;
	  }
	  break;

     case 'T' :
	  if (!estrncmp(req, "TRACE ", 6))
	  {
	       info->method = HTTP_TRACE;
	       offset = 6;
	  }
	  break;

     case 'C' :
	  if (!estrncmp(req, "CONNECT ", 8))
	  {
	       info->method = HTTP_CONNECT;
	       offset = 8;
	  }
	  break;

     default :
	  break;
     }

     /*
      *  Locate the end of the unknown method
      */
     if (!offset)
     {
	  ptr = req->data;
	  while (*ptr && (*ptr == ' ' || *ptr == '\t'))
	       ptr++;
	  if (!ptr[0] || !ptr[1])
	       /*
		*  We've prematurely hit the end of the line
		*/
	       goto bad_eol;
     }
     else
	  ptr = req->data + offset;

     /*
      *  Locate the offset to the Request-URI
      */
     ptr = req->data + offset;
     while (*ptr && (*ptr == ' ' || *ptr == '\t'))
	  ptr++;
     if (!ptr[0])
	  /*
	   *  We've prematurely hit the end of the line
	   */
	  goto bad_eol;

     /*
      *  Compute the offset
      */
     info->req_uri = ptr - req->data;

     /*
      *  Move to the start of the LWSP between the Request-URI and HTTP-Version
      */
     while (*ptr && (*ptr != ' ' && *ptr != '\t'))
	  ptr++;
     info->req_uri_len = (ptr - req->data) - info->req_uri;

     /*
      *  Now skip over the LWSP
      */
     while(*ptr && (*ptr == ' ' || *ptr == '\t'))
	  ptr++;
     if (!ptr[0] || !ptr[1])
	  /*
	   *  Premature EOL
	   */
	  goto bad_eol;

     /*
      *  Determine the HTTP version
      */
     return(parse_version(ptr + 1,
			  &info->ver_major,
			  &info->ver_minor));

bad_eol:
     /*
      *  We've prematurely hit the end of the line
      */
     debug("parse_request_line(%d): Unable to parse the supplied HTTP "
	   "Request-Line; premature end-of-line encountered; supplied "
	   "Request-Line is \"%.*s\"",
	   __LINE__, req->data ? req->len : 0, req->data ? req->data : "");
     return(-1);
}


static int
parse_status_line(http_parse_t *info, const e_string *status)
{
     char c;
     int i;
     const char *ptr;

     if (!info || !status)
     {
	  debug("parse_status_line(%d): Invalid call arguments supplied; "
		"info=%p, status=%p; both must be non-zero",
		__LINE__, info, status);
	  return(-1);
     }

     /*
      *  Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
      */
     info->ver_major  = 0;
     info->ver_minor  = 0;
     info->sta_code   = 0;
     info->reason     = 0;
     info->reason_len = 0;

     /*
      *  Parse the HTTP-Version field
      */
     if (parse_version(status->data,
		       &info->ver_major,
		       &info->ver_minor))
     {
	  /*
	   *  Give up now if we cannot parse the version field
	   */
	  debug("parse_status_line(%d): Unable to parse the supplied "
		"Status-Line; cannot parse the HTTP-Version field; supplied "
		"Request-Line is \"%.*s\"",
		__LINE__, status->data ? status->len : 0,
		status->data ? status->data : "");
	  return(-1);
     }

     /*
      *  Look for the start of the status code
      */
     ptr = status->data + 8;  /* HTTP/x.x is at least 8 bytes long */
     while (*ptr && (*ptr != ' ' && *ptr != '\t'))
	  ptr++;
     if (!ptr[0] || !ptr[1])
	  /*
	   *  We've prematurely hit the end of the line!
	   */
	  goto bad_eol;

     /*
      *  Now move past the SP separator between the
      *  HTTP-Version and Status-Code
      */
     while (*ptr && (*ptr == ' ' || *ptr == '\t'))
	  ptr++;
     if (!ptr[0] || !ptr[1])
	  /*
	   *  We've prematurely hit the end of the line!
	   */
	  goto bad_eol;

     i = 0;
     while ((c = *ptr) && ISDIGIT(c))
     {
	  ptr++;
	  i = i * 10 + c - '0';
     }
     if (i < 100 || i > 599)
	  /*
	   *  Unable to parse the line
	   */
	  goto bad_eol;
     info->sta_code = i;

     /*
      *  Advance to the start of the reason phrase
      *
      *  There should only be one SP separating the Status-Code from the
      *  Reason-Phrase
      */
     while (*ptr && (*ptr == ' ' || *ptr == '\t'))
	  ptr++;
     info->reason     = ptr - status->data;
     info->reason_len = strlen(ptr);

     /*
      *  All done
      */
     return(0);

bad_eol:
     /*
      *  We've prematurely hit the end of the line
      */
     debug("parse_status_line(%d): Unable to parse the supplied HTTP "
	   "Status-Line; premature end-of-line encountered; supplied "
	   "Status-Line is \"%.*s\"",
	   __LINE__, status->data ? status->len : 0,
	   status->data ? status->data : "");
     return(-1);
}


static int
parse_line(http_parse_t *pinfo, const char *data, size_t dlen)
{
     char c;
     const char *data_end, *dptr;
     int istat;

     /*
      *  Sanity check
      */
     if (!pinfo)
     {
	  debug("parse_line(%d): Invalid call arguments supplied; pinfo=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }

     /*
      *  Return now if there's no work to do
      */
     if (!data || !dlen)
	  goto done;

     /*
      *  Saves doing a strlen(data) when we want to determine
      *  how much of data is left.
      */
     data_end = data + dlen;

     /*
      *  Ensure enough storage for this data
      */
     if (ensure(&pinfo->req,     dlen,  2048) ||
	 ensure(&pinfo->header,  dlen,  2048) ||
	 ensure(&pinfo->content, dlen, 10240))
	  goto no_mem;

     /*
      *  Now parse the data
      */
     while ((c = *data))
     {
	  switch (pinfo->state)
	  {

	  default :
	       /*
		*  Bogus value for pinfo->state
		*/
	       debug("parse_line(%d): Invalid parser state supplied; "
		     "pinfo->state=%d", __LINE__, pinfo->state);
	       return(ERR_BADARGS);

	  /*
	   *  Extract the HTTP request or status from the first line
	   */
	  case HTTP_req :
	  case HTTP_sta :
	       switch (c)
	       {
	       case ' '  :
	       case '\t' :
		    /*
		     *  LWSP
		     */
		    if (!pinfo->req.len)
			 /* Ignore leading LWSP */
			 break;
		    else if (ISSPACE(pinfo->req.data[pinfo->req.len-1]))
			 /* Remove redundant LWSP */
			 break;
		    else
			 ECHARCAT(pinfo->req, ' ');
		    break;

	       case '\r' :
		    /*
		     *  Toss the CR
		     */
		    break;

	       case '\n' :
		    /*
		     *  End of the HTTP request
		     */
		    if (!pinfo->req.len)
			 /* Hmmm.... Bogus */
			 break;

		    /*
		     *  NUL terminate the string
		     */
		    ECHARCAT(pinfo->req, '\0');

		    /*
		     *  Parse the line
		     */
		    if (pinfo->state == HTTP_req)
			 parse_request_line(pinfo, &pinfo->req);
		    else
			 parse_status_line(pinfo, &pinfo->req);

		    /*
		     *  And move on to the HTTP header
		     */
		    pinfo->hdr_len          = 0;
		    pinfo->header.len       = 0;
		    pinfo->hdr_fld_nam      = 0;
		    pinfo->hdr_fld_val      = 0;
		    pinfo->hdr_fld_nam_last = 0;
		    pinfo->hdr_fld_val_last = 0;
		    pinfo->state            = HTTP_fld_nam;
		    break;

	       default   :
		    /*
		     *  Consume everything up to the first LWSP or NUL
		     */
		    dptr = data;
		    while ((c = *(++data)) && !ISSPACE(c))
			 ;
		    dlen = data - dptr;
		    memmove(pinfo->req.data + pinfo->req.len, dptr, dlen);
		    pinfo->req.len += dlen;
		    goto end_while;
	       }
	       break;

	  /*
	   *  HTTP field name
	   */
	  case HTTP_fld_nam :
	       switch (c)
	       {
	       case '\0' :
		    break;

	       case ' ' :
	       case '\t' :
		    /*
		     *  Folding?
		     */
		    if (!pinfo->hdr_len)
		    {
			 /*
			  *  Folded header line
			  */
			 if (pinfo->header.len >= 2 &&
			     pinfo->header.data[pinfo->header.len-1] == '\n' &&
			     pinfo->header.data[pinfo->header.len-2] == '\r')
			 {
			      /*
			       *  Replace the previous CRLF pair with a SP
			       */
			      pinfo->header.data[pinfo->header.len-2] = ' ';
			      pinfo->header.len--;
			 }
			 pinfo->hdr_fld_nam = pinfo->hdr_fld_nam_last;
			 pinfo->hdr_fld_val = pinfo->hdr_fld_val_last;
			 pinfo->hdr_len     = pinfo->hdr_len_last - 1;
			 pinfo->state       = HTTP_fld_val;
		    }
		    else
		    {
			 /*
			  *  Invalid characters for a field name:
			  *  let us just silently eat them
			  */
		    }
		    break;

	       case '\r' :
		    /*
		     *  Invalid characters for a field name; eat them
		     */
		    break;

	       case '\n' :
		    if (pinfo->hdr_len)
			 /*
			  *  Invalid characters for a field name; not
			  *  allow to CRLF within the field name either...
			  */
			 break;

		    /*
		     *  pinfo->hdr_len == 0 and thus this is an empty line.
		     *  An empty line signifies the end of the header
		     */

		    /*
		     *  Remove the trailing CRLF from the header
		     */
		    if (pinfo->header.len >= 2 &&
			pinfo->header.data[pinfo->header.len-1] == '\n' &&
			pinfo->header.data[pinfo->header.len-2] == '\r')
			 pinfo->header.len -= 2;

		    /*
		     *  Trailing header after chunked body?
		     */
		    if (pinfo->chunked == 2)
		    {
			 /*
			  *  We just saw the end of the trailing
			  *  header after the last chunk
			  */
			 pinfo->state = HTTP_done;
			 return(ERR_EOM);
		    }

		    /*
		     *  Move on to reading the HTTP content/body
		     */
		    pinfo->content.len = 0;
		    if (pinfo->chunked)
		    {
			 /*
			  *  We're chunking.  Get ready to read the size
			  *  of the first chunk of data.  We don't NUL
			  *  terminate the header just yet.
			  */
			 pinfo->clen  = 0;
			 pinfo->state = HTTP_chunk_len;
		    }
		    else if (!pinfo->clen)
		    {
			 /*
			  *  We've been told zero length content
			  */
			 pinfo->state = HTTP_done;
			 return(ERR_EOM);
		    }
		    else
		    {
			 if (pinfo->method == HTTP_GET)
			      pinfo->state = HTTP_bdy_put;
			 else if (pinfo->method == HTTP_POST)
			      pinfo->state = HTTP_bdy_post;
			 else if (pinfo->method == HTTP_PUT)
			      pinfo->state = HTTP_bdy_put;
			 else
			      pinfo->state = HTTP_bdy_put;
		    }
		    break;

	       case ':'  :
		    ECHARCAT(pinfo->header, ':');
		    pinfo->hdr_len++;
		    pinfo->state       = HTTP_fld_val;
		    pinfo->hdr_fld_val = pinfo->header.len;
		    break;

	       default   :
		    dptr = data;
		    do
		    {
			 ECHARCAT(pinfo->header, tolower(c));
		    }
		    while ((c = *(++data)) && !ISSPACE(c) && c != ':');
		    pinfo->hdr_len += data - dptr;
		    goto end_while;
	       }
	       break;

	  /*
	   *  Handle an HTTP header field value (which may be folded)
	   */
	  case HTTP_fld_val :
	       switch (c)
	       {
	       case '\0' :
		    break;

	       case '\n' :
	       {
		    int ilen;
		    
		    /*
		     *  End of this header line, start of the next
		     */
		    ECHARCAT(pinfo->header, '\n');

		    /*
		     *  Was this a header line of interest?
		     */
		    ilen = (pinfo->hdr_fld_val - pinfo->hdr_fld_nam) - 1;
		    if (ilen == 17 &&
			!memcmp("transfer-encoding",
				pinfo->header.data + pinfo->hdr_fld_nam, 17))
		    {
			 dptr = pinfo->header.data + pinfo->hdr_fld_val;
			 while (ISSPACE(*dptr))
			      /*
			       *  We know there's a terminating NUL
			       *  and thus we won't walk off the end
			       *  of the buffer
			       */
			      dptr++;
			 if (7 < (pinfo->header.data +
				  pinfo->header.len - dptr) &&
			     (*dptr == 'c' || *dptr == 'C') &&
			     !strncasecmp(dptr + 1, "hunked", 6))
			 {
			      pinfo->chunked = 1;
			      pinfo->clen    = 0;
			 }
		    }
		    else if (ilen == 14 &&
			     !memcmp("content-length",
				     pinfo->header.data + pinfo->hdr_fld_nam,
				     14))
		    {
			 /*
			  *  Content-length: nnn
			  */
			 pinfo->clen = strtoul(pinfo->header.data +
					       pinfo->hdr_fld_val, NULL, 10);
		    }
		    else if (ilen == 12 &&
			     !memcmp("content-type",
				     pinfo->header.data + pinfo->hdr_fld_nam,
				     12))
		    {
			 /*
			  *  Content-type: a/b
			  */
			 int pos;

			 /*
			  *  We store an offset since the actual
			  *  pointer value may change if/when we
			  *  handle any secondary header after the
			  *  end of chunked data.
			  */
			 pinfo->ctype = pinfo->hdr_fld_val;
			 while (ISSPACE(pinfo->header.data[pinfo->ctype]))
			      pinfo->ctype++;
			 pos = pinfo->ctype;
			 while(!ISSPACE(pinfo->header.data[pos]) &&
			       pinfo->header.data[pos] != '\r' &&
			       pinfo->header.data[pos] != '\n')
			      pos++;
			 pinfo->ctype_len = (size_t)(pos - pinfo->ctype);
		    }
		    pinfo->hdr_len_last     = pinfo->hdr_len + 1;
		    pinfo->hdr_fld_nam_last = pinfo->hdr_fld_nam;
		    pinfo->hdr_fld_val_last = pinfo->hdr_fld_val;

		    pinfo->hdr_len          = 0;
		    pinfo->hdr_fld_nam      = pinfo->header.len;
		    pinfo->hdr_fld_val      = pinfo->header.len;
		    pinfo->state            = HTTP_fld_nam;
	       }
	       break;

	       default :
		    dptr = data;
		    while ((c = *(++data)) && c != '\n')
			 ;
		    dlen = data - dptr;
		    memmove(pinfo->header.data + pinfo->header.len, dptr,
			    dlen);
		    pinfo->header.len += dlen;
		    pinfo->hdr_len += dlen;
		    goto end_while;
	       }
	       break;

	  /*
	   *  Read the chunk size for the next chunk of content
	   */
	  case HTTP_chunk_len :
	       switch (c)
	       {
	       case '\r' :
		    break;

	       case '\n' :
		    if (pinfo->clen)
			 pinfo->state = HTTP_bdy_chunk;
		    else
		    {
			 /*
			  *  Chunk size of 0 indicates end of content
			  *  Read remaining headers, if any
			  */
			 pinfo->chunked          = 2;
			 pinfo->hdr_len          = 0;
			 pinfo->hdr_fld_nam      = 0;
			 pinfo->hdr_fld_val      = 0;
			 pinfo->hdr_fld_nam_last = 0;
			 pinfo->hdr_fld_val_last = 0;
			 pinfo->state            = HTTP_fld_nam;
		    }
		    break;

	       case '0' :
	       case '1' :
	       case '2' :
	       case '3' :
	       case '4' :
	       case '5' :
	       case '6' :
	       case '7' :
	       case '8' :
	       case '9' :
		    pinfo->clen = pinfo->clen * 16 + (c) - '0';
		    break;

	       case 'a' :
	       case 'b' :
	       case 'c' :
	       case 'd' :
	       case 'e' :
	       case 'f' :
		    pinfo->clen = pinfo->clen * 16 + 10 + (c) - 'a';
		    break;

	       case 'A' :
	       case 'B' :
	       case 'C' :
	       case 'D' :
	       case 'E' :
	       case 'F' :
		    pinfo->clen = pinfo->clen * 16 + 10 + (c) - 'A';
		    break;

	       default :
		    pinfo->state = HTTP_chunk_skip2eol;
		    break;
	       }
	       break;

	  case HTTP_chunk_skip2eol :
	       if (c != '\n')
		    break;
	       if (pinfo->clen)
		    pinfo->state = HTTP_bdy_chunk;
	       else
	       {
		    /*
		     *  Chunk size of 0 indicates end of content
		     *  Read remaining headers, if any
		     */
		    pinfo->chunked          = 2;
		    pinfo->hdr_len          = 0;
		    pinfo->hdr_fld_nam      = 0;
		    pinfo->hdr_fld_val      = 0;
		    pinfo->hdr_fld_nam_last = 0;
		    pinfo->hdr_fld_val_last = 0;
		    pinfo->state            = HTTP_fld_nam;
	       }
	       break;

	  /*
	   *  Actual content; number of expected bytes is info->clen;
	   *  remove line breaks from the content
	   */
	  case HTTP_bdy_chunk :
	       dlen = data_end - data;
	       if (!dlen)
		    break;
	       if (dlen > pinfo->clen)
		    dlen = pinfo->clen;
	       if (ERR_OK != (istat = estrncat(&pinfo->content, data, dlen)))
		    goto no_mem;
	       data += dlen - 1;  /* We do data++ at end of loop */
	       pinfo->clen -= dlen;
	       if (pinfo->clen)
		    break;
	       /*
		*  Deal with trailing CRLF at end of the chunk
		*/
	       pinfo->state = HTTP_bdy_chunkeol;
	       break;

	  /*
	   *  Consume the trailing EOF after a chunk
	   */
	  case HTTP_bdy_chunkeol:
	       if (c == '\n')
	       {
		    pinfo->clen  = 0;
		    pinfo->state = HTTP_chunk_len;
	       }
	       break;

	  /*
	   *  Actual content; number of expected bytes is info->clen;
	   *  remove line breaks from the content
	   */
	  case HTTP_bdy_post :
	       if (c == '\r' || c == '\n')
		    break;
	       ECHARCAT(pinfo->content, c);
	       if (pinfo->content.len < pinfo->clen)
		    break;
/*
 *  Used to switch to HTTP_end and read until LF seen.  However, HTTP/1.1
 *  specification suggests that the type entity-body need not have a CRLF
 *  terminator pair.  In addition, the Coates were seeing a bizarre case
 *  where, left without adequate attention, Internet Exploder would omit the
 *  final CRLF pair in postings.  That is, if you left it idle for a few
 *  minutes and then POSTed, it would omit the CRLF.
 */
	       pinfo->state = HTTP_done;
	       break;

	  /*
	   *  Actual content; number of expected bytes is info->clen;
	   *  preserve line breaks as \n
	   */
	  case HTTP_bdy_put :
/*	       if (c == '\r')
 *		    break;
 */
	       ECHARCAT(pinfo->content, c);
	       if (pinfo->content.len < pinfo->clen)
		    break;
	       pinfo->state = HTTP_done; /* See comment in HTTP_bdy_post */
	       break;

	  /*
	   *  Read the last CRLF pair and then we're done
	   */
	  case HTTP_end :
	       if (c != '\n')
		    break;
	       pinfo->state = HTTP_done;
	       return(ERR_EOM);

	  /*
	   *  We shouldn't see this, but oh well
	   */
	  case HTTP_done :
	       return(ERR_EOM);
          }
	  data++;
     end_while:
	  ;
     }
done:
     return(pinfo->state != HTTP_done ? ERR_OK : ERR_EOM);

no_mem:
     debug("parse_line(%d): Insufficient virtual memory", __LINE__);
     return(istat);
}


void
http_dispose(http_msg_t *hinfo)
{
     if (hinfo)
     {
	  if (hinfo->req)
	       free(hinfo->req);
	  if (hinfo->sta)
	       free(hinfo->sta);
	  if (hinfo->hdr)
	       free(hinfo->hdr);
	  if (hinfo->bdy)
	       free(hinfo->bdy);

	  hinfo->req         = NULL;
	  hinfo->req_len     = 0;
	  hinfo->req_uri     = NULL;
	  hinfo->req_uri_len = 0;

	  hinfo->sta         = NULL;
	  hinfo->sta_len     = 0;
	  hinfo->reason      = NULL;
	  hinfo->reason_len  = 0;

	  hinfo->hdr         = NULL;
	  hinfo->hdr_len     = 0;
	  hinfo->ctype       = NULL;
	  hinfo->ctype_len   = 0;

	  hinfo->bdy         = NULL;
	  hinfo->bdy_len     = 0;
     }
}


/*
 *  Read and parse a request from the HTTP client
 */
static int
http_read(http_conn_t *hconn, http_msg_t *hinfo, int start_state)
{
#define BUFSIZE 8192
     int again, istat, n;
     char buffer[BUFSIZE+1];
     ssize_t buflen;
     fd_set efds, rfds;
     http_parse_t pinfo;

     if (!hconn || !hinfo)
     {
	  debug("http_read(%d): Invalid call arguments supplied; hconn=%p, "
		"hinfo=%p", __LINE__, hconn, hinfo);
	  return(ERR_BADARGS);
     }
     else if (hconn->sd == INVALID_SOCKET)
     {
	  debug("http_read(%d): HTTP connection is not currently opened; "
		"first open or re-open the connection with http_open()",
		__LINE__);
	  return(ERR_NO);
     }

     /*
      *  Initialize our request reading & parsing state information
      */
     istat = ERR_OK;
     memset(hinfo,  0, sizeof(http_msg_t));
     memset(&pinfo, 0, sizeof(http_parse_t));
     pinfo.state = start_state;
     pinfo.clen  = 0x7fffffff;  /* Assume that we read until socket closes */
     again = 0;

read_loop:
     /*
      *  Read data from the client
      */
     buflen = os_recv(hconn->sd, buffer, BUFSIZE, 0, hconn->recv_tmo);
     if (dbglvl & DEBUG_RECV)
     {
	  int save_errno = SOCK_ERRNO;
	  if (dbglvl & DEBUG_VERBOSE)
	  {
	       char tmpbuf[4096];
	       tdebug("http_read(%d): Read %u bytes from socket %d \"%s\"",
		      __LINE__, buflen, hconn->sd,
		      pretty_print(buffer, buflen, tmpbuf, NULL,
				   sizeof(tmpbuf)));
	  }
	  else
	       tdebug("http_read(%d): Read %u bytes from socket %d",
		      __LINE__, buflen, hconn->sd);
	  SET_SOCK_ERRNO(save_errno);
     }
     if (buflen > 0)
     {
	  again = 0;
	  buffer[buflen] = '\0';
	  istat = parse_line(&pinfo, buffer, buflen);
	  if (istat == ERR_OK)
	       goto read_loop;
	  else if (istat != ERR_EOM)
	  {
	       debug("http_read(%d): Error parsing the received HTTP data; "
		     "parse_line() returned %d", __LINE__, istat);
	       goto done_bad;
	  }
     }
     else if ((buflen == 0 || ISTEMPERR(SOCK_ERRNO)) && ++again < 2)
	  goto read_loop;
     else
     {
	  if (dbglvl & (DEBUG_RECV | DEBUG_ERRS))
	  {
	       int save_errno = SOCK_ERRNO;
	       debug("http_read(%d): Error reading from socket %d; recv() "
		     "call failed; errno=%d; %s",
		     __LINE__, hconn->sd, save_errno, strerror(save_errno));
	       SET_SOCK_ERRNO(save_errno);
	  }
	  istat = ERR_READ;
	  goto done_bad;
     }

     /*
      *  Now return the results
      */

     /*
      *  NUL terminate the HTTP header & body
      */
     if (pinfo.header.data)
     {
	  if (ERR_OK != (istat = echarcat(&pinfo.header, '\0')))
	       goto no_mem;
     }
     else
     {
	  if (ERR_OK !=
	      (istat = estrncat(&pinfo.header,
				"content-type: text/html\r\n\0", 26)))
	       goto no_mem;
	  pinfo.ctype     = 14;
	  pinfo.ctype_len =  9;
     }
     if (ERR_OK != (istat = echarcat(&pinfo.content, '\0')))
	  goto no_mem;

     /*
      *  And copy the data over from the parser state to the HTTP info
      */
     hinfo->ver_major = pinfo.ver_major;
     hinfo->ver_minor = pinfo.ver_minor;
     if (start_state == HTTP_req)
     {
	  hinfo->req         = pinfo.req.data;
	  hinfo->req_len     = pinfo.req.len;
	  hinfo->method      = pinfo.method;
	  hinfo->req_uri     = pinfo.req.data + pinfo.req_uri;
	  hinfo->req_uri_len = pinfo.req_uri_len;

	  hinfo->sta         = NULL;
	  hinfo->sta_len     = 0;
	  hinfo->sta_code    = 0;
	  hinfo->reason      = 0;
	  hinfo->reason_len  = 0;
     }
     else
     {
	  hinfo->req         = NULL;
	  hinfo->req_len     = 0;
	  hinfo->method      = 0;
	  hinfo->req_uri     = NULL;
	  hinfo->req_uri_len = 0;

	  hinfo->sta         = pinfo.req.data;
	  hinfo->sta_len     = pinfo.req.len;
	  hinfo->sta_code    = pinfo.sta_code;
	  hinfo->reason      = pinfo.req.data + pinfo.reason;
	  hinfo->reason_len  = pinfo.reason_len;
     }
     pinfo.req.data = NULL;

     if (pinfo.header.data)
     {
	  hinfo->hdr       = pinfo.header.data;
	  hinfo->hdr_len   = pinfo.header.len - 1;
	  hinfo->ctype     = pinfo.header.data + pinfo.ctype;
	  hinfo->ctype_len = pinfo.ctype_len;
	  pinfo.header.data = NULL;
     }
     else
     {
     }

     hinfo->bdy       = pinfo.content.data;
     hinfo->bdy_len   = pinfo.content.len - 1;
     pinfo.content.data = NULL;

     return(ERR_OK);

no_mem:
     debug("http_read(%d): Insufficient virtual memory", __LINE__);

done_bad:
     edispose(&pinfo.req);
     edispose(&pinfo.header);
     edispose(&pinfo.content);
     return(istat);
}


int
http_close(http_conn_t *hconn)
{
     int sd;

     if (!hconn)
	  return(ERR_BADARGS);

     sd = hconn->sd;
     hconn->sd      = INVALID_SOCKET;
     hconn->host[0] = '\0';
     hconn->hlen    =  0;
     hconn->port    =  0;

     if (sd != INVALID_SOCKET)
     {
	  if (!os_sock_close(sd))
	  {

	       if (dbglvl & DEBUG_IO)
		    tdebug("http_close(%d): Closed socket %d", __LINE__, sd);
	       return(ERR_OK);
	  }
	  else
	  {
	       if (dbglvl & (DEBUG_IO | DEBUG_ERRS))
	       {
		    int save_errno = SOCK_ERRNO;
		    debug("http_close(%d): Close failure for socket %d; "
			  "close() returned an error; errno=%d; %s",
			  __LINE__, sd, save_errno, strerror(save_errno));
		    SET_SOCK_ERRNO(save_errno);
	       }
	       return(ERR_CLOSE);
	  }
     }
     else
	  return(ERR_OK);
}


int
http_open(http_conn_t *hconn, const char *host, unsigned short port,
	  unsigned int timeout)
{
     int istat, res_errno, sd;

     if (!hconn || !host || !port)
     {
	  debug("http_open(%d): Invalid call arguments supplied; hconn=%p, "
		"host=%p, port=%u", __LINE__, hconn, host, port);
	  return(ERR_BADARGS);
     }

     memset(hconn, 0, sizeof(http_conn_t));
     hconn->recv_tmo = 1000 * timeout;
     hconn->sd       = INVALID_SOCKET;

     istat = os_get_connected(host, port, &res_errno, &sd);
     if (istat != ERR_OK)
     {
	  if (do_debug)
	  {
	       switch (istat)
	       {
	       default :
		    debug("http_open(%d): Cannot open a connection to the "
			  "remote host \"%s\"; get_connected() returned %d; "
			  "%s", __LINE__, host ? host : "", sd,
			  err_strerror((int)sd));
		    break;

	       case ERR_RESOLV :
		    debug("http_open(%d): Cannot resolve the supplied "
			  "hostname, \"%s\"; h_errno=%d; %s",
			  __LINE__, host ? host : "", res_errno,
			  hstrerror(res_errno));
		    break;

	       case ERR_SOCK :
		    debug("http_open(%d): Cannot obtain a socket descriptor; "
			  "socket() calls are failing; errno=%d; %s",
			  __LINE__, SOCK_ERRNO, STRERROR(SOCK_ERRNO));
		    break;

	       case ERR_CONNECT :
		    debug("http_open(%d): Cannot connect to the remote "
			  "host(s); connect() calls are failing; errno=%d; %s",
			  __LINE__, SOCK_ERRNO, STRERROR(SOCK_ERRNO));
		    break;

	       case ERR_BADARGS :
		    debug("http_open(%d): Supplied host name appears to be an "
			  "IP address which is malformed; supplied host name "
			  "is \"%s\"; inet_addr() is failing",
			  __LINE__, host ? host : "");
		    break;
	       }
	  }
	  return(istat);
     }

     /*
      *  Set read timeout
      */
     if (ERR_OK != os_sock_timeout(sd, hconn->recv_tmo))
     {
	  debug("http_open(%d): Unable to set read and write timeouts on "
		"the TCP connection; setsockopt() returned an error; "
		"errno=%d; %s", SOCK_ERRNO, STRERROR(SOCK_ERRNO));
	  os_sock_close(sd);
	  return(ERR_SOCK);
     }

     hconn->sd   = sd;
     hconn->port = port;
     hconn->hlen = strlen(host);
     if ((4 + hconn->hlen) >= sizeof(hconn->host))
	  hconn->hlen = sizeof(hconn->host) - (1 + 4);
     memcpy(hconn->host, host, hconn->hlen);
     memcpy(&hconn->host[hconn->hlen], "\r\n\r\n\0", 5);
     hconn->hlen += 4;

     if (dbglvl & DEBUG_IO)
	  tdebug("http_open(%d): TCP connection to %s:%u on socket %d",
		 __LINE__, host ? host : "", port, sd);

     return(ERR_OK);
}


int
http_send_request(http_conn_t *hconn, const char *method, const char *uri)
{
     struct iovec iov[5];
     int iovcnt, istat;
     static const char  str1[]   = "GET / HTTP/1.1\r\nHost: ";
     static size_t      str1_len =  22;
     static const char  str2[]   = "GET";
     static size_t      str2_len =   3;
     static const char  str3[]   = "/";
     static size_t      str3_len =   1;
     static const char  str4[]   = " HTTP/1.1\r\nHost: ";
     static size_t      str4_len =  17;
     static const char  str5[]   = " ";
     static size_t      str5_len =   1;

     /*
      *  Sanity checks
      */
     if (!hconn)
     {
	  debug("http_send_request(%d): Invalid call arguments supplied; "
		"hconn=NULL", __LINE__);
	  return(ERR_BADARGS);
     }
     else if (hconn->sd == INVALID_SOCKET)
     {
	  debug("http_send_request(%d): HTTP connection is not currently "
		"opened; first open or re-open the connection with "
		"http_open()", __LINE__);
	  return(ERR_NO);
     }

     if (!method && !uri)
     {
	  /*
	   *  Default case: "GET" SP "/" SP "HTTP/1.1" CRLF "Hostname:" SP
	   */
	  iov[0].iov_base = (char *)str1;
	  iov[0].iov_len  = str1_len;
	  iovcnt = 1;
     }
     else
     {
	  if (method && *method)
	  {
	       iov[0].iov_base = (char *)method;
	       iov[0].iov_len  = strlen(method);
	  }
	  else
	  {
	       /*
		*  No method supplied: assume "GET"
		*/
	       iov[0].iov_base = (char *)str2;
	       iov[0].iov_len  = str2_len;
	  }
	  if (uri && *uri)
	  {
	       /*
		*  First append a SP
		*/
	       iov[1].iov_base = (char *)str5;
	       iov[1].iov_len  = str5_len;

	       /*
		*  And now the URI
		*/
	       iov[2].iov_base = (char *)uri;
	       iov[2].iov_len  = strlen(uri);
	       iovcnt = 3;
	  }
	  else
	  {
	       /*
		*  No URI supplied: assume SP "/"
		*/
	       iov[1].iov_base = (char *)str3;
	       iov[1].iov_len  = str3_len;
	       iovcnt = 2;
	  }

	  /*
	   *  Append SP "HTTP/1.1" CRLF "Hostname:" SP
	   */
	  iov[iovcnt].iov_base = (char *)str4;
	  iov[iovcnt].iov_len  = str4_len;
	  iovcnt++;
     }

     /*
      *  Append the name of the host we're connected to.
      *  When we previously built this host name string,
      *  we append CRLF to it.
      */
     iov[iovcnt].iov_base = (char *)hconn->host;
     iov[iovcnt].iov_len  = hconn->hlen;
     iovcnt++;

     /*
      *  Send the data
      */
     istat = os_writev(hconn->sd, iov, iovcnt);

     /*
      *  And return a result
      */
     return((istat > 0) ? ERR_OK : ERR_WRITE);
}


int
http_read_request(http_conn_t *hconn, http_msg_t *hinfo)
{
     return(http_read(hconn, hinfo, HTTP_req));
}


int
http_read_response(http_conn_t *hconn, http_msg_t *hinfo)
{
     return(http_read(hconn, hinfo, HTTP_sta));
}


void
http_lib_done(void)
{
}


int
http_lib_init(void)
{
     return(os_sock_init());
}


int
http_init(http_conn_t *hconn)
{
     if (!hconn)
     {
	  debug("http_init(%d): Invalid call arguments supplied; hconn=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }
     memset(hconn, 0, sizeof(http_conn_t));
     hconn->sd = INVALID_SOCKET;
     return(ERR_OK);
}


#ifdef __HTTP_TEST

static void
usage(FILE *f, const char *prog)
{
     fprintf(f ? f : stderr, "Usage: %s [-p <port>] <host> [<url>]\n",
	     prog ? prog : "program");
}


int main(int argc, const char *argv[])
{
     http_conn_t hconn;
     http_msg_t hinfo;
     int hinit, i, istat, res_errno;
     const char *host, *url;
     unsigned long port;

     /*
      *  Initializations
      */
     hinit = 0;

     /*
      *  Correct usage?
      */
     host = NULL;
     port = 80;
     url  = NULL;     
     for (i = 1; i < argc; i++)
     {
	  if (argv[i][0] == '-')
	  {
	       if (argv[i][1] == 'p')
	       {
		    char *ptr;

		    if (argv[i][2] || ++i >= argc)
		    {
			 istat = 1;
			 goto show_usage;
		    }
		    ptr = NULL;
		    port = strtoul(argv[i], &ptr, 10);
		    if (!ptr)
		    {
			 fprintf(stderr, "Unable to parse the supplied TCP "
				 "port number, \"%s\"\n",
				 argv[i] ? argv[i] : "(null)");
			 istat = 1;
			 goto done;
		    }
	       }
	       else if (argv[i][1] == '?' || argv[i][1] == 'h')
	       {
		    istat = 0;
		    goto show_usage;
	       }
	  }
	  else if (argv[i][0] == '?')
	  {
	       istat = 0;
	       goto show_usage;
	  }
	  else
	  {
	       if (!host)
		    host = argv[i];
	       else if (!url)
		    url = argv[i];
	       else
	       {
		    istat = 1;
		    goto show_usage;
	       }
	  }
     }
     if (!host)
     {
	  istat = 1;
	  goto show_usage;
     }
     else if (port != (0xffff & port))
     {
	  fprintf(stderr, "The supplied TCP port number, %u, is too large\n"
		  "TCP port numbers must be in the range 0 - 65535 (0 - 0xFFFF"
		  ")\n", port);
	  istat = 1;
	  goto done;
     }

     /*
      *  Enable some debugging
      */
     http_debug(0, 0, DEBUG_IO | DEBUG_ERRS);

     /*
      *  Open a connection on port p
      */
     istat = http_open(&hconn, host, (unsigned short)(port & 0xffff));
     if (istat != ERR_OK)
     {
	  fprintf(stderr, "Unable to connect to the remote host \"%s\"; "
		  "http_open() returned %d; %s\n",
		  host ? host : "", istat, err_strerror(istat));
	  istat = 1;
	  goto done;
     }

     istat = http_send_request(&hconn, "GET", url);
     if (istat != ERR_OK)
     {
	  fprintf(stderr, "Unable to \"GET %s\"\n"
		  "http_send_request() returned %d; %s\n",
		  url ? url : "/", istat, err_strerror(istat));
	  istat = 1;
	  goto done;
     }

     istat = http_read_response(&hconn, &hinfo);
     hinit = 1;
     if (istat == ERR_OK)
     {
	  fprintf(stdout,
"Status-Line  = \"%.*s\" (%d bytes)\n"
"Content-type = \"%.*s\" (%d bytes)\n"
"\n"
"Header (%d bytes)\n"
"------------------\n"
"%.*s\n"
"------------------\n"
"\n"
"Content (%d bytes)\n"
"------------------\n"
"%.*s\r\n"
"------------------\n",
		  hinfo.sta_len,   hinfo.sta,     hinfo.sta_len,
		  hinfo.ctype_len, hinfo.ctype,   hinfo.ctype_len,
		  hinfo.hdr_len,   hinfo.hdr_len, hinfo.hdr,
		  hinfo.bdy_len,   hinfo.bdy_len, hinfo.bdy);
	  istat = 0;
     }
     else
     {
	  fprintf(stderr, "Read error of some sort; http_read_response() "
		  "returnd %d; %s\n", istat, err_strerror(istat));
	  istat = 1;
     }
     goto done;

show_usage:
     usage(istat ? stderr : stdout, argv[0]);

done:
     http_close(&hconn);
     if (hinit)
	  http_dispose(&hinfo);
     return(istat);
}

#endif
