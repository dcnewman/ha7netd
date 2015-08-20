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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "bm.h"

/*
 *  ssize_t bm_search(const unsigned char *str, size_t slen, const bm_t *info)
 *
 *    Using the Boyer-Moore algorithm, search from left to right the bytes
 *    [0, slen-1] of str for the first occurrence of the substring
 *    info->substr.  The structure pointed at by info must first be
 *    initialized by calling bm_skip_init().  Once bm_skip_init() is called,
 *    bm_search() may be repeatedly called in order to search for the SAME
 *    substring.  bm_skip_init() only needs to be called again if the substring
 *    to search for changes.
 *
 *  Call arguments:
 *
 *    const unsigned char *str
 *      Pointer to the string of data to search.  The data MAY contain
 *      NULs.  That is, the data is not prematurely terminated by a NUL
 *      character (0x00).  Used for input only.
 *
 *    size_t slen
 *      Length in bytes of the data in str to search.  An input value of
 *      slen == 0 will lead to a return value of -1.  Used for input only.
 *
 *    void *info
 *      Pointer to either a bm_t or bm_ex_t structure initialized via
 *      bm_skip_init() or bm_skip_init().  This structure describes the
 *      substring to search for and includes a pre-computed Boyer-Moore
 *      skip table.  Used for input only.
 *
 *  Return values:
 *
 *     0 <= ret < slen -- Success.  Substring located in str starting at the
 *                        (zero-based) index ret.  For example, a search of
 *                        "12345" for the substring "34" will return the value
 *                        2.
 *         ret >= slen -- Failure.  Substring not found.
 *             ret < 0 -- Failure.  Invalid call arguments supplied.
 */

ssize_t
bm_search(const unsigned char *str, size_t slen, const void *info)
{
     int i, j, n, m;
     const bm_t *binfo = (const bm_t *)info;

     if (!str || !slen || !binfo)
     {
	  /*
	   *  Bogus input
	   */
	  errno = EINVAL;
	  return((ssize_t)-1);
     }
     else if (binfo->sublen <= 0)
	  /*
	   *  Empty string
	   */
	  return((ssize_t)0);

     n = (int)(0x7fffffff & slen);
     m = binfo->sublen;

     if (!binfo->ex)
     {
	  i = j = m - 1;
	  do {
	       if (str[i] == binfo->substr[j])
	       {
		    --i; --j;
	       }
	       else
	       {
		    if ((m - j + 1) > binfo->skip[str[i]])
			 i += m - j + 1;
		    else
			 i += binfo->skip[str[i]];
		    j = m - 1;
	       }
	  } while (j >= 0 && i < n);

	  /*
	   *  If j < 0, then we found a match starting at i + 1
	   *  If j >= 0, then we did not find a match and i + 1 >= slen
	   */
	  return((ssize_t)(i + 1));
     }
     else
     {
	  const bm_ex_t *xinfo = (const bm_ex_t *)info;

	  i = j = m - 1;
	  do {
	       if (str[i] == xinfo->substr[j])
	       {
		    --i; --j;
	       }
	       else
	       {
		    if ((m - j + 1) > xinfo->skip[str[i]])
			 i += m - j + 1;
		    else
			 i += xinfo->skip[str[i]];
		    j = m - 1;
	       }
	  } while (j >= 0 && i < n);

	  /*
	   *  If j < 0, then we found a match starting at i + 1
	   *  If j >= 0, then we did not find a match and i + 1 >= slen
	   */
	  return((ssize_t)(i + 1));
     }
}


/*
 *  ssize_t bm_search_simple(const char *str, const char *substr)
 *
 *    Simplified Boyer-Moore string search.  Do not use this routine if you
 *    will be repeatedly searching text for the same substring.  When doing
 *    repeated searches for the same substring, use bm_search() and
 *    bm_skip_init() [or bm_skip_init_ex()].
 *
 *    Using the Boyer-More algorithm, search from left to right the bytes of
 *    str for first occurrence the substring substr.
 *
 *  Call arguments:
 *
 *    const char *str
 *      Pointer to the string of data to search.  The string must be NUL
 *      terminated.  Used for input only.
 *
 *    const char *substr
 *      Pointer to the substring to search str for.  The substring must be
 *      NUL terminated.  Used for input only.

 *  Return values:
 *
 *     0 <= ret < strlen(str) -- Success.  Substring located in str starting
 *                               at the (zero-based) index ret.  For example,
 *                               a search of "12345" for the substring "34"
 *                               will return the value 2.
 *         ret >= strlen(str) -- Failure.  Substring not found.
 *                    ret < 0 -- Failure.  Invalid call arguments supplied.
 */

ssize_t
bm_search_simple(const char *str, const char *substr)
{
     bm_t binfo;
     int ires;
     size_t len;

     /*
      *  Basic sanity checks
      */
     if (!str || !substr || !(len = strlen(str)))
     {
	  errno = EINVAL;
	  return((ssize_t)-1);
     }

     /*
      *  Build the skip table
      */
     if ((ires = bm_skip_init(&binfo, substr)))
	  return((ssize_t)ires);

     /*
      *  And perform the search
      */
     return(bm_search((const unsigned char *)str, len, &binfo));
}


