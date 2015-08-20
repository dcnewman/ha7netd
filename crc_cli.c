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
 *  This program is a simple command-line utility to compute an 8 or 16 bit
 *  CRC over input data.  The algorithms used are those used by 1-wire
 *  devices.  See crc.c for further information on the algorithms.
 *
 *  The input data must be expressed in hexadecimal (e.g., 0AED9C or 0aed9c).
 *  If more than one set of hexadecimal number is supplied on the command
 *  line, then the separate numbers will first be concatenated.  That is,
 *
 *     # crc 8 0A ED 9c
 *
 *  will produce the same result as
 *
 *     # crc 8 0AED9c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crc.h"
#include "utils.h"
#include "weather.h"

static void
version(FILE *fp, const char *prog)
{
     const char *bn;

     if (!prog)
	  prog = "search";
     bn = os_basename((char *)prog);
     if (!bn || !(*bn))
	  bn = prog;
     fprintf(fp,
"%s version %d.%d.%d, built " __DATE__ " " __TIME__ "\n"
"%s\n",
	     bn, WEATHER_VERSION_MAJOR, WEATHER_VERSION_MINOR,
	     WEATHER_VERSION_REVISION, WEATHER_COPYRIGHT);
}


static void
usage(FILE *fp, const char *prog)
{
     const char *bn = os_basename((char *)prog);

     fprintf(fp,
"Usage: %s [-v] [-h] [-?] 16|8 data [data [...]]\n"
"     16 - Perform a CRC-16 computation\n"
"      8 - Perform a DOW CRC computation (e.g, CRC-8)\n"
"   data - Hex encoded data to compute the CRC of\n"
" -h, -? - This usage message\n"
"     -v - Write version information and then exit\n",
	     bn ? bn : prog);
}


int main(int argc, const char *argv[])
{
     int algorithm, crc, i;
     char c;
     unsigned char *data;
     size_t l, len, maxlen, totlen;

     algorithm = 0;
     data      = NULL;
     maxlen    = 0;
     totlen    = 0;

     for (i = 1; i < argc; i++)
     {
	  c = argv[i][0];
	  if (c == '?')
	  {
	       usage(stdout, argv[0]);
	       return(0);
	  }
	  else if (c == '-')
	  {
	       switch(argv[i][1])
	       {
	       default :
		    usage(stderr, argv[0]);
		    return(1);

	       case 'h' :
	       case '?' :
		    usage(stdout, argv[0]);
		    return(0);

	       case 'v' :
		    version(stdout, argv[0]);
		    return(0);
	       }
	  }
	  else if (algorithm == 0)
	  {
	       if (c == '8' && argv[i][1] == '\0')
		    algorithm = 8;
	       else if (c == '1' && argv[i][1] == '6' && argv[i][2] == '\0')
		    algorithm = 16;
	       else
	       {
		    usage(stderr, argv[0]);
		    return(1);
	       }
	  }
	  else if (memchr("0123456789abcdefABCDEF", c, 22))
	  {
	       len = strlen(argv[i]);
	       l = (len % 2) ? len + 1 : len;
	       if ((l + totlen) > maxlen)
	       {
		    size_t newlen = 4096 * ((l + totlen + 4095) / 4096);
		    unsigned char *tmp =
			 (unsigned char *)realloc((char *)data, newlen);
		    if (!tmp)
		    {
			 fprintf(stderr, "Insufficient virtual memory\n");
			 return(1);
		    }
		    data   = tmp;
		    maxlen = newlen;
	       }
	       if (l != len)
	       {
		    data[totlen] = '0';
		    memcpy(data + totlen + 1, argv[i], len);
		    totlen += len + 1;
	       }
	       else
	       {
		    memcpy(data + totlen, argv[i], len);
		    totlen += len;
	       }
	  }
	  else
	  {
	       usage(stderr, argv[0]);
	       return(1);
	  }
     }

     /*
      *  Did we get this far without any algorithm specified?
      */
     if (algorithm == 0)
     {
	  if (data)
	       /*
		*  Not possible for data != NULL but just in case
		*/
	       free((char *)data);
	  usage(stderr, argv[0]);
	  return(1);
     }

     /*
      *  Compute the CRC
      */
     Hex2Byte(data, (char *)data, totlen);

     totlen >>= 1;
     crc = 0;

     if (algorithm == 16)
     {
	  for (i = 0; i < totlen; i++)
	       crc = crc16(crc, data[i]);
	  fprintf(stdout, "%04x\n", (0xffff & crc));
     }
     else
     {
	  for (i = 0; i < totlen; i++)
	       crc = crc8(crc, data[i]);
	  fprintf(stdout, "%02x\n", (0xff & crc));
     }

     free((char *)data);
     return(0);
}
