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

static struct {
     int   len;
     const char *val;
} pretty_print_table[256] = {
     {5, "<NUL>"},
     {5, "<SOH>"},
     {5, "<STX>"},
     {5, "<ETX>"},
     {5, "<EOT>"},
     {5, "<ENQ>"},
     {5, "<ACK>"},
     {5, "<BEL>"},
     {4, "<BS>"},
     {1, "\t"},
     {1, "\n"},
     {4, "<VT>"},
     {4, "<FF>"},
     {1, "\r"},
     {4, "<SO>"},
     {4, "<SI>"},
     {5, "<DLE>"},
     {5, "<DC1>"},
     {5, "<DC2>"},
     {5, "<DC3>"},
     {5, "<DC4>"},
     {5, "<NAK>"},
     {5, "<SYN>"},
     {5, "<ETB>"},
     {5, "<CAN>"},
     {4, "<EM>"},
     {5, "<SUB>"},
     {5, "<ESC>"},
     {4, "<FS>"},
     {4, "<GS>"},
     {4, "<RS>"},
     {4, "<US>"},
     {1, " "},
     {1, "!"},
     {1, "\""},
     {1, "#"},
     {1, "$"},
     {1, "%"},
     {1, "&"},
     {1, "'"},
     {1, "("},
     {1, ")"},
     {1, "*"},
     {1, "+"},
     {1, ","},
     {1, "-"},
     {1, "."},
     {1, "/"},
     {1, "0"},
     {1, "1"},
     {1, "2"},
     {1, "3"},
     {1, "4"},
     {1, "5"},
     {1, "6"},
     {1, "7"},
     {1, "8"},
     {1, "9"},
     {1, ":"},
     {1, ";"},
     {1, "<"},
     {1, "="},
     {1, ">"},
     {1, "?"},
     {1, "@"},
     {1, "A"},
     {1, "B"},
     {1, "C"},
     {1, "D"},
     {1, "E"},
     {1, "F"},
     {1, "G"},
     {1, "H"},
     {1, "I"},
     {1, "J"},
     {1, "K"},
     {1, "L"},
     {1, "M"},
     {1, "N"},
     {1, "O"},
     {1, "P"},
     {1, "Q"},
     {1, "R"},
     {1, "S"},
     {1, "T"},
     {1, "U"},
     {1, "V"},
     {1, "W"},
     {1, "X"},
     {1, "Y"},
     {1, "Z"},
     {1, "["},
     {1, "\\"},
     {1, "]"},
     {1, "^"},
     {1, "_"},
     {1, "`"},
     {1, "a"},
     {1, "b"},
     {1, "c"},
     {1, "d"},
     {1, "e"},
     {1, "f"},
     {1, "g"},
     {1, "h"},
     {1, "i"},
     {1, "j"},
     {1, "k"},
     {1, "l"},
     {1, "m"},
     {1, "n"},
     {1, "o"},
     {1, "p"},
     {1, "q"},
     {1, "r"},
     {1, "s"},
     {1, "t"},
     {1, "u"},
     {1, "v"},
     {1, "w"},
     {1, "x"},
     {1, "y"},
     {1, "z"},
     {1, "{"},
     {1, "|"},
     {1, "}"},
     {1, "~"},
     {5, "<DEL>"},
     {4, "<80>"},
     {4, "<81>"},
     {4, "<82>"},
     {4, "<83>"},
     {4, "<84>"},
     {4, "<85>"},
     {4, "<86>"},
     {4, "<87>"},
     {4, "<88>"},
     {4, "<89>"},
     {4, "<8a>"},
     {4, "<8b>"},
     {4, "<8c>"},
     {4, "<8d>"},
     {4, "<8e>"},
     {4, "<8f>"},
     {4, "<90>"},
     {4, "<91>"},
     {4, "<92>"},
     {4, "<93>"},
     {4, "<94>"},
     {4, "<95>"},
     {4, "<96>"},
     {4, "<97>"},
     {4, "<98>"},
     {4, "<99>"},
     {4, "<9a>"},
     {4, "<9b>"},
     {4, "<9c>"},
     {4, "<9d>"},
     {4, "<9e>"},
     {4, "<9f>"},
     {4, "<a0>"},
     {4, "<a1>"},
     {4, "<a2>"},
     {4, "<a3>"},
     {4, "<a4>"},
     {4, "<a5>"},
     {4, "<a6>"},
     {4, "<a7>"},
     {4, "<a8>"},
     {4, "<a9>"},
     {4, "<aa>"},
     {4, "<ab>"},
     {4, "<ac>"},
     {4, "<ad>"},
     {4, "<ae>"},
     {4, "<af>"},
     {4, "<b0>"},
     {4, "<b1>"},
     {4, "<b2>"},
     {4, "<b3>"},
     {4, "<b4>"},
     {4, "<b5>"},
     {4, "<b6>"},
     {4, "<b7>"},
     {4, "<b8>"},
     {4, "<b9>"},
     {4, "<ba>"},
     {4, "<bb>"},
     {4, "<bc>"},
     {4, "<bd>"},
     {4, "<be>"},
     {4, "<bf>"},
     {4, "<c0>"},
     {4, "<c1>"},
     {4, "<c2>"},
     {4, "<c3>"},
     {4, "<c4>"},
     {4, "<c5>"},
     {4, "<c6>"},
     {4, "<c7>"},
     {4, "<c8>"},
     {4, "<c9>"},
     {4, "<ca>"},
     {4, "<cb>"},
     {4, "<cc>"},
     {4, "<cd>"},
     {4, "<ce>"},
     {4, "<cf>"},
     {4, "<d0>"},
     {4, "<d1>"},
     {4, "<d2>"},
     {4, "<d3>"},
     {4, "<d4>"},
     {4, "<d5>"},
     {4, "<d6>"},
     {4, "<d7>"},
     {4, "<d8>"},
     {4, "<d9>"},
     {4, "<da>"},
     {4, "<db>"},
     {4, "<dc>"},
     {4, "<dd>"},
     {4, "<de>"},
     {4, "<df>"},
     {4, "<e0>"},
     {4, "<e1>"},
     {4, "<e2>"},
     {4, "<e3>"},
     {4, "<e4>"},
     {4, "<e5>"},
     {4, "<e6>"},
     {4, "<e7>"},
     {4, "<e8>"},
     {4, "<e9>"},
     {4, "<ea>"},
     {4, "<eb>"},
     {4, "<ec>"},
     {4, "<ed>"},
     {4, "<ee>"},
     {4, "<ef>"},
     {4, "<f0>"},
     {4, "<f1>"},
     {4, "<f2>"},
     {4, "<f3>"},
     {4, "<f4>"},
     {4, "<f5>"},
     {4, "<f6>"},
     {4, "<f7>"},
     {4, "<f8>"},
     {4, "<f9>"},
     {4, "<fa>"},
     {4, "<fb>"},
     {4, "<fc>"},
     {4, "<fd>"},
     {4, "<fe>"},
     {4, "<ff>"}
};