/*
 *  int bm_skip_init(bm_t *info, const char *substr)
 *
 *    Initialize a Boyer-Moore "skip" table for use with bm_search().
 *    The substring's must be NUL terminated and its length may not
 *    exceed 255 bytes.  Both of these limits are arbitrary and can
 *    easily be removed by simple code changes.  The use of "unsigned char"
 *    for the skip table is the cause of the 255 byte length limit.
 *    The lack of a length call argument for substr is the source of
 *    the NUL terminator limit.  Use bm_skip_init_ex() to avoid these
 *    limits.
 *
 *  Call arguments:
 *
 *    bm_t *info
 *      Pointer to a bm_t structure to initialize.  Used for output only.
 *
 *    const char *substr
 *      Pointer to the substring to search for with bm_search().  The
 *      substring must be NUL terminated.  Used for input only.
 *
 *  Return values:
 *
 *     0 -- Success.
 *    -1 -- Invalid call arguments.
 */

int
bm_skip_init(bm_t *info, const char *substr)
{
     int i;
     unsigned char *skip;

     if (!info || !substr)
     {
	  errno = EINVAL;
	  return(-1);
     }

     /*
      *  Size limit of 0xff is driven by the fact that the skip table
      *  uses unsigned chars.  To allow larger strings, change the
      *  skip table to use, say, unsigned shorts or ints.
      */
     info->sublen = strlen(substr);
     if (info->sublen <= 0 || info->sublen > 0xff)
     {
	  errno = EINVAL;
	  return(-1);
     }

     /*
      *  Save the search substring
      */
     memmove((void *)info->substr, substr, info->sublen);

     /*
      *  Note the flavor of bm_ struct
      */
     info->ex = 0;

     /*
      *  Set each entry in the table to the length of the search string
      */
     memset((void *)info->skip, (unsigned char)(0xff & info->sublen), 256);

     /*
      *  Now, for entries corresponding to characters in the search
      *  string, adjust the values in the skip table
      */
     skip = (unsigned char *)info->skip; /* To deal with const'ness */
     for (i = 0; i < info->sublen; i++)
	  skip[(unsigned char)substr[i]] =
	       (unsigned char)(info->sublen - (i + 1));

     return(0);
}


/*
 *  int bm_skip_init_ex(bm_ex_t *info, const unsigned char *substr, size_t len)
 *
 *    Initialize a Boyer-Moore "skip" table for use with bm_search().
 *    Unlike bm_skip_init(), this routine allows for a nearly unlimited
 *    size substring, and the substring may contain NULs.
 *
 *    When finished with the bm_ex_t structure, bm_skip_done_ex() must be
 *    called to release allocated memory referenced by the structure.
 *
 *  Call arguments:
 *
 *    bm_ex_t *info
 *      Pointer to a bm_ex_t structure to initialize.  Used for output only.
 *
 *    const char *substr
 *      Pointer to the substring to search for with bm_search().  The length
 *      of the substring may not exceed 2,147,483,647 bytes (0x7fffffff).
 *      Used for input only.
 *
 *    size_t len
 *      Length in bytes of the data pointed at by substr.  Used for input
 *      only.
 *
 *  Return values:
 *
 *     0 -- Success.
 *    -1 -- Invalid call arguments.
 */

int
bm_skip_init_ex(bm_ex_t *info, const unsigned char *substr, size_t len)
{
     int i, *skip;

     if (!info || !substr || !len || len > 0x7fffffff)
     {
	  errno = EINVAL;
	  return(-1);
     }

     info->ex = 1;
     info->sublen = (int)(0x7fffffff & len);

     /*
      *  Save the search substring
      */
     info->substr = (unsigned char *)malloc(info->sublen);
     if (!info->substr)
	  return(-1);
     memmove((void *)info->substr, substr, info->sublen);

     /*
      *  Set each entry in the table to the length of the search string
      */
     skip = (int *)info->skip; /* to avoid const'ness */
     for (i = 0; i < 256; i++)
	  skip[i] = info->sublen;

     /*
      *  Now, for entries corresponding to characters in the search
      *  string, adjust the values in the skip table
      */
     for (i = 0; i < info->sublen; i++)
	  skip[substr[i]] = info->sublen - (i + 1);

     return(0);
}


/*
 *  void bm_skip_done_ex(bm_ex_t *info)
 *
 *    Free up virtual memory associated with a bm_ex_t structure.
 *
 *  Call arguments:
 *
 *    bm_ex_t *info
 *      Pointer to a bm_ex_t structure previously initialized with
 *      bm_skip_init_ex().  Used for input and output.
 *
 *  Return values:
 *
 *    None.
 */

void
bm_skip_done_ex(bm_ex_t *info)
{
     if (info && info->substr)
     {
	  free((void *)info->substr);
	  info->substr = NULL;
     }
}
