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
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#if defined(__osf__) || defined(__APPLE__)
#  define MAP_FLAGS MAP_FILE | MAP_SHARED
#else
#  define MAP_FLAGS MAP_SHARED
#endif

#include "opt.h"

typedef struct {
     const char *allows_emtpy_opts;
} context_t;

/*
 *  Useful to keep an empty string around
 */
static const char *empty_str = "";

/*
 *  Line terminator
 */
#if !defined(_WIN32)
static char ch_line_term = '\n';
#else
static char ch_line_term = '\r';
#endif

/*
 *  Forward declarations
 */
static int parse_line(opt_group_t *group, const char **name, size_t *nlen,
  const char **valu, size_t *vlen, int *isgroup, const char *buf, size_t len);
static void dispose_sources(opt_source_t *sources);
static void dispose_groups(opt_group_t *groups);
static opt_group_t *add_group(opt_t *groups, const char *nam, size_t nlen,
  const char *val, size_t vlen, opt_source_t *source, int lineno, int flags,
  int *errcode);
static int add_option(opt_group_t *group, const char *nam, size_t nlen,
  const char *val, size_t vlen, opt_source_t *source, int lineno,
  int flags);
static opt_group_t *opt_read_inner(opt_t *opts, opt_group_t *current_group,
  size_t depth, const char *fname, size_t flen, int *fexists, int *errcode);
static int empty_allowed(context_t *ctx, const char *gname, size_t glen);


static debug_proc_t  our_debug_ap;
static debug_proc_t *debug_proc = our_debug_ap;
static void         *debug_ctx  = NULL;
static int do_debug = 0;

static void
our_debug_ap(void *ctx, int reason, const char *fmt, va_list ap)
{
     (void)ctx;
     (void)reason;

     vfprintf(stderr, fmt, ap);
     fputc('\n', stderr);
     fflush(stderr);
}

static void
debug(const char *fmt, ...)
{
     va_list ap;

     if (do_debug && debug_proc)
     {
	  va_start(ap, fmt);
	  (*debug_proc)(debug_ctx, ERR_LOG_ERR, fmt, ap);
	  va_end(ap);
     }
}


void
opt_debug_set(debug_proc_t *proc, void *ctx, int flags)
{
     debug_proc = proc ? proc : our_debug_ap;
     debug_ctx  = proc ? ctx : NULL;
     do_debug   = flags ? 1 : 0;
}


static void
dispose_sources(opt_source_t *sources)
{
     opt_source_t *tmp;

     while (sources)
     {
	  tmp = sources->next;
	  free(sources);
	  sources = tmp;
     }
}


static int
add_source(opt_t *opts, const char *source, size_t slen)
{
     opt_source_t *tmp;

     /*
      *  Habitual sanity checks
      */
     if (!opts || !source)
     {
	  debug("add_source(%d): Invalid call arguments supplied;"
		" opts=%p, source=%p", __LINE__, opts, source);
	  return(ERR_BADARGS);
     }

     /*
      *  Already in the list?  We only look at the most recent
      *  entry in the list.
      */
     if (opts->sources && opts->sources->slen == slen &&
	 !memcmp(opts->sources->source, source, slen))
	  return(ERR_OK);

     /*
      *  Allocate memory for this entry
      */
     tmp = (opt_source_t *)malloc(sizeof(opt_source_t) + slen);
     if (!tmp)
     {
	  debug("add_source(%d): Insufficient virtual memory", __LINE__);
	  return(ERR_NOMEM);
     }

     tmp->stype = OPT_STYPE_SOURCE;
     tmp->ssize = sizeof(opt_source_t) + slen;
     tmp->slen  = slen;
     memcpy(tmp->source, source, tmp->slen);
     tmp->source[tmp->slen] = '\0';
     tmp->next = opts->sources;
     opts->sources = tmp;

     /*
      *  Done
      */
     return(ERR_OK);
}


static int
parse_line(opt_group_t *grp, const char **name, size_t *nlen,
	   const char **valu, size_t *vlen, int *isgroup, const char *buf,
	   size_t len)
{
     char c;
     int isgrp;
     const char *nam, *val;
     size_t nl, vl;

     /*
      *  Sanity check
      */
     if (!buf)
     {
	  debug("parse_line(%d): Invalid call arguments; buf=%p",
		__LINE__, buf);
	  return(ERR_BADARGS);
     }

     /*
      *  Initializations
      */
     isgrp = 0;

     /*
      *  Ignore leading LWSP
      */
     while (len && isspace(*buf))
     {
	  buf++;
	  len--;
     }
     if (!len || (*buf == '#'))
	  /*
	   *  Semantically blank line
	   */
	  return(ERR_EOM);

     /*
      *  Group name?
      */
     if (*buf == '[')
     {
	  /*
	   *  Start of a new option group
	   */
	  isgrp = 1;
	  buf++;
	  len--;

	  /*
	   *  Skip any LWSP
	   */
	  while (len && isspace(*buf))
	  {
	       buf++;
	       len--;
	  }
	  if (!len || (*buf == ']'))
	      return(ERR_SYNTAX);
     }

     /*
      *  Ignore trailing LWSP
      */
     while (len && isspace(buf[len-1]))
	  --len;
     if (!len)
	  /*
	   *  When !isgrp, we shouldn't land here but just in case...
	   */
	  return(isgrp ? ERR_SYNTAX : ERR_EOM);

     if (isgrp)
     {
	  /*
	   *  And move to the left of the closing ']'
	   */
	  if (buf[len-1] != ']')
	       goto bad_line;
	  --len;
	  if (!len)
	       /*
		*  No closing ']'
		*/
	       return(ERR_SYNTAX);
	  /*
	   *  Trim trailing LWSP within the [...]
	   */
	  while (len && isspace(buf[len-1]))
	       --len;
	  if (!len)
	       /*
		*  Nothing but LWSP between the square brackets...
		*/
	       return(ERR_SYNTAX);
     }

     /*
      *  Locate text to the left of an "="
      */
     nam = buf;
     while (len && !isspace(*buf) && *buf != '=')
     {
	  buf++;
	  len--;
     }
     nl = buf - nam;
     if (!len)
     {
	  if (isgrp || (grp && (grp->flags & OPT_GFLAGS_EMPTYOK)))
	  {
	       /*
		*  "[group-name]\n" is okay
		*/
	       val = empty_str;
	       vl  = 0;
	       goto done;
	  }
	  /*
	   *  "opt-name\n" is not okay
	   */
	  goto bad_line;
     }

     /*
      *  Skip to the right of the "=" ignoring LWSP on either side of the "="
      */
     if (*buf != '=')
     {
	  buf++;
	  len--;
	  while(len && isspace(*buf))
	  {
	       buf++;
	       len--;
	  }
	  if (!len)
	  {
	       if (isgrp)
	       {
		    /*
		     *  "[group-name]\n" is okay
		     */
		    val = empty_str;
		    vl  = 0;
		    goto done;
	       }
	       else
		    /*
		     *  "opt-name\n" is not okay
		     */
		    goto bad_line;
	  }
	  else if (*buf != '=')
	       goto bad_line;
     }
     /*
      *  Skip over the "="
      */
     buf++;
     len--;

     /*
      *  Skip any LWSP to the right of the "="
      */
     while (len && isspace(*buf))
     {
	  buf++;
	  len--;
     }
     if (!len)
     {
	  if (isgrp)
	       /*
		*  "[group-name=]\n" is not okay
		*/
	       goto bad_line;

	  /*
	   *  "opt-name=\n" is okay
	   */
	  val = empty_str;
	  vl  = 0;
	  goto done;
     }

     /*
      *  Ignore enclosing double quotes, if any
      *  Note this makes possible '[group-name=""]\n' which
      *  may be at odds with disallowing '[group-name=]\n' previously
      */
     if (len >= 2 && buf[0] == '"' && buf[len-1] == '"')
     {
	  buf += 1;
	  len -= 2;
     }
     /*
      *  And the rest is the value
      */
     val = buf;
     vl  = len;

done:
     if (name)
	  *name = nam;
     if (nlen)
	  *nlen = nl;
     if (valu)
	  *valu = val;
     if (vlen)
	  *vlen = vl;
     if (isgroup)
	  *isgroup = isgrp;

     return(ERR_OK);

bad_line:
     /*
      *  Not really worth a "goto"
      */
     return(ERR_SYNTAX);
}


