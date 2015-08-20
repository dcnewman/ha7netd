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
#include <ctype.h>
#include <time.h>
#include <pthread.h>

static void os_close_files(int fd_min, int fd_max);

#if defined(_WIN32)
#  include "os-win32.c"
#  include "os-win32-pthread.c"
#else
#  include <unistd.h>
#  include "os-unix.c"
#endif


static void
os_close_files(int fd_min, int fd_max)
{
     if (fd_min > fd_max)
	  return;
     else if (fd_min < 0)
	  fd_min = 0;
     for (; fd_min <= fd_max; fd_min++)
	  close(fd_min);
}


typedef struct {
     os_pthread_mutex_t mutex;
     os_pthread_cond_t  cond;
     int                flag;
     int                nthreads;
} shutdown_t;


int
os_shutdown_create(os_shutdown_t **info)
{
     int istat;
     shutdown_t *sinfo;

     if (!info)
     {
	  errno = EINVAL;
	  return(-1);
     }

     sinfo = (shutdown_t *)calloc(1, sizeof(shutdown_t));
     if (!sinfo)
     {
	  /*
	   *  Leave errno as set by calloc()
	   */
	  *info = NULL;
	  return(-1);
     }

     istat = os_pthread_mutex_init(&sinfo->mutex, NULL);
     if (!istat)
	  istat = os_pthread_cond_init(&sinfo->cond, NULL);
     if (istat)
     {
	  errno = istat;
	  return(-1);
     }
     sinfo->flag     = 0;
     sinfo->nthreads = 0;
     *info = (os_shutdown_t *)sinfo;
     return(0);
}


int
os_shutdown(os_shutdown_t *info)
{
     shutdown_t *sinfo = (shutdown_t *)info;
     if (!info)
     {
	  errno = EINVAL;
	  return(-1);
     }
     else
	  /*
	   *  Go ahead and access the flag without the mutex
	   */
	  return(sinfo->flag ? 1 : 0);
}


int
os_shutdown_begin(os_shutdown_t *info)
{
     int istat;
     shutdown_t *sinfo = (shutdown_t *)info;

     if (!info)
     {
	  errno = EINVAL;
	  return(-1);
     }

     /*
      *  Set the shutdown flag and raise the condition
      */
     istat = os_pthread_mutex_lock(&sinfo->mutex);
     if (istat)
     {
	  errno = istat;
	  return(-1);
     }
     sinfo->flag = 1;
     istat = os_pthread_cond_broadcast(&sinfo->cond);
     os_pthread_mutex_unlock(&sinfo->mutex);
     if (!istat)
	  return(0);

     errno = istat;
     return(-1);
}


int
os_shutdown_finish(os_shutdown_t *info, unsigned int seconds)
{
     time_t die_by;
     int istat, s;
     shutdown_t *sinfo = (shutdown_t *)info;
     struct timespec tv;

     if (!info)
     {
	  errno = EINVAL;
	  return(-1);
     }

     /*
      *  Initializations
      */
     die_by = time(0) + seconds;

     /*
      *  Lock the mutex before (1) looking at sinfo->nthreads, and
      *  (2) calling os_pthread_cond_timedwait()
      */
     istat = os_pthread_mutex_lock(&sinfo->mutex);
     if (istat)
     {
	  errno = istat;
	  return(-1);
     }

     /*
      *  Set the shutdown flag just in case shutdown_begin() was not called
      */
     sinfo->flag = 1;

     /*
      *  Now wait for all the running threads to wind down and exit
      */
     istat      = 0;
     tv.tv_nsec = 0;
     while (sinfo->nthreads > 0)
     {
	  tv.tv_sec = time(0);
	  if ((int)difftime(tv.tv_sec, die_by) >= 0)
	  {
	       /*
		*  We've waited too long; let's blow this popsicle stand
		*/
	       errno = ETIMEDOUT;
	       istat = -1;
	       break;
	  }
	  tv.tv_sec += 1;

	  /*
	   *  Wait 1 second
	   */
	  os_pthread_cond_timedwait(&sinfo->cond, &sinfo->mutex, &tv);
     }
     os_pthread_mutex_unlock(&sinfo->mutex);

     /*
      *  Release our resources only if all the threads exited...
      *  Otherwise, we may core later on.  (We could do some double
      *  dereferencing to fix all this.)
      */
     if (!istat)
     {
	  os_pthread_cond_destroy(&sinfo->cond);
	  os_pthread_mutex_destroy(&sinfo->mutex);
	  free(sinfo);
     }

     return(istat);
}


void
os_shutdown_thread_decr(os_shutdown_t *info)
{
     shutdown_t *sinfo = (shutdown_t *)info;

     if (!info)
	  return;

     /*
      *  Decrement the count of running threads and raise the condition in
      *  case os_shutdown_finish()is waiting on us.
      */
     os_pthread_mutex_lock(&sinfo->mutex);
     sinfo->nthreads -= 1;
     os_pthread_cond_signal(&sinfo->cond);
     os_pthread_mutex_unlock(&sinfo->mutex);
}


void
os_shutdown_thread_incr(os_shutdown_t *info)
{
     shutdown_t *sinfo = (shutdown_t *)info;

     if (!info)
	  return;

     os_pthread_mutex_lock(&sinfo->mutex);
     sinfo->nthreads += 1;
     os_pthread_mutex_unlock(&sinfo->mutex);
}


