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

#if !defined(__BM_H__)

#define __BM_H__

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
     int                  ex;
     int                  sublen;
     const unsigned char  substr[256];
     const unsigned char  skip[256];
} bm_t;

typedef struct {
     int                  ex;
     int                  sublen;
     const unsigned char *substr;
     const int            skip[256];
} bm_ex_t;


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

ssize_t bm_search(const unsigned char *str, size_t slen, const void *info);


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

ssize_t bm_search_simple(const char *str, const char *substr);


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

int bm_skip_init(bm_t *info, const char *substr);


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

int bm_skip_init_ex(bm_ex_t *info, const unsigned char *substr, size_t len);


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

void bm_skip_done_ex(bm_ex_t *info);


#if defined(__cplusplus)
}
#endif

#endif
