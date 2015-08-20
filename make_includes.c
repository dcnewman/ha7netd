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
 *  This program generates the xml_const.h header file and the xml_const.xsl
 *  XSLT include file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "opt.h"
#include "err.h"
#include "os.h"
#include "bm.h"

typedef struct {
     FILE *fp;
     int   make_header;
     int   counter;
} callback_t;


static void
usage(FILE *fp, const char *prog)
{
     const char *bn;

     bn = prog ? os_basename(prog) : "make_includes";
     fprintf(fp ? fp : stderr,
"%s: <input-file> <output-file>\n"
" input-file - Input file to read (e.g., make_includes.conf)\n"
"output-file - Output file to produce; must end with either \".xsl\" or "
"\".h\"\n",
	     bn);
}


static void
parse(char **x, char **y, char **z, char **d, char *buf, const char *nam,
      size_t nlen, const char *val, size_t vlen)
{
     char *bptr, *dd, *ptr, *xx, *yy, *zz;

     /*
      *  Handle the bozo case
      */
     if (!buf)
     {
	  xx = yy = zz = dd = NULL;
	  goto done;
     }

     bptr = buf;

     xx = bptr;
     if (!nam)
	  nlen = 0;
     if (nlen)
	  memcpy(bptr, nam, nlen);
     bptr += nlen;
     *bptr++ = '\0';

     ptr = memchr(xx, ';', nlen);
     if (ptr)
     {
	  *ptr = '\0';
	  yy = ptr + 1;
     }
     else
     {
	  yy = bptr;
	  if (nlen)
	       memcpy(bptr, nam, nlen);
	  bptr += nlen;
	  *bptr++ = '\0';
     }

     dd = bptr;
     if (!val)
	  vlen = 0;
     if (vlen)
	  memcpy(bptr, val, vlen);
     bptr += vlen;
     *bptr++ = '\0';

     ptr = memchr(dd, ';', vlen);
     if (ptr)
     {
	  *ptr = '\0';
	  zz = dd;
	  dd = ptr + 1;
     }
     else
     {
	  zz = bptr;
	  if (nlen)
	       memcpy(bptr, nam, nlen);
	  bptr += nlen;
	  *bptr++ = '\0';
     }

done:
     if (x)
	  *x = xx;
     if (y)
	  *y = yy;
     if (z)
	  *z = zz;
     if (d)
	  *d = dd;

     return;
}


static int
walk_datatypes1(void *our_ctx, void *optlib_ctx, const char *gname, 
		size_t gnlen, const char *gval, size_t gvlen)
{
     char buf[2*OPT_NAM_LEN + 2*OPT_VAL_LEN + 4], *d, *ptr, *x, *y, *z;
     int counter, ires, npad1, npad2;
     callback_t *ctx = (callback_t *)our_ctx;
     size_t dlen, i, len, xlen, ylen, zlen;
     const char *name, *desc;
     static const char *padding = "                                        ";

     if (!ctx || !ctx->fp)
	  return(ERR_NO);

     /*
      *  Output gval as a comment
      */
     if (!ctx->make_header)
     {
	  if (!gval || !gvlen)
	       gval = "Names of the various measurement types";
	  fprintf(ctx->fp,
		  "\n"
		  "  <!-- %s -->\n",
		  gval);
     }

loop:
     ires = opt_get_next(optlib_ctx, &name, &len, (char **)&desc, &dlen, "",
			 OPT_MATCH_BEGINS_WITH | OPT_MATCH_NOGLOBAL);
     if (ires != ERR_OK)
	  goto done;
     else if (!name || !len)
	  goto loop;

     /*
      *  x,["," y] "=" [z, "," ] d
      */
     x = y = z = d = NULL;
     parse(&x, &y, &z, &d, buf, name, len, desc, dlen);
     xlen = x ? strlen(x) : 0;
     ylen = y ? strlen(y) : 0;
     zlen = z ? strlen(z) : 0;
     dlen = d ? strlen(d) : 0;

     if (ctx->make_header)
     {
	  /*
	   *  Output the unknown device type?
	   */
	  if (ctx->counter <= 0)
	  {
	       ires = fprintf(ctx->fp,
			      "#define DEV_DTYPE_FIRST     0\n"
			      "#define DEV_DTYPE_UNKNOWN   0\n");
	       if (ires < 0)
	       {
		    ires = ERR_WRITE;
		    goto done;
	       }
	       ctx->counter = 1;
	  }

	  /*
	   *  Upper case x
	   */
	  ptr = x;
	  while (*ptr)
	  {
	       *ptr = toupper(*ptr);
	       ptr++;
	  }

	  npad1 = 25 - (xlen + 18);
	  if (npad1 < 0)
	       npad1 = 0;
	  npad2 = 38 - (dlen + 10);
	  if (npad2 < 0)
	       npad2 = 0;
	  ires = fprintf(ctx->fp, "#define DEV_DTYPE_%.*s %.*s%3d  "
					 "/* %.*s %.*s*/\n",
					 (int)xlen, x, (int)npad1, padding, ctx->counter,
					 (int)dlen, d, (int)npad2, padding);
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto done;
	  }
	  ctx->counter += 1;
     }
     else
     {
	  /*
	   *  Make a lower case version of y
	   */
	  ptr = y;
	  while (*ptr)
	  {
	       *ptr = tolower(*ptr);
	       ptr++;
	  }

	  npad1 = 7 - ylen;
	  if (npad1 < 0)
	       npad1 = 0;
	  npad2 = 5 - ylen;
	  if (npad2 < 0)
	       npad2 = 0;
	  ires = fprintf(ctx->fp, "  <xsl:variable name=\"%.*s\" %.*s"
					 "select=\"'%.*s\'\"/> %.*s<!-- %.*s -->\n",
					 (int)ylen, y, (int)npad1, padding,
					 (int)zlen, z, (int)npad2, padding,
					 (int)dlen, d);
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto done;
	  }
     }
     goto loop;