int
os_shutdown_wait(os_shutdown_t *info)
{
     int istat;
     shutdown_t *sinfo = (shutdown_t *)info;

     if (!sinfo)
     {
	  errno = EINVAL;
	  return(-1);
     }

     /*
      *  Wait indefinitely
      */
     istat = os_pthread_mutex_lock(&sinfo->mutex);
     if (istat)
     {
	  errno = istat;
	  return(-1);
     }
     while (!sinfo->flag)
	  os_pthread_cond_wait(&sinfo->cond, &sinfo->mutex);
     istat = sinfo->flag;
     os_pthread_mutex_unlock(&sinfo->mutex);

     /*
      *  All done
      */
     return(istat ? 1 : 0);
}


int
os_shutdown_sleep(os_shutdown_t *info, unsigned int milliseconds)
{
     int istat, s;
     shutdown_t *sinfo = (shutdown_t *)info;
     struct timespec tv;

     if (!sinfo)
	  return(os_sleep(milliseconds));

     /*
      *  Split milliseconds into whole seconds and a remainder
      */
     s = (int)(milliseconds / 1000);
     milliseconds = milliseconds - (s * 1000);

     /*
      *  Unlike nanosleep(), os_pthread_cond_timedwait()
      *  takes an absolute time
      */
     tv.tv_sec  = time(NULL) + s;
     tv.tv_nsec = milliseconds * 1000000;

     /*
      *  Now wait; when os_pthread_cond_timedwait() returns, check to see
      *  if the shutdown flag was set...
      */
     istat = os_pthread_mutex_lock(&sinfo->mutex);
     if (istat)
     {
	  errno = istat;
	  return(-1);
     }

     /*
      *  Don't bother waiting if a shutdown has already been signalled
      */
     if (sinfo->flag)
	  goto done;
loop:
     os_pthread_cond_timedwait(&sinfo->cond, &sinfo->mutex, &tv);
     if (!sinfo->flag)
     {
	  /*
	   *  Make sure that this is not a spurious wake up
	   */
	  time_t now = time(NULL);
	  if ((int)difftime(tv.tv_sec, now) > 1)
	       goto loop;
     }
done:
     istat = sinfo->flag;
     os_pthread_mutex_unlock(&sinfo->mutex);

     /*
      *  All done
      */
     return(istat ? 1 : 0);
}


static void
argv_init(os_argv_t *argv)
{
     if (argv)
     {
	  memset(argv, 0, sizeof(os_argv_t));
	  argv->bufptr = argv->buf;
	  argv->maxbuf = OS_ARGV_BUFLEN;
     }
}


void
os_argv_free(os_argv_t *argv)
{
     if (argv)
     {
	  if (argv->bufptr && argv->bufptr != argv->buf)
	       free((char *)argv->bufptr);
	  argv_init(argv);
     }
}


static const char *empty_str = "";

int
os_argv_make(os_argv_t *argv, const char *cmd)
{
     char c, *ptr;
     int dquoted, i, literal, space, squoted; 
     
     /*
      *  Sanity checks
      */
     if (!argv || !cmd)
     {
	  errno = EFAULT;
	  return(-1);
     }

     /*
      *  Initialize argv
      */
     argv_init(argv);

     /*
      *  Useful input?
      */
     if (!cmd[0])
     {
	  argv->argv[0] = (char *)empty_str;
	  argv->argc    = 1;
	  return(0);
     }

     /*
      *  Make a copy of the command line.  We will then point argv[0]
      *  to the head of this string, and the other pieces of argv[] to
      *  other points in the string.
      */
     argv->buflen = strlen(cmd);
     if (argv->buflen >= argv->maxbuf)
     {
	  ptr = (char *)malloc(argv->buflen + 1);
	  if (!ptr)
	  {
	       argv->buflen = 0;
	       errno = ENOMEM;
	       return(-1);
	  }
	  argv->bufptr = (char *)ptr;
	  argv->maxbuf = argv->buflen + 1;
     }

     /*
      *  Copy the command line to argv->bufptr
      */
     memcpy((char *)argv->bufptr, cmd, argv->buflen + 1);

     /*
      *  Now set argv[0] = bufptr;
      */
     argv->argc    = 1;
     argv->argv[0] = (char *)argv->bufptr;

     /*
      *  Initialize the parsing state
      */
     dquoted = 0;
     i       = 1;
     literal = 0;
     space   = 0;
     squoted = 0;

     /*
      *  Now parse the command string;
      */
     ptr = (char *)argv->argv[0] + 1;
     while ((c = *ptr++))
     {
	  if (literal)
	       /*
		*  Ignore this character
		*/
	       literal = 0;
	  else if (c == '\\')
	       /*
		*  Ignore the next character
		*/
	       literal = 1;
	  else if (squoted)
	  {
	       /*
		*  Ignore chars until we see a closing '
		*/
	       if (c == '\'')
		    squoted = 0;
	  }
	  else if (dquoted)
	  {
	       /*
		*  Ignore chars untile we see a closing "
		*/
	       if (c == '\"')
		    dquoted = 0;
	  }
	  else if (isspace(c))
	  {
	       ptr[-1] = '\0';
	       space   = 1;
	  }
	  else
	  {
	       if (space)
	       {
		    space = 0;
		    argv->argv[i++] = ptr - 1;
		    if (i >= OS_ARGV_MAXARG)
			 break;
	       }
	       if (c == '\'')
		    squoted = 1;
	       else if (c == '\"')
		    dquoted = 1;
	  }
     }

     /*
      *  Terminate the list
      */
     argv->argv[i] = NULL;

     /*
      *  Remove any outer quotes
      */
     for (i = 0, ptr = (char *)argv->argv[0];
	  ptr != NULL;
	  ptr = (char *)argv->argv[++i])
     {
	  if (*ptr == '"' || *ptr == '\'')
	  {
	       argv->argv[i] = ptr + 1;
	       ptr[strlen(ptr) - 1] = '\0';
	  }
     }

     /*
      *  All done
      */
     return(0);
}