static void
dispose_groups(opt_group_t *groups)
{
     opt_group_t *tmpg;

     while (groups)
     {
	  tmpg = groups->next;
	  if (groups->options)
	  {
	       opt_option_t *tmpo1, *tmpo2;

	       tmpo1 = groups->options;
	       while (tmpo1)
	       {
		    tmpo2 = tmpo1->next;
		    free(tmpo1);
		    tmpo1 = tmpo2;
	       }
	  }
	  free(groups);
	  groups = tmpg;
     }
}

#if !defined(__NO_REGEX)

static void
dispose_regex(opt_regex_t *rinfo)
{
     if (!rinfo)
	  return;

     if (rinfo->pat)
     {
	  regfree(&rinfo->cexp);
	  free(rinfo->pat);
	  rinfo->pat = NULL;
     }
     rinfo->plen   = 0;
     rinfo->rflags = 0;
}

#endif

void
opt_dispose(opt_t *opts)
{
     if (opts)
     {
	  dispose_sources(opts->sources);
	  dispose_groups(opts->groups);
#if !defined(__NO_REGEX)
	  dispose_regex(&opts->group_regex);
	  dispose_regex(&opts->option_regex);
#endif
	  opt_init(opts);
     }
}


void
opt_init(opt_t *opts)
{
     if (!opts)
	  return;
     opts->stype               = OPT_STYPE_OPT;
     opts->ssize               = sizeof(opt_t);
     opts->groups              = NULL;
     opts->global_group        = NULL;
     opts->sources             = NULL;
     opts->empty_opts_allowed  = NULL;
#if !defined(__NO_REGEX)
     opts->group_regex.pat     = NULL;
     opts->group_regex.plen    = 0;
     opts->group_regex.rflags  = 0;
     opts->option_regex.pat    = NULL;
     opts->option_regex.plen   = 0;
     opts->option_regex.rflags = 0;
#endif
}


int
opt_set(opt_t *opts, int item_code, ...)
{
     va_list ap;
     int istat;

     if (!opts)
	  return(ERR_BADARGS);

     va_start(ap, item_code);

next:
     switch (item_code)
     {
     case 0 :
	  goto done;

     default :
	  istat = ERR_NO;
	  break;

     case OPT_ITEM_EMPTY_ALLOWED :
	  opts->empty_opts_allowed = va_arg(ap, const char *);
	  break;
     }
     if (istat != ERR_OK)
	  goto done;
     item_code = va_arg(ap, int);
     goto next;

done:
     va_end(ap);
     return(istat);
}

static opt_group_t *
add_group(opt_t *opts, const char *name, size_t nlen, const char *valu,
	  size_t vlen, opt_source_t *source, int lineno, int flags,
	  int *errcode)
{
     opt_group_t *tmpg, *tmpg_last;

     /*
      *  Sanity checks
      */
     if (!name || !valu)
     {
	  debug("add_group(%d): Invalid call arguments supplied;"
		" name=%p, valu=%p", __LINE__, name, valu);
	  if (errcode)
	       *errcode = ERR_BADARGS;
	  return(NULL);
     }
     else if (nlen >= OPT_NAM_LEN || vlen >= OPT_VAL_LEN)
     {
	  debug("add_group(%d): Supplied name or value is too long; nlen=%u, "
		"vlen=%u; nlen must be < %u; vlen must be < %u",
		__LINE__, nlen, vlen, OPT_NAM_LEN, OPT_VAL_LEN);
	  if (errcode)
	       *errcode = ERR_TOOLONG;
	  return(NULL);
     }

     /*
      *  See if this group has been specified before
      */
     tmpg = opts ? opts->groups : NULL;
     if (tmpg)
     {
	  while (tmpg)
	  {
	       if (nlen == tmpg->nlen && vlen == tmpg->vlen &&
		   0 == memcmp(name, tmpg->name, nlen) &&
		   0 == memcmp(valu, tmpg->valu, vlen))
		    /*
		     *  Found it
		     */
		    return(tmpg);
	       tmpg_last = tmpg;
	       tmpg = tmpg->next;
	  }
     }
     else
	  tmpg_last = NULL;

     /*
      *  Check our flags to see if we want to actually add this group
      */
     if (!(flags & OPT_FLAGS_ADD))
     {
	  /*
	   *  Unable to locate this group AND we're not supposed to add it
	   *  [May be that this call was just made to lookup the group]
	   */
	  debug("add_group(%d): Option does not exist and we are prohibited "
		"from adding it as flags does not have the OPT_FLAGS_ADD bit "
		"set; flags=%x", __LINE__, flags);
	  if (errcode)
	       *errcode = ERR_NO;
	  return(NULL);
     }

     /*
      *  New group
      */
     tmpg = (opt_group_t *)malloc(sizeof(opt_group_t));
     if (!tmpg)
     {
	  debug("add_group(%d): Insufficient virtual memory", __LINE__);
	  if (errcode)
	       *errcode = ERR_NOMEM;
	  return(NULL);
     }
     if (tmpg_last)
	  tmpg_last->next = tmpg;
     tmpg->stype   = OPT_STYPE_GROUP;
     tmpg->ssize   = sizeof(opt_group_t);
     tmpg->next    = NULL;
     tmpg->options = NULL;
     tmpg->parent  = NULL;
     tmpg->used    = 0;
     tmpg->source  = source;
     tmpg->lineno  = source ? lineno : 0;
     tmpg->nlen    = nlen;
     if (tmpg->nlen)
	  memcpy(tmpg->name, name, tmpg->nlen);
     tmpg->name[tmpg->nlen] = '\0';
     tmpg->vlen = vlen;
     if (tmpg->vlen)
	  memcpy(tmpg->valu, valu, tmpg->vlen);
     tmpg->valu[tmpg->vlen] = '\0';

     /*
      *  Group allows options with no "=" following?
      */
     if (opts->empty_opts_allowed)
     {
	  const char *ae = opts->empty_opts_allowed;
	  int matched = 0;

	  /*
	   *  Ignore any leading |
	   */
	  while(*ae && (*ae == '|' || isspace(*ae)))
	       ae++;
	  if (!(*ae))
	       goto done;

	  /*
	   *  See if we get a match first thing
	   */
	  if (!strncasecmp(ae, tmpg->name, tmpg->nlen) &&
	      (ae[tmpg->nlen] == '\0' || ae[tmpg->nlen] == '|' ||
	       isspace(ae[tmpg->nlen])))
	  {
	       matched = 1;
	       goto done;
	  }


	  /*
	   *  Loop looking for a match [strcasestr() would be useful here]
	   */
	  while (*ae && (ae = strchr(ae, '|')))
	  {
	       /*
		*  Skip over the leading '|' and any LWSP
		*/
	       while (*ae && (*ae == '|' || isspace(*ae)))
		    ae++;

	       if (*ae && !strncasecmp(ae, tmpg->name, tmpg->nlen) &&
		   (ae[tmpg->nlen] == '\0' || ae[tmpg->nlen] == '|' ||
		    isspace(ae[tmpg->nlen])))
	       {
		    matched = 1;
		    break;
	       }
	  }
     done:
	  if (matched)
	       tmpg->flags |= OPT_GFLAGS_EMPTYOK;
	  else
	       tmpg->flags &= ~OPT_GFLAGS_EMPTYOK;
     }

     /*
      *  All done
      */
     return(tmpg);
}


