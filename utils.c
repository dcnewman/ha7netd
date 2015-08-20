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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include "utils.h"

/*
 *  void Hex2Byte(unsigned char *dst, const char *src, size_t srclen)
 *
 *  Convert a series of hexadecimal digits to raw binary
 */

void
Hex2Byte(unsigned char *dst, const char *src, size_t srclen)
{
     size_t i;
     const unsigned char hex2byte[256] = {
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0,10,11,12,13,14,15, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

     if (!dst || !src || !srclen)
	  return;

     /*
      *  Handle the first byte as a special case so that we can deal
      *  with an odd srclen.  When srclen is odd, we assume a leading '0'
      */
     if ((srclen % 2))
     {
	  *dst++ = (unsigned char)hex2byte[src[0]];
	  src++;
	  i = 1;
     }
     else
	  i = 0;

     srclen >>= 1;
     for (; i < srclen; i++)
     {
	  *dst++ = (unsigned char)(hex2byte[src[0]] * 16 +
				   hex2byte[src[1]]);
	  src += 2;
     }
}

/*
 *  const char *xml_strquote(char const **dst, size_t *dlen, const char *src,
 *                           size_t slen)
 *
 *    Copy the input string src, producing a quoted copy safe for use as
 *    a data string within XML.  Specifically, the output string will have
 *
 *       " replaced with &quot;
 *       & replaced with &amp;
 *       ' replaced with &apos;
 *       < replaced with &lt;
 *       > replaced with &gt;
 *
 *       all unprintable other than space, tab, and line feed are replaced
 *       with
 *
 *              &#x;
 *
 *       where "x" is the ordinal value of that unprintable character
 *       expressed in hexadecimal.
 *
 *    The output string is returned by reference.  The returned reference
 *    MAY be the input string itself and as such will only be NUL terminated
 *    when the input string was itself NUL terminated.  When the returned
 *    reference is NOT the input string, then it is a pointer to virtual
 *    memory.  In this case, the output string will be NUL terminated.
 *    Additionally, the output string must be released when no longer needed
 *    by calling make_quoted_free().
 *
 *    NOTE that NULs appearing in the input string src are treated as data
 *    to be quoted.  NULs appearing in src are not interpreted as terminators.
 *
 *    NOTE that this routine does not produce strings safe for use in XML
 *    comments.  Strings in XML comments must not contain "--".
 *
 *  Call arguments
 *
 *    char const **dst
 *      Optional pointer to a "const char *" to receive the pointer to the
 *      quoted string.  The returned value MAY be the value of the "src" call
 *      argument.  Used for output only.
 *
 *    size_t *dlen
 *      Optional pointer to a size_t to receive the length in bytes of the
 *      quoted output string.  This length does not include any NUL terminator.
 *      Used for output only.
 *
 *    const char *src
 *      Pointer to the input string to quote any NULs in this string are
 *      treated as data.  The length in bytes of this string are supplied
 *      with the slen call argument.  Used for input only.
 *
 *    size_t slen
 *      The length in bytes of the input string src.  Used for input only.
 *
 *  Return values
 *
 *    == NULL   -- Bad call arguments; src == NULL
 *    != NULL   -- Pointer to the quoted output string
 *
 *  Routines called
 *
 *    malloc(), memcpy()
 */

const char *
xml_strquote(char const **dst, size_t *dlen, int *dispose, const char *src,
	     size_t slen)
{
     size_t l;
     char *ptr, *ptr0;
     const char *sptr;
     static const int char_handling[256] = {
	  6,6,6,6,6,6,6,6,6,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	  0,0,1,0,0,0,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,5,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,
	  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6};
     static const char hex[16+1] = "0123456789abcdef";

     /*
      *  Assume no malloc()
      */
     if (dispose)
	  *dispose = 0;

     /*
      *  Sanity check
      */
     if (!src)
     {
	  if (dst)
	       *dst = NULL;
	  return(NULL);
     }

     /*
      *  An empty string requires no quoting
      */
     if (!slen)
     {
	  if (dlen)
	       *dlen = slen;
	  if (dst)
	       *dst = src;
	  return(src);
     }

     /*
      *  See if the string contains any characters which require quoting
      */
     sptr = src;
     for (l = 0; l < slen; l++)
	  if (char_handling[(unsigned char)(*sptr++)])
	       goto needs_quoting;
     /*
      *  Doesn't need quoting
      */
     if (dlen)
	  *dlen = slen;
     if (dst)
	  *dst = src;
     return(src);

needs_quoting:
     /*
      *  We require upwards of l + 6 * (slen - l) bytes
      */
     ptr0 = (char *)malloc(1 + l + 6 * (slen - l));
     if (!ptr0)
     {
	  if (dst)
	       *dst = NULL;
	  return(NULL);
     }

     /*
      *  Copy the first l bytes over as is
      */
     ptr = ptr0;
     if (l)
     {
	  memcpy(ptr, src, l);
	  ptr += l;
	  src += l;
     }

     /*
      *  Now handle the quoting needs of the rest of the string
      */
     for (; l < slen; l++, src++)
     {
	  switch(char_handling[(unsigned char)(*src)])
	  {
	  /*  No quoting */
	  case 0 :
	       *ptr++ = *src;
	       break;

	  /* " -> &quot; */
	  case 1 :
	       *ptr++ = '&';
	       *ptr++ = 'q';
	       *ptr++ = 'u';
	       *ptr++ = 'o';
	       *ptr++ = 't';
	       *ptr++ = ';';
	       break;

	  /* & -> &amp; */
	  case 2 :
	       *ptr++ = '&';
	       *ptr++ = 'a';
	       *ptr++ = 'm';
	       *ptr++ = 'p';
	       *ptr++ = ';';
	       break;

	  /* ' -> &apos; */
	  case 3 :
	       *ptr++ = '&';
	       *ptr++ = 'a';
	       *ptr++ = 'p';
	       *ptr++ = 'o';
	       *ptr++ = 's';
	       *ptr++ = ';';
	       break;

	  /* < -> &lt; */
	  case 4 :
	       *ptr++ = '&';
	       *ptr++ = 'l';
	       *ptr++ = 't';
	       *ptr++ = ';';
	       break;

	  /* > -> &gt; */
	  case 5 :
	       *ptr++ = '&';
	       *ptr++ = 'g';
	       *ptr++ = 't';
	       *ptr++ = ';';
	       break;

	  /* unprintable -> &#<hex>; */
	  case 6 :
	       *ptr++ = '&';
	       *ptr++ = '#';
	       if ((unsigned char)(*src) & 0xf0)
		    *ptr++ = hex[((unsigned char)(*src) & 0xf0) >> 4];
	       *ptr++ = hex[(unsigned char)(*src) & 0x0f];
	       *ptr++ = ';';
	       break;
	  }
     }
     *ptr = '\0';
     if (dispose)
	  *dispose = 1;
     if (dlen)
	  *dlen = (size_t)(ptr - ptr0);
     if (dst)
	  *dst = ptr0;
     return(ptr0);
}


/*
 *  void make_timestr(timestr buf, time_t t, int do_ampm)
 *
 *  Format a time_t value into an HH:MM string with an optional AM or PM
 *  indicator.
 */

void
make_timestr(timestr buf, time_t t, int do_ampm)
{
     struct tm tmbuf;

     if (t == (time_t)0)
	  t = time(NULL);
     localtime_r(&t, &tmbuf);
     if (do_ampm)
     {
	  int hour = (tmbuf.tm_hour == 12) ? 12 : tmbuf.tm_hour % 12;
	  if (hour < 10)
	  {
	       buf[0] = '0' + (hour % 10);
	       buf[1] = ':';
	       buf[3] = '0' + (tmbuf.tm_min % 10); tmbuf.tm_min /= 10;
	       buf[2] = '0' + (tmbuf.tm_min % 10);
	       buf[4] = ' ';
	       buf[5] = (tmbuf.tm_hour < 12) ? 'A' : 'P';
	       buf[6] = 'M';
	       buf[7] = '\0';
	       buf[8] = '\0';
	  }
	  else
	  {
	       buf[1] = '0' + (hour % 10); hour /= 10;
	       buf[0] = '0' + (hour % 10);
	       buf[2] = ':';
	       buf[4] = '0' + (tmbuf.tm_min % 10); tmbuf.tm_min /= 10;
	       buf[3] = '0' + (tmbuf.tm_min % 10);
	       buf[5] = ' ';
	       buf[6] = (tmbuf.tm_hour < 12) ? 'A' : 'P';
	       buf[7] = 'M';
	       buf[8] = '\0';
	  }
     }
     else
     {
	  buf[1] = '0' + (tmbuf.tm_hour % 10); tmbuf.tm_hour /= 10;
	  buf[0] = '0' + (tmbuf.tm_hour % 10);
	  buf[2] = ':';
	  buf[4] = '0' + (tmbuf.tm_min % 10); tmbuf.tm_min /= 10;
	  buf[3] = '0' + (tmbuf.tm_min % 10);
	  buf[5] = '\0';
     }
}