done:
     if (ires == ERR_EOM)
	  return(ERR_OK);
     else
	  return(ires);
}


static int
walk_datatypes2(void *our_ctx, void *optlib_ctx, const char *gname, 
		size_t gnlen, const char *gval, size_t gvlen)
{
     char buf[2*OPT_NAM_LEN + 2*OPT_VAL_LEN + 4], *d, *ptr, *x, *y, *z;
     int counter, ires, npad1, npad2;
     callback_t *ctx = (callback_t *)our_ctx;
     const char *name, *desc;
     size_t dlen, i, len, xlen, ylen, zlen;
     static const char *padding = "                                        ";

     if (!ctx || !ctx->fp)
	  return(ERR_NO);
     else if (!ctx->make_header)
	  return(ERR_OK);

     if (ctx->counter <= 0)
     {
	  ires = fprintf(ctx->fp,
			 "#if defined(__BUILD_DNAMES__)\n"
			 "\n"
			 "static struct {\n"
			 "     int         dtype;\n"
			 "     const char *dname;\n"
			 "     const char *descr;\n"
			 "} build_dnames[] = {\n"
			 "     { DEV_DTYPE_UNKNOWN,  \"\",      \"\" },\n");
     }

loop:
     ires = opt_get_next(optlib_ctx, &name, &len, (char **)&desc, &dlen, "",
			 OPT_MATCH_BEGINS_WITH | OPT_MATCH_NOGLOBAL);
     if (ires != ERR_OK)	  goto done;
     else if (!name || !len)
	  goto loop;

     /*
      *  x,["," y] "=" [z, "," ] d
      */
     x = y = z = d = NULL;
     parse(&x, &y, &z, &d, buf, name, len, desc, dlen);
     xlen = x ? strlen(x) : 0;
     ylen = y ? strlen(y) : 0;
     zlen = z ? strlen(z) : 0;
     dlen = d ? strlen(d) : 0;

     /*
      *  Make an upper case version of x
      */
     ptr = x;
     while (*ptr)
     {
	  *ptr = toupper(*ptr);
	  ptr++;
     }

     npad1 = 8 - xlen;
     if (npad1 < 0)
	  npad1 = 0;
     npad2 = 4 - zlen;
     if (npad2 < 0)
	  npad2 = 0;
     ires = fprintf(ctx->fp,
					"     { DEV_DTYPE_%.*s, %.*s\"%.*s\", %.*s \"%s\" },\n",
					(int)xlen, x, (int)npad1, padding,
					(int)zlen, z, (int)npad2, padding, d);
     if (ires >= 0)
	  goto loop;

     ires = ERR_WRITE;

done:
     if (ires == ERR_EOM)
	  return(ERR_OK);
     else
	  return(ires);
}