static int
add_option(opt_group_t *group, const char *name, size_t nlen, const char *valu,
	   size_t vlen, opt_source_t *source, int lineno, int flags)
{
     opt_option_t *tmpo, *tmpo_last;

     /*
      *  Sanity checks
      */
     if (!group || !name || !valu)
     {
	  debug("add_option(%d): Invalid call arguments supplied; group=%p, "
		"name=%p, valu=%p", __LINE__, group, name, valu);
	  return(ERR_BADARGS);
     }
     else if (nlen >= OPT_NAM_LEN || vlen >= OPT_VAL_LEN)
     {
	  debug("add_option(%d): Supplied name or value is too long; "
		"nlen=%u, vlen=%u; nlen must be < %u; vlen must be < %u",
		__LINE__, nlen, vlen, OPT_NAM_LEN, OPT_VAL_LEN);
	  return(ERR_TOOLONG);
     }

     /*
      *  See if this option has been specified before
      */
     tmpo_last = NULL;
     tmpo = group->options;
     if (tmpo)
     {
	  while (tmpo)
	  {
	       if (nlen == tmpo->nlen && 0 == memcmp(name, tmpo->name, nlen))
	       {
		    /*
		     *  Found it
		     */
		    if (flags & OPT_FLAGS_NOREPLACE)
		    {
			 /*
			  *  Prohibited from replacing
			  */
			 debug("add_option(%d): Option already exists and "
			       "flags have OPT_FLAGS_NOREPLACE bit set; "
			       "flags=%x", __LINE__, flags);
			 return(ERR_NO);
		    }
		    /*
		     *  Replace the values
		     */
		    goto set_vals;
	       }
	       tmpo_last = tmpo;
	       tmpo = tmpo->next;
	  }
     }


     /*
      *  Check our flags to see if we want to actually add this option
      */
     if (!(flags & OPT_FLAGS_ADD))
     {
	  /*
	   *  Unable to locate this option AND we're not supposed to add it
	   *  [May be that this call was just made to replace the option]
	   */	
	  debug("add_option(%d): Option does not exist and we are prohibited "
		"from adding it as flags does not have the OPT_FLAGS_ADD bit "
		"set; flags=%x", __LINE__, flags);
	  return(ERR_NO);
     }

     /*
      *  New option
      */
     tmpo = (opt_option_t *)malloc(sizeof(opt_option_t));
     if (!tmpo)
     {
	  debug("add_option(%d): Insufficient virtual memory", __LINE__);
	  return(ERR_NOMEM);
     }
     tmpo->stype  = OPT_STYPE_OPTION;
     tmpo->ssize  = sizeof(opt_option_t);
     tmpo->parent = group;
     tmpo->next   = NULL;
     if (tmpo_last)
	  tmpo_last->next = tmpo;
     else
	  group->options = tmpo;

set_vals:
     tmpo->used   = 0;
     tmpo->source = source;
     tmpo->lineno = source ? lineno : 0;
     tmpo->nlen   = nlen;
     if (tmpo->nlen)
	  memcpy(tmpo->name, name, tmpo->nlen);
     tmpo->name[tmpo->nlen] = '\0';
     tmpo->vlen = vlen;
     if (tmpo->vlen)
	  memcpy(tmpo->valu, valu, vlen);
     tmpo->valu[tmpo->vlen] = '\0';

     /*
      *  All done
      */
     return(ERR_OK);
}


static opt_group_t *
opt_read_inner(opt_t *opts, opt_group_t *current_group, size_t depth,
	       const char *fname, size_t flen, int *fexists, int *errcode)
{
     const char *buf, *ptr;
     size_t buflen, len, nlen, vlen;
     char *fbuf;
     int fd, is_badconfig, is_group, istat, lineno;
     const char *fdata, *name, *value;
     struct stat sb;
     opt_source_t *src;

     /*
      *  Sanity check
      */
     if (!opts || !fname || !flen)
     {
	  debug("opt_read_inner(%d): Bad call arguments supplied; opts=%p, "
		"fname=%p, flen=%u", __LINE__, opts, fname, flen);
	  if (errcode)
	       *errcode = ERR_BADARGS;
	  return(NULL);
     }

     /*
      *  Initializations
      */
     if (!current_group)
	  current_group = opts->groups;
     fd           = -1;
     is_badconfig = 0;
     fbuf         = NULL;
     fdata        = NULL;

     /*
      *  Build a NUL terminated version of the file name.
      *  Note: our file names may be non NUL terminated strings
      *  living in read-only memory such as a memory mapped
      *  configuration file.
      */
     fbuf = (char *)malloc(flen + 1);
     if (!fbuf)
     {
	  debug("opt_read_inner(%d): Insufficient virtual memory", __LINE__);
	  istat = ERR_NOMEM;
	  goto badness;
     }
     memcpy(fbuf, fname, flen);
     fbuf[flen] = '\0';

     /*
      *  Does the option file even exist?
      */
     if (access(fbuf, F_OK))
     {
	  /*
	   *  For the time being, don't tolerate non-existence
	   *  of included files.  However, non-existent include
	   *  files is a useful means of having a default, distributed
	   *  configuration file which then references an optional,
	   *  site-supplied file.
	   */
	  if (fexists)
	       *fexists = 0;
	  if (depth)
	  {
	       debug("opt_read_inner(%d): Include file \"%s\" does not exist; "
		     "errno=%d; %s", __LINE__, fbuf, errno, strerror(errno));
	       istat = ERR_NO;
	  }
	  else
	       istat = ERR_OK;
	  goto badness;
     }
     else if (fexists)
	  *fexists = 1;

     /*
      *  Open the file
      */
     fd = open(fbuf, O_RDONLY, 0);
     if (fd < 0)
     {
	  debug("opt_read_inner(%d): Unable to open the file \"%s\"; errno=%d;"
		" %s", __LINE__, fbuf, errno, strerror(errno));
	  istat = ERR_NO;
	  goto badness;
     }

     /*
      *  Determine the file size
      */
     if (fstat(fd, &sb))
     {
	  debug("opt_read_inner(%d): Unable to determine the size of the "
		"file \"%s\"; errno=%d; %s",
		__LINE__, fbuf, errno, strerror(errno));
	  istat = ERR_NO;
	  goto badness;
     }
     else if (!sb.st_size)
     {
	  /*
	   *  Empty file
	   */
	  istat = ERR_OK;
	  goto badness;
     }

     /*
      *  mmap() the file
      */
     if (!(fdata = (const char *)mmap(NULL, sb.st_size, PROT_READ,
				      MAP_FLAGS, fd, 0)))
     {
	  debug("opt_read_inner(%d): Unable to memory map the file \"%s\"; "
		"errno=%d; %s", __LINE__, fbuf, errno, strerror(errno));
	  istat = ERR_NO;
	  goto badness;
     }

     /*
      *  Check for an embedded NUL.  Using strlen() would be more
      *  efficient but this read-only, memory mapped file isn't obliging.
      */
     name = memchr(fdata, '\0', sb.st_size);
     if (name)
     {
	  nlen = 1 + name - fdata;
	  debug("opt_read_inner(%d): The file \"%s\" contains a NUL byte "
		"(0x00) at byte %u; unwilling to parse the file",
		__LINE__, fbuf, nlen);
	  istat = ERR_NO;
	  goto badness;
     }

     /*
      *  Record this source in the source list
      */
     if (ERR_OK == add_source(opts, fbuf, flen))
	  /*
	   *  In case of an error, just don't bother with noting the source
	   */
	  src = opts->sources;
     else
	  src = NULL;

     /*
      *  Now loop, reading our option file from memory
      */
     lineno  = 0;
     buf     = fdata;
     buflen  = sb.st_size;
     while(buflen)    
     {
	  /*
	   *  Locate the end of the next record
	   */
	  ++lineno;
	  ptr = memchr(buf, ch_line_term, buflen);
	  len = ptr ? ptr - buf : buflen;

	  /*
	   *  Include file?
	   */
	  if (buflen > 1 && buf[0] == '<')
	  {
	       size_t flen;
	       const char *fptr;
	       int idummy;

	       /*
		*  "<" [ LWSP ] fname-spec [ LWSP ]
		*  buf points at leading "<"
		*  len is entire line
		*/
	       fptr = buf + 1;
	       flen = len - 1;

	       /*
		*  Ignore any leading LWSP
		*/
	       while (flen && isspace(*fptr))
	       {
		    fptr++;
		    flen--;
	       }
	       if (flen)
	       {
		    /*
		     *  Ignore any trailing LWSP
		     */
		    while (flen && isspace(fptr[flen-1]))
			 flen--;
	       }
	       if (!flen)
	       {
		    is_badconfig = 1;
		    debug("read_opts(%d): Error parsing line %d of the %s "
			  "file \"%s\"; the name of the file to include is "
			  "missing",
			  __LINE__, lineno, depth ? "option" : "include",
			  fbuf);
		    goto next_line;
	       }
	       current_group = opt_read_inner(opts, current_group, depth + 1,
					      fptr, flen, &idummy, &istat);
	       if (!current_group)
		    goto badness;
	  }
	  else
	  {
	       istat = parse_line(current_group, &name, &nlen, &value, &vlen,
				  &is_group, buf, len);
	       if (istat == ERR_OK)
	       {
		    if (is_group)
		    {
			 if (!(current_group = add_group(opts,
							 name, nlen,
							 value, vlen,
							 src, lineno,
							 OPT_FLAGS_ADD,
							 &istat)))
			      /*
			       *  Could be a coding error, but most likely
			       *  a lack of virtual memory.
			       */
			      goto badness;
			 current_group->parent = opts;
		    }
		    else
			 if (ERR_OK != (istat = add_option(
					     current_group, name, nlen,
					     value, vlen, src, lineno,
					     OPT_FLAGS_OVERRIDE)))
			      /*
			       *  Probably a coding error
			       */
			      goto badness;
	       }
	       else if (istat == ERR_EOM)
		    /*
		     *  ERR_EOM ==> Semantically blank line
		     */
		    goto next_line;
	       else if (istat != ERR_SYNTAX)
		    /*
		     *  Error of some sort -- likely a coding error
		     */
		    goto badness;
	       else
	       {
		    /*
		     *  Note that the config is bad, but continue parsing it so
		     *  that we can report any further errors.
		     */
		    is_badconfig = 1;
		    debug("read_opts(%d): Error parsing line %d of the %s "
			  "file \"%s\"",
			  __LINE__, lineno, depth ? "option" : "include",
			  fbuf);
	       }
	  }

     next_line:
	  if (len >= buflen)
	       break;
	  buflen -= len + 1;
	  buf     = ptr + 1;
     }

     /*
      *  All done
      */
     istat = is_badconfig ? ERR_SYNTAX : ERR_OK;

badness:
     if (fdata)
	  munmap((void *)fdata, sb.st_size);
     if (fd >= 0)
	  close(fd);
     if (fbuf)
	  free(fbuf);
     if (errcode)
	  *errcode = istat;
     return((istat == ERR_OK) ? current_group : NULL);
}


