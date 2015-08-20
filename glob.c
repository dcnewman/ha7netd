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
 *  Yet another implementation of glob-style pattern matching.
 *  This implementation matches bytes to bytes (i.e., may not
 *  be appropriate for character sets with multibyte characters).
 *
 *        *   - Matches zero or more bytes
 *        ?   - Matches exactly one byte
 *    [xyz]   - Matches the bytes 'x', 'y', or 'z'
 *    [!xyz]  - Matches any bytes except 'x', 'y', or 'z'
 *    [x-z]   - Matches any bytes in the range 'x' through 'z'
 *    [!x-z]  - Matches any bytes except for those in the range 'x' through 'z'
 *    [x-zab] - Matches the bytes 'a', 'b', or in the range 'x' through 'z'
 */

#include <string.h>
#include "glob.h"

int
isglob(const char *str)
{
     if (!str)
	  return(0);
     while (*str)
     {
	  switch (*str)
	  {
	  case '*' :
	  case '?' :
	  case '[' :
	       return(-1);

	  case '\\' :
	       if (str[1] == '\0')
		    return(0);
	       str += 2;
	       break;

	  default :
	       str += 1;
	       break;
	  }
     }
     return(0);
}


int
glob(const char *pat, const char *str, int isdotspecial)
{
     char c, cnext;
     const char *str0;

     if (!pat || !str)
	  /*
	   *  Bozos
	   */
	  return(0);

     str0 = str;
     while ((c = *pat++))
     {
	  /*
	   *  Note that we do not stop here if *str == NULL as we still
	   *  need to inspect the rest of pattern.  Even if there are bytes
	   *  left in the pattern, we may still have a match (e.g., if
	   *  pattern still has a trailing * which matches zero or more
	   *  characters).
	   */
	  switch (c)
	  {

	       /*
		*  First, let's handle the three easy cases:
		*
		*    1. A literal character in the pattern,
		*    2. No special in pattern at this point (nearly the same
		*       as 1), and
		*    3. The special '?'
		*/

	  /*
	   *  The next byte is literal: see that the pattern and the
	   *  input string both agree in the next position
	   */
	  case '\\' :
	       c = *pat++;
	       /*
		*  Fall through to the next case
		*/

	   /*
	    *  No special in pattern: see that at this byte, the pattern
	    *  an input strings agree
	    */
	  default :
	       if (c != *str++)
		    /*
		     *  Well that stopped us dead: no match :-(
		     */
		    return(0);
	       break;

	  /*
	   *  ? in pat consumes the next byte in the input string
	   */
	  case '?' :
	       if (!(*str) || (isdotspecial && str == str0 && *str == '.'))
		    return(0);
	       str++;
	       break;

	       /*
		*  Now for the two relatively difficult cases:
		*
		*    4. The special '*', and
		*    5. The special '['
		*/

	  /*
	   *  You know, recursion seems like the way to go here:
	   */
	  case '*' :
	       if (isdotspecial && str == str0 && *str == '.')
		    return(0);

	       /*
		*  While a little bit of recursion is a good thing,
		*  let's no get carried away: collapse  multiple,
		*  consecutive occurrences of '*' into a single '*'
		*
		*  So, let us move over any consecutive '*' or '?'
		*/
	       while ((c = *pat++) == '*' || c == '?')
		    if (c == '?' && !(*str++))
			 /*
			  *  Pattern needs more input characters to consume
			  *  but the input string has already been completely
			  *  consumed: no match :-(
			  */
			 return(0);
	       if (c == '\0')
		    /*
		     *  Pattern ends with * which consumes the
		     *  remainder of the input string: a match :-)
		     */
		    return(1);

	       /*
		*  Okay, now let the '*' that started this all now
		*  start consuming everything from str up until we
		*  find a character in str which matches whatever
		*  came next after the '*' in the pattern.
		*/
	       cnext = (c == '\\') ?
		    *pat : /* what comes next after the '\' */
		    c;     /* or the character we stopped at when we zoomed
			    * through pat
			    */
	       while (1)
	       {
		    /*
		     *  Note that we loop here since we don't know if
		     *      pat = '*x...' should match str = '123xxx'
		     *      at the first, second, or third 'x' in str.
		     */
		    if ((c == '[' || *str == cnext) &&
			/*
			 *  Finally, we recur.  And guess what?  We need
			 *  to back pat up by one.  An exercise best left
			 *  to the reader ;-)
			 */
			glob(--pat, str, 0))
			 return(1);
		    if (!(*str++))
			 return(0);
	       }
	       break;

	  /*
	   *  Okay: don't really think that we need this one here,
	   *  but might as well do it as it seems to be part of
	   *  the "generic" glob matching expectations people have.
	   *
	   *  Since this is just globbing and not regular expressions,
	   *  no fancy fru-fru is allowed within or after the '[...]'
	   *  expression.  This means that [...] matches exactly one
	   *  character.  No more, no less.  As such, we can bail
	   *  immediately if we have exhausted the input string.
	   */
	  case '[' :
	       if (!(*str))
		    /*
		     *  Nothing else left in the input string to match
		     *  against.  However, as mentioned above, '[...]'
		     *  is a restricted case of '?' in that there must
		     *  be an input character to match against: no match :-(
		     */
		    return(0);
	       else
	       {
		    unsigned char c_endpoints[2], mark, targets[256], uc, us;
		    int flip_flop, isliteral;

		    /*
		     *  For purposes of short ciruiting the construction
		     *  of targets[], we note in advance the character we
		     *  wish to match against.
		     *
		     *  Note that we can only do this short circuiting when
		     *  a negation, '!', was not present.  When no negation
		     *  is present, we are being told serially what the
		     *  next input string character, *str, must match. And
		     *  so, as we serially parse the '[...]' from left to
		     *  right, we know that we are done if we see within the
		     *  '[...]' the character *str.
		     *
		     *  However, when a negation has occurred, we do not know
		     *  what the acceptable matches are until we have scanned
		     *  everything within the '[...]'.  For example, if the
		     *  next character in the input string is '1' and the
		     *  pattern is '[!A-Za-z1]' we do not know that '1' is
		     *  unacceptable until the very end of the contents of
		     *  '[!A-Za-z1]'.  (We interpret '[!A-Za-z1]' as 'anything
		     *  but the characters 'A' through 'Z', not the characters
		     *  'a' through 'z', and not the character '1'.)
		     */
		    us = (unsigned char)(*str++);

		    /*
		     *  A leading ! indicates to match against the characters
		     *  NOT specified within the square brackets
		     */
		    if (*pat != '!')
		    {
			 /*
			  *  Set targets[] to all zeroes and
			  *  set to non-zero the characters specified
			  *  within the '[...]' expression
			  */
			 mark = 0xff;
			 memset(targets, 0x00, sizeof(targets));
		    }
		    else
		    {
			 /*
			  *  Set targets[] to a non-zero value and
			  *  set to zero the characters specified
			  *  within the '[...]' expression
			  */
			 mark = 0x00;
			 memset(targets, 0xff, sizeof(targets));
			 targets[0] = 0x00;
			 pat++;
		    }

		    flip_flop = 0;
		    isliteral = 0;
		    while ((uc = *pat++) && uc != ']')
		    {
			 if (isliteral)
			 {
			      isliteral = 0;
			      if (mark && uc == us)
				   goto hyperspace_bypass;
			      c_endpoints[flip_flop] = uc;
			      targets[uc] = mark;
			      if (flip_flop == 1)
				   /*
				    *  We previously saw a '-' so now
				    *  lets fill out c_endpoints[0] -
				    *  c_endpoints[1] in the array of
				    *  targets.
				    */
				   flip_flop = 2;
			 }
			 else
			 {
			      switch(uc)
			      {
			      default :
				   if (mark && uc == us)
					goto hyperspace_bypass;
				   c_endpoints[flip_flop] = uc;
				   targets[uc] = mark;
				   if (flip_flop == 1)
					/*
					 *  We previously saw a '-' so now
					 *  lets fill out c_endpoints[0] -
					 *  c_endpoints[1] in the array of
					 *  targets.
					 */
					flip_flop = 2;
				   break;

			      case '\\' :
				   isliteral = 1;
				   break;

			      case '-' :
				   if (flip_flop == 1)
					/*
					 *  '--' is an error!
					 */
					return(-1);
				   flip_flop = 1;
				   break;
			      }
			      if (flip_flop == 2 && !isliteral)
			      {
				   unsigned char c1, c2;

				   /*
				    *  Before we forget,
				    *  set flip_flop back to 0
				    */
				   flip_flop = 0;

				   /*
				    *  For convenience
				    */
				   c1 = c_endpoints[0];
				   c2 = c_endpoints[1];

				   /*
				    *  Treat [Z-A] as [A-Z]
				    */
				   if (c1 > c2)
				   {
					c1 = c2;
					c2 = c_endpoints[0];
				   }

				   if (mark && (c1 <= us && us <= c2))
					goto hyperspace_bypass;

				   /*
				    *  We've already set targets[c1] and
				    *  targets[c2] so there's nothing to do
				    *  if c2 = c1 + 1 or c2 == c1.
				    */
				   if ((c2 - c1) > 1)
					/*
					 *  We now need to set targets[c1 + 1]
					 *  through targets[c2 - 1]
					 */
					memset(targets + c1 + 1, mark,
					       (c2 - c1) - 1);
			      }
			 }
		    }
		    if (!(*pat))
			 /*
			  *  Expression did not have a closing ']'.
			  *  Consider this an error!
			  */
			 return(-1);
		    /*
		     *  Okay, we went to all that work for this next
		     *  very simple step....  Sort of anti-climatic, if
		     *  you ask me....
		     */
		    if (!targets[us])
			 /*
			  *  Current character in the input string was not in
			  *  the specified set/range of matching characters:
			  *  no match :-(
			  */
			 return(0);

		    /*
		     *  Everything fine so far
		     */
		    break;

	       hyperspace_bypass:
		    isliteral = 0;
		    while ((uc = *pat++) && uc != ']')
		    {
			 if (isliteral)
			      isliteral = 0;
			 else if (uc == '\\')
			      isliteral = 1;
			 else if (uc == '-' && *pat == '-')
			      return(-1);
		    }
		    break;
	       }
	       break;
	  }
     }

     /*
      *  Okay, we've consumed the pattern.  We therefore have a match
      *  if we also consumed the input string.  So, now it's time for
      *  the big question.....
      */
     return(!(*str) ? 1 : 0);
}