static const char *
pretty_print(const void *data, ssize_t len, char *workbuf, size_t *buflen,
	     size_t maxbuflen)
{
     unsigned char *buf, *buf_end, *buf_start, c;
     int i;
     const unsigned char *str;
     static const char empty_str[] = "";

     /*
      *  Sanity checks
      */
     if (!data || !workbuf || !maxbuflen || len <= 0)
     {
	  if (buflen)
	       *buflen = 0;
	  if (workbuf && maxbuflen)
	  {
	       *workbuf = '\0';
	       return(workbuf);
	  }
	  else
	       return(empty_str);
     }

     str = (const unsigned char *)data;
     buf = (unsigned char *)workbuf;
     buf_end = buf_start = (unsigned char *)buf;
     buf_end += maxbuflen - 1;
     while (len--)
     {
	  c = *str++;
	  i = pretty_print_table[c].len;
	  if (i == 1)
	  {
	       *buf++ = c;
	       if (buf >= buf_end)
		    break;
	  }
	  else
	  {
	       if ((buf+i) > buf_end)
		    break;
	       memmove(buf, pretty_print_table[c].val, i);
	       buf += i;
	  }
     }
     *buf = '\0';
     if (buflen)
	  *buflen = buf - buf_start;
     return((const char *)buf_start);
}


static int
ensure(e_string *estr, size_t len, size_t incr)
{
     size_t newlen;
     char *tmp;

     if (!estr || (estr->len + len) < estr->maxlen)
	  return(0);
     newlen = incr * (int)((estr->len + len + 1 + incr - 1) / incr);
     tmp = (char *)malloc(newlen);
     if (!tmp)
	  return(-1);
     if (estr->data)
     {
	  if (estr->len)
	       memmove(tmp, estr->data, estr->len);
	  free(estr->data);
     }
     estr->data   = tmp;
     estr->maxlen = newlen;
     return(0);
}


static int
echarcat(e_string *estr, char c)
{
     if (!estr->data)
     {
	  estr->maxlen = 0;
	  estr->len    = 0;
     }
     if (estr->len >= estr->maxlen)
     {
	  char *tmp;
	  size_t newlen;

	  /*
	   *  Grow in 2K increments
	   */
	  newlen = estr->maxlen + 2048;
	  tmp = (char *)realloc(estr->data, newlen);
	  if (!tmp)
	  {
	       debug("echarcat(%d): Insufficient virtual memory; call failed",
		     __LINE__);
	       return(ERR_NOMEM);
	  }
	  estr->data   = tmp;
	  estr->maxlen = newlen;
     }
     if (estr->data)
	  estr->data[estr->len++] = c;
     return(ERR_OK);
}


static int
estrncat(e_string *estr, const char *data, size_t len)
{
     if (!estr->data)
     {
	  estr->maxlen = 0;
	  estr->len    = 0;
     }

     if ((estr->len + len) >= estr->maxlen)
     {
	  char *tmp;
	  size_t newlen;

	  /*
	   *  Grow in multiples of 10K
	   */
	  newlen = (int)((estr->len + len + (10240 - 1)) / 10240) * 10240;
	  tmp = (char *)realloc(estr->data, newlen);
	  if (!tmp)
	  {
	       debug("estrncat(%d): Insufficient virtual memory; call failed",
		     __LINE__);
	       return(ERR_NOMEM);
	  }
	  estr->data   = tmp;
	  estr->maxlen = newlen;
     }
     memmove(estr->data + estr->len, data, len);
     estr->len += len;

     return(ERR_OK);
}


static int
estrncmp(const e_string *estr, const char *str, size_t len)
{
     int ires;

     ires = -1;
     if (!estr || !str || !len || estr->len < len ||
	 (ires = memcmp(estr->data, str, len)))
	  return(ires);
     else
	  return(0);
}


static void
edispose(e_string *estr)
{
     if (estr->data != NULL)
	  free(estr->data);
     estr->len    = 0;
     estr->maxlen = 0;
}