int
opt_read(opt_t *opts, const char *fname, int *fexists)
{
     int istat;
     opt_group_t *groups;

     /*
      *  Sanity tests
      */
     if (!opts || !fname)
     {
	  debug("opt_read(%d): Bad call arguments supplied; opts=%p, "
		"fname=%p", __LINE__, opts, fname);
	  return(ERR_BADARGS);
     }

     /*
      *  Establish the global group
      */
     groups = add_group(opts, empty_str, 0, empty_str, 0, 0, 0,
			OPT_FLAGS_ADD, &istat);
     if (!groups)
     {
	  debug("opt_read(%d): Insufficient virtual memory", __LINE__);
	  return(istat);
     }

     /*
      *  Now recursively read the option file and any files it might include
      */
     groups->parent     = opts;
     opts->groups       = groups;
     opts->global_group = groups;
     if (opt_read_inner(opts, groups, 0, fname, fname ? strlen(fname) : 0,
			fexists, &istat) && istat == ERR_OK)
	  return(ERR_OK);
     else
     {
	  /*
	   *  Error of some sort: should have been reported by opt_read_inner()
	   */
	  opt_dispose(opts);
	  return(istat);
     }
}


int
opt_option_push(void *ctx, const char *gname, const char *gval,
		const char *oname, const char *oval, const char *source,
		int lineno, int flags)
{
     opt_t *opts;
     opt_group_t *current_group;
     size_t gnlen, gvlen, ovlen;
     int istat;
     opt_source_t *src;

     /*
      *  Bozo check
      */
     if (!ctx || !oname)
     {
	  debug("opt_option_push(%d): Invalid call arguments supplied; "
		"ctx=%p, oname=%p", __LINE__, ctx, oname);
	  return(ERR_BADARGS);
     }

     /*
      *  Figure out what sort of context we were passed
      */
     opts = (opt_t *)ctx;
     if (opts->stype == OPT_STYPE_WALK &&
	 opts->ssize == sizeof(opt_walk_t) &&
	 ((opt_walk_t *)ctx)->parent && ((opt_walk_t *)ctx)->parent->parent)
	  opts = ((opt_walk_t *)ctx)->parent->parent;
     else if (opts->stype != OPT_STYPE_OPT || opts->ssize != sizeof(opt_t))
     {
	  debug("opt_option_push(%d): Invalid call arguments; ctx=%p does not "
		"appear to be a pointer to a valid opt_t or opt_walk_t "
		"structure", __LINE__, ctx);
	  return(ERR_BADARGS);
     }
	 
     /*
      *  Add the source?
      */
     if (source)
     {
	  /*
	   *  Note that add_source() will be a no-op if this
	   *  source matches the previous one
	   */
	  istat = add_source(opts, source, strlen(source));
	  if (istat != ERR_OK)
	  {
	       debug("opt_option_push(%d): Unable to add source information; "
		     "add_source() returned %d; %s",
		     __LINE__, istat, err_strerror(istat));
	       return(istat);
	  }

	  /*
	   *  Source added to the head of the list
	   */
	  src = opts->sources;
     }
     else
	  src = NULL;

     /*
      *  Initializations
      */
     if (!gname)
     {
	  gname = empty_str;
	  gnlen = 0;
     }
     else
	  gnlen = strlen(gname);

     if (!gval)
     {
	  gval = empty_str;
	  gvlen = 0;
     }
     else
	  gvlen = strlen(gval);

     if (!oval)
     {
	  oval  = empty_str;
	  ovlen = 0;
     }
     else
	  ovlen = strlen(oval);

     /*
      *  Locate the group, adding it if it does not already exist
      */
     current_group = add_group(opts, gname, gnlen, gval, gvlen,
			       src, lineno, OPT_FLAGS_ADD, &istat);
     if (!current_group)
     {
	  debug("opt_option_push(%d): Unable to locate or add the group "
		"[%s=\"%s\"]; add_group() returned %d; %s",
		__LINE__, gname, gval, istat, err_strerror(istat));
	  return(istat);
     }

     /*
      *  And add the option
      */
     istat = add_option(current_group, oname, strlen(oname), oval, ovlen,
			src, lineno, flags);
     if (istat == ERR_OK)
	  return(ERR_OK);
     debug("opt_option_push(%d): Unable to add the option-value pair "
	   "%s=\"%s\" with flags=%d; add_option() returned %d; %s",
	   __LINE__, oname, oval, flags, istat, err_strerror(istat));
     return(istat);
}