static int
walk_units1(void *our_ctx, void *optlib_ctx, const char *gname, 
	    size_t gnlen, const char *gval, size_t gvlen)
{
     char buf[2*OPT_NAM_LEN + 2*OPT_VAL_LEN + 4], *d, *ptr, *x, *y, *z;
     int counter, ires, npad1, npad2;
     callback_t *ctx = (callback_t *)our_ctx;
     const char *name, *desc;
     size_t dlen, i, len, xlen, ylen, zlen;
     static const char *padding = "                                        ";

     if (!ctx || !ctx->fp)
	  return(ERR_NO);

     if (!ctx->make_header && gval)
	  /*
	   *  Output gval as a comment
	   */
	  fprintf(ctx->fp,
		  "\n"
		  "  <!-- %s -->\n",
		  gval);

loop:
     ires = opt_get_next(optlib_ctx, &name, &len, (char **)&desc, &dlen, "",
			 OPT_MATCH_BEGINS_WITH | OPT_MATCH_NOGLOBAL);
     if (ires != ERR_OK)
	  goto done;
     else if (!name || !len)
	  goto loop;

     /*
      *  x,["," y] "=" [z, "," ] d
      */
     x = y = z = d = NULL;
     parse(&x, &y, &z, &d, buf, name, len, desc, dlen);
     xlen = x ? strlen(x) : 0;
     ylen = y ? strlen(y) : 0;
     zlen = z ? strlen(z) : 0;
     dlen = d ? strlen(d) : 0;

     if (ctx->make_header)
     {
	  /*
	   *  Output the unknown device type?
	   */
	  if (ctx->counter <= 0)
	  {
	       ires = fprintf(ctx->fp,
			      "#define DEV_UNIT_UNKNOWN    0\n");
	       if (ires < 0)
	       {
		    ires = ERR_WRITE;
		    goto done;
	       }
	       ctx->counter = 1;
	  }

	  /*
	   *  Make an upper case version of "x"
	   */
	  ptr = x;
	  while (*ptr)
	  {
	       *ptr = toupper(*ptr);
	       ptr++;
	  }

	  npad1 = 25 - (xlen + 17);
	  if (npad1 < 0)
	       npad1 = 0;
	  npad2 = 38 - (dlen + 10);
	  if (npad2 < 0)
	       npad2 = 0;
	  ires = fprintf(ctx->fp, "#define DEV_UNIT_%.*s %.*s%3d  "
					 "/* %.*s %.*s*/\n",
					 (int)xlen, x, (int)npad1, padding, ctx->counter,
					 (int)dlen, d, (int)npad2, padding);
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto done;
	  }
	  ctx->counter += 1;
     }
     else
     {
	  npad1 = 5 - ylen;
	  if (npad1 < 0)
	       npad1 = 0;
	  npad2 = 5 - zlen;
	  if (npad2 < 0)
	       npad2 = 0;
	  ires = fprintf(ctx->fp, "  <xsl:variable name=\"u-%.*s\" %.*s"
					 "select=\"'%.*s\'\"/> %.*s<!-- %.*s -->\n",
					 (int)ylen, y, (int)npad1, padding,
					 (int)zlen, z, (int)npad2, padding,
					 (int)dlen, d);
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto done;
	  }
     }
     goto loop;

done:
     if (ires == ERR_EOM)
	  return(ERR_OK);
     else
	  return(ires);
}


