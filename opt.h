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
#if !defined(__OPT_H__)

#define __OPT_H__

#include <stdarg.h>
#include "err.h"
#include "debug.h"

#if !defined(__NO_REGEX)
#include <regex.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define OPT_FLAGS_ADD        0x01
#define OPT_FLAGS_NOREPLACE  0x02

#define OPT_FLAGS_OVERRIDE   ( OPT_FLAGS_ADD )
#define OPT_FLAGS_UNDERRIDE  ( OPT_FLAGS_ADD | OPT_FLAGS_NOREPLACE )

#define OPT_GFLAGS_EMPTYOK   0x01

#define OPT_STYPE_GROUP  0
#define OPT_STYPE_OPT    1
#define OPT_STYPE_OPTION 2
#define OPT_STYPE_SOURCE 3
#define OPT_STYPE_WALK   4

#define OPT_NAM_LEN  64
#define OPT_VAL_LEN 256

typedef struct opt_source_s {
     int                  stype;
     size_t               ssize;
     struct opt_source_s *next;
     size_t               slen;
     char                 source[1];
} opt_source_t;

typedef struct opt_option_s {
     int                  stype;
     size_t               ssize;
     struct opt_option_s *next;
     struct opt_group_s  *parent;
     int                  used;
     opt_source_t        *source;
     int                  lineno;
     char                 name[OPT_NAM_LEN];
     size_t               nlen;
     char                 valu[OPT_VAL_LEN];
     size_t               vlen;
} opt_option_t;

typedef struct opt_group_s {
     int                 stype;
     size_t              ssize;
     struct opt_group_s *next;
     struct opt_s       *parent;
     int                 used;
     opt_source_t       *source;
     int                 lineno;
     char                name[OPT_NAM_LEN];
     size_t              nlen;
     char                valu[OPT_VAL_LEN];
     size_t              vlen;
     opt_option_t       *options;
     unsigned int        flags;
} opt_group_t;

#if !defined(__NO_REGEX)

typedef struct opt_regex_s {
     int     rflags;
     size_t  plen;
     char   *pat;
     regex_t cexp;
} opt_regex_t;

#endif

typedef struct opt_s {
     int           stype;
     size_t        ssize;
     opt_group_t  *groups;
     opt_group_t  *global_group;
     opt_source_t *sources;
     const char   *empty_opts_allowed;
#if !defined(__NO_REGEX)
     opt_regex_t   group_regex;
     opt_regex_t   option_regex;
#endif
} opt_t;

typedef struct {
     int            stype;
     size_t         ssize;
     int            opos;
     int            ogbldone;
     opt_option_t  *onext;
     opt_group_t   *parent;
     opt_option_t **global_options;
     opt_option_t **local_options;
} opt_walk_t;

#define OPT_DTYPE_STRING  0
#define OPT_DTYPE_FLOAT   1
#define OPT_DTYPE_GID     2
#define OPT_DTYPE_INT     3
#define OPT_DTYPE_SHORT   4
#define OPT_DTYPE_UID     5
#define OPT_DTYPE_UINT    6
#define OPT_DTYPE_USHORT  7

#define OPT_DTYPE_FIRST   0
#define OPT_DTYPE_LAST    7

#define OPT_DTYPE_NUMERIC_FIRST 1
#define OPT_DTYPE_NUMERIC_LAST  7

#define OPT_DTYPE_STRING_FIRST  0
#define OPT_DTYPE_STRING_LAST   0

struct opt_bulkload_s;

typedef int opt_parse_proc_t(void *ctx, void *outbuf, size_t outbufsize,
  const char *inbuf, size_t inlen, const opt_option_t *opt,
  const struct opt_bulkload_s *item);

typedef struct opt_bulkload_s {
     int               dtype;
     int               base;
     size_t            width;
     const char       *name;
     size_t            nlen;
     const void       *ptr;
     size_t            offset;
     opt_parse_proc_t *proc;
     void             *ctx;
} opt_bulkload_t;

#define OBULK_TERM \
     0, 0, 0, NULL, 0, NULL, 0, NULL, NULL

#define OBULK_STRP(name,fld,trunc,proc,ctx) \
     OPT_DTYPE_STRING, trunc, sizeof(fld), name, sizeof(name)-1, &fld, 0, \
	  proc, ctx

#define OBULK_NUMP(name,fld,base,dtype,proc,ctx) \
     dtype, base, sizeof(fld), name, sizeof(name)-1, &fld, 0, proc, ctx

#define OBULK_STR(name,fld,trunc) OBULK_STRP(name,fld,trunc,NULL,NULL)

#define OBULK_NUM(name,fld,base,dtype) OBULK_NUMP(name,fld,base,dtype,NULL,NULL)

#define OBULK_GID(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_GID)
#define OBULK_INT(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_INT)
#define OBULK_SHORT(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_SHORT)
#define OBULK_UID(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_UID)
#define OBULK_UINT(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_UINT)
#define OBULK_USHORT(name,fld,base) OBULK_NUM(name,fld,base,OPT_DTYPE_USHORT)
#define OBULK_FLOAT(name,fld) OBULK_NUM(name,fld,0,OPT_DTYPE_FLOAT)

int opt_bulkload_init(opt_bulkload_t *bdata, void *whatever);
int opt_bulkload(void *ctx, opt_bulkload_t *bdata, void *whatever, int match);

/*
 *  We want the value zero to correspond to the common case of
 *
 *  "match this case-insensistive string exactly"
 *
 *  OPT_MATCH_x where x >= 4 are bit flags
 */
#define OPT_MATCH_EXACT           0x00000000
#define OPT_MATCH_ENDS_WITH       0x00000001
#define OPT_MATCH_BEGINS_WITH     0x00000002
#define OPT_MATCH_REGEX           0x00000003
#define OPT_MATCH_CASE            0x00000004
#define OPT_MATCH_NOGLOBAL        0x00000008
#define OPT_MATCH_GLOBAL_FALLBACK 0x00000010

#define OPT_MATCH_MASK            0x00000007

void opt_debug_set(debug_proc_t *proc, void *ctx, int flags);

typedef int opt_group_walk_callback_t(void *callback_ctx, void *opt_ctx,
  const char *group_name, size_t name_len, const char *group_value,
  size_t value_len);

void opt_init(opt_t *opts);
void opt_dispose(opt_t *opts);
int opt_read(opt_t *opts, const char *fname, int *fexists);
int opt_get_start(void *ctx, int flags);
int opt_get_next(void *ctx, const char **oname, size_t *onlen, char **ovalu,
  size_t *ovlen, const char *name, int flags);
int opt_group_walk(opt_t *opts, const char *group_name, int flags,
  opt_group_walk_callback_t *callback_proc, void *callback_ctx);

const char *opt_source(void *ctx);
ssize_t opt_lineno(void *ctx);

#define OPT_ITEM_END           0
#define OPT_ITEM_EMPTY_ALLOWED 1

int opt_set(opt_t *opts, int item_code, ...);
int opt_option_push(void *ctx, const char *gname, const char *gval,
  const char *oname, const char *oval, const char *source, int lineno,
  int flags);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__OPT_H__) */