static const char *
compare_debug(int match)
{
     static const char *bogus = "**bad matching flags** will never match the "
	  "string";
     static const char *ome   = "exactly matching the case-sensitive string";
     static const char *ombw  = "beginning with the case-sensitive string";
     static const char *omew  = "ending with the case-sensitive string";
     static const char *omen  = "exactly matching the case-insensitive string";
     static const char *ombwn = "beginning with the case-insensitive string";
     static const char *omewn = "ending with the case-insensitive string";
     static const char *omr   = "matching the case-sensitive regular "
	                           "expression";
     static const char *omrn  = "matching the case-insensitive regular "
	                           "expression";

     switch (match & OPT_MATCH_MASK)
     {
     default                    : return(bogus);
     case OPT_MATCH_EXACT       : return(omen);
     case OPT_MATCH_BEGINS_WITH : return(ombwn);
     case OPT_MATCH_ENDS_WITH   : return(omewn);
     case OPT_MATCH_REGEX       : return(omrn);
     case OPT_MATCH_EXACT | OPT_MATCH_CASE       : return(ome);
     case OPT_MATCH_BEGINS_WITH | OPT_MATCH_CASE : return(ombw);
     case OPT_MATCH_ENDS_WITH | OPT_MATCH_CASE   : return(omew);
     case OPT_MATCH_REGEX | OPT_MATCH_CASE       : return(omr);
     }
}


static int
compare(opt_t *opts, const char *s1, size_t l1, const char *s2, size_t l2,
	int match, int isgroup)
{
#if !defined(__NO_REGEX)
     int istat, rflags;
     opt_regex_t *rinfo;
#endif

     switch (match & OPT_MATCH_MASK)
     {
     default :
	  /*
	   *  May never occur as all the bits may now be used!
	   */
	  debug("compare(%d): Bad call arguments supplied; invalid value "
		"supplied for match=%d", __LINE__, match);
	  return(ERR_BADARGS);

     case OPT_MATCH_EXACT :
	  if (l1 != l2 || strncasecmp(s1, s2, l1))
	       return(-1);
	  else
	       return(0);

     case OPT_MATCH_BEGINS_WITH :
	  if (l2 > l1 || strncasecmp(s1, s2, l2))
	       return(-1);
	  else
	       return(0);

     case OPT_MATCH_ENDS_WITH :
	  if (l2 > l1 || strncasecmp(s1 + (l1 - l2), s2, l2))
	       return(-1);
	  else
	       return(0);

     case OPT_MATCH_REGEX :
     case OPT_MATCH_REGEX | OPT_MATCH_CASE :
#if defined(__NO_REGEX)
	  debug("compare(%d): Attempt to use a regular expression match in "
		"code not compiled to support regular expression matching",
		__LINE__);
	  return(ERR_BADARGS);
#else
	  if (!opts)
	  {
	       debug("compare(%d): Bad call arguments; opts=NULL", __LINE__);
	       return(ERR_BADARGS);
	  }
	  rflags = REG_EXTENDED | REG_NOSUB;
	  if (!(match & OPT_MATCH_CASE))
	       rflags |= REG_ICASE;
	  rinfo = isgroup ? &opts->group_regex : &opts->option_regex;
	  if (!rinfo->pat ||
	      rflags != rinfo->rflags ||
	      l2 != rinfo->plen ||
	      memcmp(s2, rinfo->pat, l2))
	  {
	       if (rinfo->pat)
	       {
		    regfree(&rinfo->cexp);
		    free(rinfo->pat);
	       }
	       rinfo->pat = malloc(l2 + 1);
	       if (!rinfo->pat)
	       {
		    debug("compare(%d): Insufficient virtual memory",
			  __LINE__);
		    return(ERR_NOMEM);
	       }
	       memcpy(rinfo->pat, s2, l2);
	       rinfo->pat[l2] = '\0';
	       rinfo->plen = l2;
	       rinfo->rflags = rflags;
	       if ((istat=regcomp(&rinfo->cexp, rinfo->pat, rinfo->rflags)))
	       {
		    if (do_debug)
		    {
			 char buf[256];
			 regerror(istat, &rinfo->cexp, buf, sizeof(buf));
			 debug("compare(%d): Unable to compile the regular "
			       "expression \"%s\"; regcomp() returned %d; %s",
			       __LINE__, rinfo->pat, istat, buf);
		    }
		    free(rinfo->pat);
		    rinfo->pat = NULL;
		    return(ERR_NO);
	       }
	  }
	  istat = regexec(&rinfo->cexp, s1, 0, NULL, 0);
	  return (istat ? -1 : 0);
#endif

     case OPT_MATCH_EXACT | OPT_MATCH_CASE :
	  if (l1 != l2 || memcmp(s1, s2, l1))
	       return(-1);
	  else
	       return(0);

     case OPT_MATCH_BEGINS_WITH | OPT_MATCH_CASE :
	  if (l2 > l1 || memcmp(s1, s2, l2))
	       return(-1);
	  else
	       return(0);

     case OPT_MATCH_ENDS_WITH | OPT_MATCH_CASE :
	  if (l2 > l1 || memcmp(s1 + (l1 - l2), s2, l2))
	       return(-1);
	  else
	       return(0);
     }
}


int
opt_group_walk(opt_t *opts, const char *group_name, int flags,
		opt_group_walk_callback_t *callback_proc, void *callback_ctx)
{
     size_t glen;
     opt_group_t *groups;
     int istat;
     opt_walk_t our_ctx;

     /*
      *  Sanity checks
      */
     if (!opts || !callback_proc)
     {
	  debug("opt_group_walk(%d): Invalid call arguments supplied; "
		"opts=%p, callback_proc=%p, flags=%d",
		__LINE__, opts, callback_proc);
	  return(ERR_BADARGS);
     }

     /*
      *  Interpret group_name == NULL as a request to list all groups *when*
      *  flags != OPT_MATCH_EXACT.
      */
     if (!group_name)
     {
	  group_name = empty_str;
	  glen = 0;
     }
     else
	  glen = strlen(group_name);

     /*
      *  Initialize our context
      */
     our_ctx.stype          = OPT_STYPE_WALK;
     our_ctx.ssize          = sizeof(opt_walk_t);
     our_ctx.local_options  = NULL;
     our_ctx.global_options =
	  opts->global_group ? &opts->global_group->options : NULL;

     /*
      *  Now walk the list of option groups looking for a matching group
      *  name.  Skip the global options.
      */
     groups = opts->groups;
     while (groups)
     {
	  if (!(istat = compare(opts, groups->name, groups->nlen, group_name,
				glen, flags, 1)))
	  {
	       our_ctx.parent        = groups;
	       our_ctx.local_options = &groups->options;
	       our_ctx.ogbldone      = 0;
	       our_ctx.opos          = 0;
	       our_ctx.onext         = NULL;
	       istat = (*callback_proc)(
		                 callback_ctx, (void *)&our_ctx,
				 (const char *)groups->name, groups->nlen,
				 (const char *)groups->valu, groups->vlen);
	       if (istat != ERR_OK)
	       {
		    debug("opt_group_walk(%d): The caller-supplied callback "
			  "procedure %p returned an error status of %d",
			  __LINE__, callback_proc, istat);
		    return(ERR_ABORT);
	       }
	  }
	  else if (istat > 0)
	       return(istat);
	  groups = groups->next; 
     }

     /*
      *  If we didn't find any matching groups AND OPT_MATCH_GLOBAL_FALLBACK
      *  is set, then pretend that we found one group and that that group
      *  is the global group.
      */
     if (our_ctx.local_options || !(flags & OPT_MATCH_GLOBAL_FALLBACK))
	  return(ERR_OK);

     istat = (*callback_proc)(callback_ctx, (void *)&our_ctx,
			      empty_str, 0, empty_str, 0);
     if (istat == ERR_OK)
	  return(istat);

     debug("opt_group_walk(%d): The caller-supplied callback procedure %p "
	   "returned an error status of %d", __LINE__, callback_proc, istat);
     return(ERR_ABORT);
}