static int
walk_units2(void *our_ctx, void *optlib_ctx, const char *gname, 
	    size_t gnlen, const char *gval, size_t gvlen)
{
     char buf[2*OPT_NAM_LEN + 2*OPT_VAL_LEN + 4], *d, *ptr, *x, *y, *z;
     int counter, ires, npad1, npad2;
     callback_t *ctx = (callback_t *)our_ctx;
     size_t dlen, i, len, xlen, ylen, zlen;
     const char *name, *desc;
     static const char *padding = "                                        ";

     if (!ctx || !ctx->fp)
	  return(ERR_NO);
     else if (!ctx->make_header)
	  return(ERR_OK);

loop:
     ires = opt_get_next(optlib_ctx, &name, &len, (char **)&desc, &dlen, "",
			 OPT_MATCH_BEGINS_WITH | OPT_MATCH_NOGLOBAL);
     if (ires != ERR_OK)
	  goto done;
     else if (!name || !len)
	  goto loop;

     /*
      *  Output gval as a comment
      */
     if (!ctx->counter)
     {
	  ctx->counter = 1;
	  ires = fprintf(ctx->fp,
			 "#if defined(__BUILD_UNITS__)\n"
			 "\n"
			 "static struct {\n"
			 "     int         atype;\n"
			 "     const char *abbrev;\n"
			 "} build_units[] = {\n"
			 "     { DEV_UNIT_UNKNOWN,  \"\"     },\n");
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto done;
	  }
     }

     /*
      *  x,["," y] "=" [z, "," ] d
      */
     x = y = z = d = NULL;
     parse(&x, &y, &z, &d, buf, name, len, desc, dlen);
     xlen = x ? strlen(x) : 0;
     ylen = y ? strlen(y) : 0;
     zlen = z ? strlen(z) : 0;
     dlen = d ? strlen(d) : 0;

     /*
      *  Make an upper case version of "x"
      */
     ptr = x;
     while (*ptr)
     {
	  *ptr = toupper(*ptr);
	  ptr++;
     }

     npad1 = 24 - (xlen + 16);
     if (npad1 < 0)
	  npad1 = 0;
     npad2 = 4 - zlen;
     if (npad2 < 0)
	  npad2 = 0;
     ires = fprintf(ctx->fp,
					"     { DEV_UNIT_%.*s, %.*s\"%.*s\" %.*s},\n",
					(int)xlen, x, (int)npad1, padding,
					(int)zlen, z, (int)npad2, padding);
     if (ires < 0)
     {
	  ires = ERR_WRITE;
	  goto done;
     }
     ctx->counter += 1;
     goto loop;

done:
     if (ires == ERR_EOM)
	  return(ERR_OK);
     else
	  return(ires);
}


static int
build_skips(FILE *fp, const char *name, const char *str)
{
     bm_t binfo;
     char c;
     int j;

     if (!fp || !name || !str)
	  return(-1);

     if (bm_skip_init(&binfo, str))
     {
	  fprintf(stderr, "Unable to build the Boyer-Moore skip table for "
		  "the string \"%s\"\n",
		  str ? str : "(null)");
	  return(-1);
     }

     if (0 > fprintf(fp,
		     "\n"
		     "static const bm_t bm_info_%s = {\n"
		     "  0, %d, {\"", name, binfo.sublen))
	  return(-1);

     while ((c = *str++))
     {
	  switch(c)
	  {
	  case '"' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('"', fp))
		    return(-1);
	       break;

	  case '\\' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp))
		    return(-1);
	       break;

	  case '\f' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp) ||
		   EOF == fputc('f', fp))
		    return(-1);
	       break;

	  case '\n' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp) ||
		   EOF == fputc('n', fp))
		    return(-1);
	       break;

	  case '\r' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp) ||
		   EOF == fputc('r', fp))
		    return(-1);
	       break;

	  case '\t' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp) ||
		   EOF == fputc('t', fp))
		    return(-1);
	       break;

	  case '\v' :
	       if (EOF == fputc('\\', fp) ||
		   EOF == fputc('\\', fp) ||
		   EOF == fputc('v', fp))
		    return(-1);
	       break;

	  default :
	       if (EOF == fputc(c, fp))
		    return(-1);
	       break;
	  }
     }
     if (0 > fprintf(fp,
		     "\"},\n"
		     "  {"))
	  return(-1);
     for (j = 0; j < 256; j++)
     {
	  if (0 == (j % 16) && j != 0)
	       if (0 > fprintf(fp, "\n   "))
		    return(-1);
	  if (0 > fprintf(fp, "%3d%s", binfo.skip[j], (j < 255) ? "," : ""))
	       return(-1);
     }
     if (0 > fprintf(fp, "}\n};\n"))
	 return(-1);

     return(0);
}


static int
walk_bm(void *our_ctx, void *optlib_ctx, const char *gname, 
	size_t gnlen, const char *gval, size_t gvlen)
{
     callback_t *ctx = (callback_t *)our_ctx;
     int ires;
     const char *name, *str;
     size_t nlen, slen;

     if (!ctx || !ctx->fp)
	  return(ERR_NO);

loop:
     ires = opt_get_next(optlib_ctx, &name, &nlen, (char **)&str, &slen, "",
			 OPT_MATCH_BEGINS_WITH | OPT_MATCH_NOGLOBAL);
     if (ires != ERR_OK)
	  goto done;
     else if (!name || !nlen || !str || !slen)
	  goto loop;

     if (build_skips(ctx->fp, name, str))
	  return(ERR_NO);

     goto loop;

done:
     if (ires == ERR_EOM)
	  return(ERR_OK);
     else
	  return(ires);
}