static const opt_option_t *
opt_get(void *ctx, const char *name, size_t nlen, int match, int *errcode)
{
     opt_group_t *group;
     int istat;
     opt_t *opts;
     opt_walk_t *our_ctx = (opt_walk_t *)ctx;
     const opt_option_t *tmpg, *tmpo;

     if (!ctx || !name || !nlen)
     {
	  debug("opt_get(%d): Invalid call arguments supplied; ctx=%p, "
		"name=%p, nlen=%u", __LINE__, ctx, name, nlen);
	  if (errcode)
	       *errcode = ERR_BADARGS;
	  return(NULL);
     }

     /*
      *  See what sort of pointer we were given...
      */
     opts = NULL;
     tmpo = NULL;
     tmpg = NULL;
     switch(our_ctx->stype)
     {
     default :
	  debug("get_opt(%d): Invalid call arguments supplied; the context "
		"ctx=%p does not appear to be a valid context; ctx->stype=%d "
		"is unknown", __LINE__, our_ctx, our_ctx->stype);
	  if (errcode)
	       *errcode = ERR_BADARGS;
	  return(NULL);

     case OPT_STYPE_WALK :
	  if (our_ctx->ssize != sizeof(opt_walk_t))
	       goto bad_size;

	  if (our_ctx->local_options)
	       tmpo = *our_ctx->local_options;
	  if (our_ctx->global_options)
	       tmpg = *our_ctx->global_options;
	  if (our_ctx->parent)
	       opts = our_ctx->parent->parent;
	  break;

     case OPT_STYPE_OPT :
	  if (our_ctx->ssize != sizeof(opt_t))
	       goto bad_size;
	  opts = (opt_t *)ctx;
	  if (opts->global_group)
	       tmpg = opts->global_group->options;
	  break;

     case OPT_STYPE_GROUP :
	  if (our_ctx->ssize != sizeof(opt_group_t))
	       goto bad_size;
	  group = (opt_group_t *)ctx;
	  tmpo = group->options;
	  if (group->parent)
	  {
	       opts = group->parent;
	       if (opts->global_group)
		    tmpg = opts->global_group->options;
	  }
	  break;

     case OPT_STYPE_OPTION :
	  if (our_ctx->ssize != sizeof(opt_option_t))
	       goto bad_size;
	  tmpo = (opt_option_t *)ctx;
	  if (tmpo->parent)
	  {
	       tmpo = tmpo->parent->options;
	       if (tmpo->parent && tmpo->parent->parent)
	       {
		    opts = tmpo->parent->parent;
		    if (opts->global_group)
			 tmpg = opts->global_group->options;
	       }
	  }
	  break;
     }

     /*
      *  First walk the local options
      */
     while (tmpo)
     {
	  if (!(istat = compare(opts, tmpo->name, tmpo->nlen, name, nlen,
				match, 0)))
	       return(tmpo);
	  else if (istat > 0)
	  {
	       if (errcode)
		    *errcode = istat;
	       return(NULL);
	  }
	  tmpo = tmpo->next;
     }

     /*
      *  Didn't find anything?  Then walk the global options
      */
     if (!(match & OPT_MATCH_NOGLOBAL))
     {
	  while (tmpg)
	  {
	       if (!(istat = compare(opts, tmpg->name, tmpg->nlen, name, nlen,
				     match, 1)))
		    return(tmpg);
	       else if (istat > 0)
	       {
		    if (errcode)
			 *errcode = istat;
		    return(NULL);
	       }
	       tmpg = tmpg->next;
	  }
     }

#if 0
     /*
      *  Nada
      */
     debug("opt_get(%d): Unable to locate an option %s \"%.*s\"; match=%d",
	   __LINE__, compare_debug(match), nlen, name, match);
#endif
     if (errcode)
	  *errcode = ERR_EOM;
     return(NULL);

bad_size:
     debug("opt_get(%d): Invalid call arguments; the context ctx=%p does not "
	   "appear to be valid; ctx->stype=%d and ctx->ssize=%u do not agree",
	   __LINE__, our_ctx, our_ctx->stype, our_ctx->ssize);
     if (errcode)
	  *errcode = ERR_BADARGS;
     return(NULL);
}


int
opt_get_str(void *ctx, const char *name, const char **val, size_t *len,
	    int match)
{
     int istat;
     const opt_option_t *opt;

     opt = opt_get(ctx, name, name ? strlen(name) : 0, match, &istat);
     if (!opt)
	  return(istat);

     if (val)
	  *val = opt->valu;
     if (len)
	  *len = opt->vlen;
     return(ERR_OK);
}


int
opt_get_int(void *ctx, const char *name, int *val, int base, int match)
{
     int ival, istat;
     const opt_option_t *opt;
     char *ptr;

     /*
      *  Base must be either 0 or in the range [2, 36]
      */
     if (base < 0 || base == 1 || base > 36)
     {
	  debug("opt_get_uint(%d): Invalid call arguments supplied; base=%d; "
		"base/radix must be either 0 or in the range [2, 32]",
		__LINE__, base);
	  return(ERR_BADARGS);
     }

     /*
      *  Locate the option
      */
     opt = opt_get(ctx, name, name ? strlen(name) : 0, match, &istat);
     if (!opt)
	  return(istat);

     ptr = NULL;
     ival = (int)strtol(opt->valu, &ptr, base);
     if (!ptr || ptr == opt->valu)
     {
	  debug("opt_get_int(%d): Unable to parse the string \"%s\" as a "
		"signed integer in base %d", __LINE__, opt->valu, base);
	  return(ERR_SYNTAX);
     }
     if (val)
	  *val = ival;
     return(ERR_OK);
}


int
opt_get_uint(void *ctx, const char *name, unsigned int *val, int base,
	     int match)
{
     unsigned int uval;
     int istat;
     const opt_option_t *opt;
     char *ptr;

     /*
      *  Base must be either 0 or in the range [2, 36]
      */
     if (base < 0 || base == 1 || base > 36)
     {
	  debug("opt_get_uint(%d): Invalid call arguments supplied; base=%d; "
		"base/radix must be either 0 or in the range [2, 32]",
		__LINE__, base);
	  return(ERR_BADARGS);
     }

     /*
      *  Locate the option
      */
     opt = opt_get(ctx, name, name ? strlen(name) : 0, match, &istat);
     if (!opt)
	  return(istat);

     ptr = NULL;
     uval = (int)strtoul(opt->valu, &ptr, base);
     if (!ptr || ptr == opt->valu)
     {
	  debug("opt_get_uint(%d): Unable to parse the string \"%s\" as an "
		"unsigned integer in base %d", __LINE__, opt->valu, base);
	  return(ERR_SYNTAX);
     }
     if (val)
	  *val = uval;
     return(ERR_OK);
}



static const char *
report(char *buf, size_t buflen, const char *name, size_t nlen,
       const char *valu, size_t vlen, const char *source, size_t slen,
       int lineno)
{
     const char *ptr;

     if (!buf)
	  return(empty_str);

     if (!name)
     {
	  name = empty_str;
	  nlen = 0;
     }
     else if (nlen > 32)
	  nlen = 32;
     if (source)
     {
	  ptr = source;
	  if (slen > 48)
	  {
	       /*
		*  Show only the last 48 bytes
		*/
	       ptr += slen - 48;
	       slen = 48;
	  }
	  if (lineno > 0)
	       snprintf(buf, buflen, "line %d of %.*s", lineno, (int)slen, ptr);
	  else
	       snprintf(buf, buflen, "option %.*s from %.*s",
			(int)nlen, name, (int)slen, ptr);
     }
     else
     {
	  if (!valu)
	  {
	       valu = empty_str;
	       vlen = 0;
	       ptr  = empty_str;
	  }
	  else if (vlen > 32)
	  {
	       vlen = 32;
	       ptr = "...";
	  }
	  snprintf(buf, buflen, "option=value pair %.*s=\"%.*s%s\"",
		   (int)nlen, name, (int)vlen, valu, ptr);
     }
     return(buf);
}


int
opt_bulkload_init(opt_bulkload_t *bdata, void *whatever)
{
     int istat;

     if (!bdata || !whatever)
     {
	  debug("opt_bulkload_init(%d): Invalid call arguments supplied; "
		"bdata=%p, whatever=%p", __LINE__, bdata, whatever);
	  return(ERR_BADARGS);
     }

     istat = ERR_OK;
     while (bdata->name)
     {
	  bdata->offset = (unsigned char *)bdata->ptr -
	       (unsigned char *)whatever;
	  if (!bdata->nlen)
	       bdata->nlen = strlen(bdata->name);
	  if (bdata->dtype < OPT_DTYPE_FIRST || bdata->dtype > OPT_DTYPE_LAST)
	  {
	       debug("opt_bulkload_init(%d): Invalid data type %d specified "
		     "for the option \"%.*s\"",
		     __LINE__, bdata->dtype, bdata->nlen, bdata->name);
	       istat = ERR_NO;
	  }
	  else
	  {
	       if (bdata->dtype >= OPT_DTYPE_STRING_FIRST &&
		   bdata->dtype <= OPT_DTYPE_STRING_LAST)
	       {
		    if (!bdata->width)
		    {
			 debug(
"opt_bulkload_init(%d): Maximum string length of zero specified for the "
"option \"%.*s\"; maxlen must be non-zero",
__LINE__, bdata->nlen, bdata->name);
			 istat = ERR_NO;
		    }
	       }
	       else if (bdata->dtype >= OPT_DTYPE_NUMERIC_FIRST &&
			bdata->dtype <= OPT_DTYPE_NUMERIC_LAST)
	       {
		   if (bdata->base < 0 ||
		       bdata->base == 1 || bdata->base > 36)
		   {
			debug(
"opt_bulkload_init(%d): Invalid radix/base specified for the option \"%.*s\"; "
"base=%d; base must be either 0 or in the range [2,36]",
__LINE__, bdata->nlen, bdata->name, bdata->base);
			istat = ERR_NO;
		   }
	       }
	  }
	  bdata++;
     }
     return(istat);
}