int
main(int argc, const char *argv[])
{
     callback_t ctx;
     int boyer_moore, dispose_opts, i, ires;
     const char *infile, *outfile;
     size_t len;
     opt_t opts;

     boyer_moore  = 0;
     ctx.fp       = NULL;
     dispose_opts = 0;
     infile       = NULL;
     outfile      = NULL;

     for (i = 1; i < argc; i++)
     {
	  switch(argv[i][0])
	  {
	  case '?' :
	       usage(stdout, argv[0]);
	       return(0);

	  case '-' :
	       goto bad;
	       break;

	  default :
	       if (!infile)
		    infile = argv[i];
	       else if (!outfile)
		    outfile = argv[i];
	       else
		    goto bad;
	  }
     }

     /*
      *  Acceptable invocation?
      */
     if (!infile || !infile[0] || !outfile || !outfile[0])
	  goto bad;

     /*
      *  Acceptable output file name?
      */
     len = strlen(outfile);
     if (len > 2 && !memcmp(outfile + len - 2, ".h", 2))
	  ctx.make_header = 1;
     else if (len > 4 && !memcmp(outfile + len - 4, ".xsl", 4))
	  ctx.make_header = 0;
     else
	  goto bad;

     if (ctx.make_header && outfile[0] == 'b')
	  boyer_moore = 1;

     /*
      *  Process the input file
      */
     opt_init(&opts);
     ires = opt_read(&opts, infile, NULL);
     if (ires != ERR_OK)
     {
	  fprintf(stderr, "Unable to process the input file, \"%s\"; %s\n",
		  infile, err_strerror(ires));
	  return(1);
     }
     dispose_opts = 1;

     /*
      *  Open the output file
      */
     ctx.fp = fopen(outfile, "w");
     if (!ctx.fp)
     {
	  fprintf(stderr, "Unable to open the output file, \"%s\"; "
		  "errno=%d; %s\n", outfile, errno, strerror(errno));
	  opt_dispose(&opts);
	  return(1);
     }

     /*
      *  File preambles
      */
     if (ctx.make_header)
     {
	  ires = fprintf(ctx.fp,
		        "/*\n"
		        " *  Copyright (c) 2005, Daniel C. Newman "
		        "<dan.newman@mtbaldy.us>\n"
		        " *  All rights reserved.\n"
			" *  See the file COPYRIGHT for further information.\n"
		        " */\n"
			"\n");
	  if (ires < 0)
	  {
	       ires = ERR_WRITE;
	       goto error;
	  }
	  if (boyer_moore)
	       ires = fprintf(ctx.fp,
			 "/*\n"
			 " * *** DO NOT EDIT THIS FILE ***\n"
			 " * *** This file was automatically generated by %s\n"
			 " * *** from the source file %s\n"
			 " */\n"
			 "\n"
			 "#if !defined(__BM_CONST_H__)\n"
			 "\n"
			 "#define __BM_CONST_H__\n"
			 "\n"
			 "#include \"bm.h\"\n", __FILE__, infile);
	  else
	       ires = fprintf(ctx.fp,
			 "/*\n"
			 " * *** DO NOT EDIT THIS FILE ***\n"
			 " * *** This file was automatically generated by %s\n"
			 " * *** from the source file %s\n"
			 " */\n"
			 "\n"
			 "#if !defined(__XML_CONST_H__)\n"
			 "\n"
			 "#define __XML_CONST_H__\n"
			 "\n"
			 "#if defined(__cplusplus)\n"
			 "extern \"C\" {\n"
			 "#endif\n"
			 "\n"
			 "#define DEV_MISSING_VALUE '*'\n"
			 "\n", __FILE__, infile);
     }
     else
	  ires = fprintf(ctx.fp,
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"\n"
"<!-- Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>\n"
"     All rights reserved.\n"
"     See the file COPYRIGHT for further information. -->\n"
"\n"
"<!-- *** DO NOT EDIT THIS FILE ***\n"
"     *** This file was automatically generated by %s\n"
"     *** from the source file %s. -->\n"
"\n"
"<xsl:stylesheet version=\"1.0\"\n"
"                xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">\n"
"\n"
"  <!-- String used to indicate a missing data value -->\n"
"  <xsl:variable name=\"missing-value\" select=\"'*'\"/>\n",
			 __FILE__, infile);
     if (ires < 0)
     {
	  ires = ERR_WRITE;
	  goto error;
     }

     if (boyer_moore)
     {
	  ires = opt_group_walk(&opts, "boyer-moore",
				OPT_MATCH_EXACT | OPT_MATCH_NOGLOBAL,
				walk_bm, (void *)&ctx);
	  if (ires == ERR_OK)
	       goto finish;
	  else
	       goto error;
     }


     /*
      *  Process all data types
      */
     ctx.counter = 0;
     ires = opt_group_walk(&opts, "measurements",
			   OPT_MATCH_EXACT | OPT_MATCH_NOGLOBAL,
			   walk_datatypes1, (void *)&ctx);
     if (ires != ERR_OK)
	  goto error;

     if (ctx.make_header)
     {
	  if (0 > fprintf(ctx.fp, "#define DEV_DTYPE_LAST    %3d\n\n",
			  ctx.counter - 1))
	  {
	       ires = ERR_WRITE;
	       goto error;
	  }

	  ctx.counter = 0;
	  ires = opt_group_walk(&opts, "measurements",
				OPT_MATCH_EXACT | OPT_MATCH_NOGLOBAL,
				walk_datatypes2, (void *)&ctx);
	  if (ires != ERR_OK)
	       goto error;

	  if (0 > fprintf(ctx.fp,
			  "     { -1, 0 }\n"
			  "};\n"
			  "\n"
			  "#endif /* if defined(__BUILD_DNAMES__) */\n"
			  "\n"))
	  {
	       ires = ERR_WRITE;
	       goto error;
	  }
     }
     
     /*
      *  Process all units
      */
     ctx.counter = 0;
     ires = opt_group_walk(&opts, "units",
			   OPT_MATCH_EXACT | OPT_MATCH_NOGLOBAL,
			   walk_units1, (void *)&ctx);
     if (ires != ERR_OK)
	  goto error;

     if (ctx.make_header)
     {
	  if (0 > fprintf(ctx.fp, "#define DEV_UNIT_LAST     %3d\n\n",
			  ctx.counter - 1))
	  {
	       ires = ERR_WRITE;
	       goto error;
	  }

	  ctx.counter = 0;
	  ires = opt_group_walk(&opts, "units",
				OPT_MATCH_EXACT | OPT_MATCH_NOGLOBAL,
				walk_units2, (void *)&ctx);
	  if (ires != ERR_OK)
	       goto error;

	  if (0 > fprintf(ctx.fp,
			  "     { -1, 0 }\n"
			  "};\n"
			  "\n"
			  "#endif /* if defined(__BUILD_UNITS__) */\n"))
	  {
	       ires = ERR_WRITE;
	       goto error;
	  }
     }

     /*
      *  All done with these
      */
finish:
     opt_dispose(&opts);
     dispose_opts = 0;

     /*
      *  File postambles
      */
     if (ctx.make_header)
     {
	  if (boyer_moore)
	       ires = fprintf(ctx.fp,
			      "\n"
			      "#endif /* !defined(__BM_CONST_H__) */\n");
	  else
	       ires = fprintf(ctx.fp,
			      "\n"
			      "#if defined(__cplusplus)\n"
			      "}\n"
			      "#endif\n"
			      "\n"
			      "#endif /* !defined(__XML_CONST_H__) */\n");
     }
     else
	  ires = fprintf(ctx.fp,
			 "\n"
			 "</xsl:stylesheet>\n");
     if (ires < 0)
     {
	  ires = ERR_WRITE;
	  goto error;
     }

     /*
      *  Attempt to close the file
      */
     if (!fclose(ctx.fp))
	  /*
	   *  Success
	   */
	  return(0);

     /*
      *  Error of some sort
      */
     fprintf(stderr, "Error closing the output file, \"%s\"; errno=%d; %s\n",
	     outfile, errno, strerror(errno));
     if (outfile)
	  remove(outfile);
     return(1);
     
error:
     if (ires == ERR_WRITE)
	  fprintf(stderr, "Unable to write the output file, \"%s\", errno=%d; "
		  "%s\n", outfile, errno, strerror(errno));
     else
	  fprintf(stderr, "Unable to process the input file, \"%s\", "
		  "%s\n", outfile, err_strerror(ires));
     if (ctx.fp)
     {
	  fclose(ctx.fp);
	  ctx.fp = NULL;
	  if (outfile)
	       remove(outfile);
     }
     if (dispose_opts)
	  opt_dispose(&opts);
     return(1);

bad:
     usage(stderr, argv[0]);
     return(1);
}