int
opt_bulkload(void *ctx, opt_bulkload_t *bdata, void *whatever, int match)
{
     float *float_ptr;
     gid_t *gid_ptr;
     int *int_ptr, istat;
     long long_val;
     opt_option_t *opt;
     char *ptr;
     short *short_ptr;
     char *str_ptr;
     uid_t *uid_ptr;
     unsigned int *uint_ptr;
     unsigned long ulong_val;
     unsigned short *ushort_ptr;
     size_t vlen;

     if (!ctx || !bdata || !whatever)
     {
	  debug("opt_bulkload(%d): Invalid call arguments supplied; ctx=%p, "
		"bdata=%d, whatever=%p", __LINE__, ctx, bdata, whatever);
	  return(ERR_BADARGS);
     }

     /*
      *  There's two ways to handle this load:
      *
      *  1. Loop over the list of site-supplied options.
      *     For each site-supplied option:
      *       a. Locate the corresponding option name in bdata,
      *       b. Set the associated field in whatever if a match
      *          is found in step a, or
      *       c. Report a warning that an unknown option has been
      *          specified.
      *
      *       Upside: Unrecognized options appearing in the option file
      *               can be flagged.
      *     Downside: Doesn't pick up global defaults specified outside
      *               of the current option group.
      *
      *  2. Loop over the caller-supplied list of known options
      *     For each known option:
      *       a. Locate a the matching group-specific, or global
      *          option specified in the site-supplied option file.
      *       b. Set the associated field in whatever if a match is
      *          found in step a, or
      *       c. Move on if no match is found.
      *
      *       Upside: Gets global defaults when a group-specific setting
      *               does not exist.
      *     Downside: Does not spot unrecognized options specified in the
      *               site-supplied option file.
      *
      *  We here implement method 2 BUT we also note which options have
      *  been used from the list of site-supplied options.  Then, we can
      *  warn about unused options on the assumption that they were
      *  unrecognized.  This is controlled via a flag.
      */
     while (bdata->name)
     {
	  /*
	   *  Find the group-specific or global option matching this option
	   */
	  if ((opt = (opt_option_t *)
	       opt_get(ctx, bdata->name, bdata->nlen, match, &istat)))
	  {
	       /*
		*  Note that this option has been referenced
		*/
	       opt->used = 1;
	       if (opt->parent)
		    opt->parent->used = 1;

	       /*
		*  Parse the option's value
		*/
	       if (bdata->proc)
	       {
		    istat = (*bdata->proc)(
			 bdata->ctx,
			 (void *)((char *)whatever + bdata->offset),
			 bdata->width, opt->valu, opt->vlen, opt, bdata);
		    if (istat != ERR_OK)
			 goto done_bad;
	       }
	       else
	       {
		    switch (bdata->dtype)
		    {
		    case OPT_DTYPE_FLOAT :
			 float_ptr = (float *)((char *)whatever + bdata->offset);
			 if (1 != sscanf(opt->valu, "%f", float_ptr))
			      goto syntax_err;
			 break;

		    case OPT_DTYPE_GID :
			 gid_ptr = (gid_t *)((char *)whatever + bdata->offset);
			 ptr = NULL;
			 ulong_val = strtoul(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 if (ulong_val != (unsigned long)((gid_t)ulong_val))
			      goto range_err;
			 *gid_ptr = (gid_t)ulong_val;
			 break;

		    case OPT_DTYPE_INT :
			 int_ptr = (int *)((char *)whatever + bdata->offset);
			 ptr = NULL;
			 long_val = strtol(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 *int_ptr = (int)long_val;
			 break;

		    case OPT_DTYPE_SHORT :
			 short_ptr =
			      (short *)((char *)whatever + bdata->offset);
			 ptr = NULL;
			 long_val = strtol(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 if (long_val != (0xffff & long_val))
			      goto length_err;
			 *short_ptr = (short)(0xffff & long_val);
			 break;
      
		    case OPT_DTYPE_STRING :
			 vlen = opt->vlen;
			 if (vlen >= bdata->width)
			 {
			      if (bdata->base)
				   vlen = bdata->width ?
					bdata->width - 1 : 0;
			      else
				   goto length_err;
			 }
			 str_ptr = (char *)whatever + bdata->offset;
			 memcpy(str_ptr, opt->valu, vlen);
			 str_ptr[vlen] = '\0';
			 break;

		    case OPT_DTYPE_UID :
			 uid_ptr = (uid_t *)((char *)whatever + bdata->offset);
			 ptr = NULL;
			 ulong_val = strtoul(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 if (ulong_val != (unsigned long)((uid_t)ulong_val))
			      goto range_err;
			 *uid_ptr = (uid_t)ulong_val;
			 break;

		    case OPT_DTYPE_UINT :
			 uint_ptr = (unsigned int *)
			      ((char *)whatever + bdata->offset);
			 ptr = NULL;
			 ulong_val = strtoul(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 if (ulong_val !=
			     (unsigned long)((unsigned int)ulong_val))
			      goto range_err;
			 *uint_ptr = (unsigned int)ulong_val;
			 break;

		    case OPT_DTYPE_USHORT :
			 ushort_ptr = (unsigned short *)
			      ((char *)whatever + bdata->offset);
			 ptr = NULL;
			 ulong_val = strtoul(opt->valu, &ptr, bdata->base);
			 if (!ptr || ptr == opt->valu)
			      goto syntax_err;
			 if (ulong_val != (0xffff & ulong_val))
			      goto length_err;
			 *ushort_ptr = (unsigned short)(0xffff & ulong_val);
			 break;
      
		    default :
			 /*
			  *  If we're here, the caller either never called
			  *  opt_bulkload_init() OR they did but ignored
			  *  the error return status that resulted.
			  */
			 debug("opt_bulkload(%d): Unrecognized data type %d "
			       "specified for the option \"%.*s\"",
			       __LINE__, bdata->dtype,
			       bdata->nlen, bdata->name);
			 return(ERR_NO);
		    }
	       }
	  }
	  bdata++;
     }

     /*
      *  All done
      */
     return(ERR_OK);

done_bad:
     if (istat == ERR_SYNTAX)
	  goto syntax_err;
     else if (istat == ERR_RANGE)
	  goto range_err;
     else if (istat == ERR_TOOLONG)
	  goto length_err;

generic_err:
     if (do_debug)
     {
	  char buf[256];
	  debug("opt_bulkload(%d): Unable to parse; %s",
		__LINE__, report(buf, sizeof(buf),
				 opt->name, opt->nlen,
				 opt->valu, opt->vlen,
				 opt->source ? opt->source->source : NULL,
				 opt->source ? opt->source->slen : 0,
				 opt->lineno));
     }
     return(istat);

syntax_err:
     if (do_debug)
     {
	  char buf[256];
	  debug("opt_bulkload(%d): Unable to parse option value; %s",
		__LINE__,
		report(buf, sizeof(buf), opt->name, opt->nlen,
		       opt->valu, opt->vlen,
		       opt->source ? opt->source->source : NULL,
		       opt->source ? opt->source->slen : 0,
		       opt->lineno));
     }
     return(ERR_SYNTAX);

range_err:
     if (do_debug)
     {
	  char buf[256];
	  debug("opt_bulkload(%d): Invalid range for option value; %s",
		__LINE__,
		report(buf, sizeof(buf), opt->name, opt->nlen,
		       opt->valu, opt->vlen,
		       opt->source ? opt->source->source : NULL,
		       opt->source ? opt->source->slen : 0,
		       opt->lineno));
     }
     return(ERR_RANGE);

length_err:
     if (do_debug)
     {
	  char buf[256];
	  debug("opt_bulkload(%d): Value is too long; the maximum length is "
		"%u bytes; %s",
		__LINE__, bdata->width ? bdata->width - 1 : 0,
		report(buf, sizeof(buf), opt->name, opt->nlen,
		       opt->valu, opt->vlen,
		       opt->source ? opt->source->source : NULL,
		       opt->source ? opt->source->slen : 0,
		       opt->lineno));
     }
     return(ERR_TOOLONG);
}


const char *
opt_source(void *ctx)
{
     opt_t *octx;
     opt_option_t *ooctx;
     opt_group_t *ogctx;
     opt_walk_t *owctx;

     if (!ctx)
	  return(NULL);

     /*
      *  Figure out what sort of context we have in hand
      */
     octx = (opt_t *)ctx;
     switch (octx->stype)
     {
     default :
	  return(NULL);

     case OPT_STYPE_OPT :
	  if (octx->ssize != sizeof(opt_t))
	       return(NULL);
	  /*
	   *  Let's just return the most recent source
	   */
	  return(octx->sources ? octx->sources->source : NULL);

     case OPT_STYPE_GROUP :
	  if (octx->ssize != sizeof(opt_group_t))
	       return(NULL);
	  ogctx = (opt_group_t *)ctx;
	  return(ogctx->source ? ogctx->source->source : NULL);

     case OPT_STYPE_OPTION :
	  if (octx->ssize != sizeof(opt_option_t))
	       return(NULL);
	  ooctx = (opt_option_t *)ctx;
	  return(ooctx->source ? ooctx->source->source : NULL);

     case OPT_STYPE_WALK :
	  if (octx->ssize != sizeof(opt_walk_t))
	       return(NULL);
	  owctx = (opt_walk_t *)ctx;
	  return((owctx->parent && owctx->parent->source) ?
		 owctx->parent->source->source : NULL);
     }
}


ssize_t
opt_lineno(void *ctx)
{
     opt_option_t *ooctx;
     opt_group_t *ogctx;
     opt_walk_t *owctx;

     if (!ctx)
	  return(0);

     /*
      *  Figure out what sort of context we have in hand
      */
     owctx = (opt_walk_t *)ctx;
     switch (owctx->stype)
     {
     default :
	  return(0);

     case OPT_STYPE_GROUP :
	  if (owctx->ssize != sizeof(opt_group_t))
	       return(0);
	  ogctx = (opt_group_t *)ctx;
	  return(ogctx->lineno);

     case OPT_STYPE_OPTION :
	  if (owctx->ssize != sizeof(opt_option_t))
	       return(0);
	  ooctx = (opt_option_t *)ctx;
	  return(ooctx->lineno);

     case OPT_STYPE_WALK :
	  if (owctx->ssize != sizeof(opt_walk_t))
	       return(0);
	  return(owctx->parent ? owctx->parent->lineno : 0);
     }
}


int
opt_get_start(void *ctx, int flags)
{
     opt_walk_t *owctx = (opt_walk_t *)ctx;

     if (!ctx)
     {
	  return(ERR_BADARGS);
     }
     else if (owctx->stype != OPT_STYPE_WALK ||
	      owctx->ssize != sizeof(opt_walk_t))
     {
	  return(ERR_NO);
     }

     owctx->ogbldone = (flags & OPT_MATCH_NOGLOBAL) ? 1 : 0;
     owctx->onext    = owctx->local_options ? *owctx->local_options : NULL;
     if (!owctx->onext && !owctx->ogbldone)
     {
	  owctx->onext = owctx->global_options ? *owctx->global_options : NULL;
	  owctx->ogbldone = 1;
     }
     owctx->opos = owctx->onext ? 1 : -1;
     return(ERR_OK);
}


int
opt_get_next(void *ctx, const char **oname, size_t *onlen, char **ovalu,
	     size_t *ovlen, const char *name, int flags)
{
     int istat;
     size_t nlen;
     opt_t *opts;
     opt_walk_t *owctx = (opt_walk_t *)ctx;
     opt_option_t *tmpo;

     if (!ctx)
     {
	  debug("opt_get_next(%d): Invalid call arguments supplied; ctx=NULL",
		__LINE__);
	  return(ERR_BADARGS);
     }
     else if (owctx->stype != OPT_STYPE_WALK ||
	      owctx->ssize != sizeof(opt_walk_t))
     {
	  debug("opt_get_next(%d): Invalid call arguments supplied; ctx=%p "
		"does not appear to be of type \"opt_walk_t *\"",
		__LINE__, ctx);
	  return(ERR_BADARGS);
     }

     if (owctx->opos == -1)
	  /*
	   *  List exhausted and opt_get_start() not called
	   */
	  return(ERR_EOM);
     else if (owctx->opos == 0)
	  opt_get_start(ctx, flags);

     /*
      *  And some more initializations
      */
     if (!name)
     {
	  name = empty_str;
	  nlen = 0;
     }
     else
	  nlen = strlen(name);
     opts = owctx->parent ? owctx->parent->parent : NULL;
loop:
     /*
      *  See if we've exhausted the list
      */
     if (!owctx->onext && !owctx->ogbldone && !(flags & OPT_MATCH_NOGLOBAL))
     {
	  /*
	   *  Some global options to consider
	   */
	  owctx->onext = owctx->global_options ? *owctx->global_options : NULL;
	  owctx->ogbldone = 1;
     }
     if (!owctx->onext)
     {
	  owctx->opos = -1;
	  return(ERR_EOM);
     }

     /*
      *  See if this option matches
      */
     tmpo = owctx->onext;
     if ((istat = compare(opts, tmpo->name, tmpo->nlen, name, nlen, flags, 0)))
     {
	  if (istat > 0)
	       return(istat);
	  /*
	   *  Does not match
	   */
	  owctx->onext = tmpo->next;
	  goto loop;
     }

     /*
      *  A match
      */
     if (oname)
	  *oname = tmpo->name;
     if (onlen)
	  *onlen = tmpo->nlen;
     if (ovalu)
	  *ovalu = tmpo->valu;
     if (ovlen)
	  *ovlen = tmpo->vlen;

     /*
      *  Book keeping
      */
     tmpo->used   = 1;
     owctx->onext = tmpo->next;
     return(ERR_OK);
}
