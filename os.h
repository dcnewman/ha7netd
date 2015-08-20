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
#if !defined(__OS_H__)

#define __OS_H__

#include <stdarg.h>

#if defined(_WIN32)
#else
#include <libgen.h>    /* for basename() */
#include <sys/types.h> /* for pid_t      */
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_WIN32)

#define NOT_STREAMS_COMPATIBLE 1

#if !defined(_WINDOWS)
#define _WINDOWS /* makes dbm leave ANSI C keywords in */
#endif

#if !defined(EINVAL)
#define EINVAL WSAEINVAL
#endif

#if !defined(EBADF)
#define EBADF WSAENOTSOCK
#endif

#if !defined(EFAULT)
#define EFAULT WSAEFAULT
#endif

#if !defined(ETIMEDOUT)
#define ETIMEDOUT WSAETIMEDOUT
#endif

#define SOCK_ERRNO        WSAGetLastError()
#define SET_SOCK_ERRNO(e) WSASetLastError((e))
#define ISWOULDBLOCK(e) ((e) == WSAEWOULDBLOCK)
#define ISTEMPERR(e)    (((e) == WSAENOBUFS) || ((e) == WSAEINPROGRESS))

#else /* defined(_WIN32) */

#if !defined(O_TEXT)
#define O_TEXT 0
#endif

#if defined(__APPLE__)
#define NOT_STREAMS_COMPATIBLE 1
#endif

#define SOCK_ERRNO        errno
#define SET_SOCK_ERRNO(e) errno = (e)
#define STRERROR          strerror
#define ISWOULDBLOCK(e) (((e) == EAGAIN) || ((e) == EWOULDBLOCK))
#if NOT_STREAMS_COMPATIBLE
#define ISTEMPERR(e) (((e) == EINTR) || ((e) == ENOMEM) || ((e) == ENOBUFS))
#else
#define ISTEMPERR(e) (((e) == EINTR) || ((e) == ENOMEM) || ((e) == ENOBUFS) || \
		      ((e) == ENOSR))
#endif  /* NOT_STREAMS_COMPATIBLE */
#endif  /* !defined(_WIN32) */

#if defined(__osf__) || defined(__sun) || defined(_WIN32)
#  define _EXIT _exit
#elif defined(__APPLE__) || defined(__linux__)
#  define _EXIT _Exit
#else
#  define _EXIT _exit
#endif

#define OS_ARGV_MAXARG  64
#define OS_ARGV_BUFLEN 256

typedef struct {
     int     argc;
     char   *argv[OS_ARGV_MAXARG];
     char   *bufptr;
     size_t  buflen;
     size_t  maxbuf;
     char    buf[OS_ARGV_BUFLEN];
} os_argv_t;

typedef pid_t os_pid_t;

/*
 *  Return the basename of a file path
 */

#if defined(_WIN32)
const char *os_basename(const char *path);
#else
#define os_basename(p) basename((char *)(p))
#endif

/*
 *  Return the local offset from GMT in seconds as well as the local
 *  time zone string.  Both values will reflect current daylight
 *  savings time setting.
 */

const char *os_tzone(long *gmt_offset, const char **tzone, char *buf,
  size_t buflen);


/*
 *  Daemonize via the classic fork() or fork() & execve() with
 *  parent process performing an _exit().  This is a no-op on Windows
 *  where presumably the process was started as a service.
 *
 */

int os_daemonize(int argc, char **argv, const char *extra_arg);


/*
 *  Once daemonized, become process group leader, set our umask(),
 *  and change our working directory.
 *
 */

void os_server_start_1(const char *wdir, int bg);


/*
 *  Finally, close file descriptors inherited from our parent,
 *  and make irrevocable changes of our UID and GID, both real
 *  and effective.
 */

int os_server_start_2(const char *user, int close_stdfiles);


/*
 *  Initialize the event logging system.  NOOP on Unix-like platforms;
 *  important on Windows.
 */

void *os_log_open(const char *facility);


/*
 *  Shut down the event logging system.  NOOP on Unix-like platforms;
 *  good to call on Windows.
 */

void os_log_close(void *ctx);


/*
 *  Log a message to the event logging system.  Syslog on Unix-like
 *  platforms; Windows event log on Windows.
 */

void os_log(void *ctx, int priority, const char *fmt, va_list ap);


/*
 *  Utility routine to return the syslog LOG_ mask value for a
 *  given facility name [e.g., LOG_LOCAL3 == os_facstr2mask("local3")]
 */

int os_facstr2int(const char *str);

/*
 *  Sleep the calling thread for the specified number of milliseconds.
 *  Sleep() is called on Windows; nanosleep() on other platforms.
 */

int os_sleep(unsigned int milliseconds);

/*
 *  pthreads emulation: this *is* pthreads where available and is
 *  an emulation layer elsewhere (i.e., Windows).
 */
#if defined(_WIN32)

void os_pthread_thread_init(void);
int os_mutex_init(os_mutex_t *mutex, const os_mutexattr_t *attr);
int os_mutex_destroy(os_mutex_t *mutex);
int os_mutex_lock(os_mutex_t *mutex);
int os_mutex_unlock(os_mutex_t *mutex);
int os_mutex_trylock(os_mutex_t *mutex);
int os_cond_init(os_cond_t *cond, const os_condattr_t *attr);
int os_cond_destroy(os_cond_t *cond);
int os_cond_signal(os_cond_t *cond);
int os_cond_broadcast(os_cond_t *cond);
int os_cond_wait(os_cond_t *cond, os_mutex_t *mutex);
int os_cond_timedwait(os_cond_t *cond, os_mutex_t *mutex,
  const struct timespec *abstime);


#else

#include <pthread.h>

typedef pthread_mutex_t     os_pthread_mutex_t;
typedef pthread_mutexattr_t os_pthread_mutexattr_t;
typedef pthread_cond_t      os_pthread_cond_t;
typedef pthread_condattr_t  os_pthread_condattr_t;

void os_pthread_thread_init(void);
#define os_pthread_mutex_init     pthread_mutex_init
#define os_pthread_mutex_destroy  pthread_mutex_destroy
#define os_pthread_mutex_lock     pthread_mutex_lock
#define os_pthread_mutex_unlock   pthread_mutex_unlock
#define os_pthread_mutex_trylock  pthread_mutex_trylock
#define os_pthread_cond_init      pthread_cond_init
#define os_pthread_cond_destroy   pthread_cond_destroy
#define os_pthread_cond_signal    pthread_cond_signal
#define os_pthread_cond_broadcast pthread_cond_broadcast
#define os_pthread_cond_wait      pthread_cond_wait
#define os_pthread_cond_timedwait pthread_cond_timedwait

#endif

/*
 *  O/S dependent routines to handle shutdown signalling
 *  Uses pthread condition signalling on platforms with
 *  pthread support.
 */

typedef void os_shutdown_t;

/*
 *  Return a pointer to a newly created and initialized os_shutdown_t
 *  structure.  Call os_shutdown_finish() to properly dispose of the
 *  structure.
 *
 *  On systems with pthread support, the structure primarily consists
 *  of a pthread_cond_t condition variable and a pthread_mutex_t mutex.
 */

int os_shutdown_create(os_shutdown_t **info);

/*
 *  Signal a shutdown condition.  Any threads sleeping with os_shutdown_sleep()
 *  or os_shutdown_wait() will be awakened and os_shutdown_sleep() or
 *  os_shutdown_wait() will return 1 to indicate that a shutdown has been
 *  requested.
 *
 *  A return value of 0 indicates a success.  A non-zero return value
 *  indicates an error in which case errno should be consulted.
 */

int os_shutdown_begin(os_shutdown_t *info);


/*
 *  Wait for all threads to gracefully exit and then release resources
 *  associated with the os_shutdown_t structure.  The procedure will
 *  wait no longer than "seconds" seconds.  A minimum wait of 0.2 seconds
 *  is imposed if there is at least one thread outstanding.
 *
 *  If all threads registered with os_shutdown_thread_incr() have
 *  deregistered themselves with os_shutdown_thread_decr(), then
 *  os_shutdown_finish() will return 0 (ERR_OK) and it will release
 *  the resources associated with the os_shutdown_t structure.
 *  Otherwise, it will return a non-zero value (ERR_NO) and it will NOT
 *  release resources associated with the os_shutdown_t structure so
 *  as to prevent a core should a thread attempt to subsequently use
 *  the structure.
 */

int  os_shutdown_finish(os_shutdown_t *info, unsigned int seconds);
int os_shutdown_wait(os_shutdown_t *info);


void os_shutdown_thread_incr(os_shutdown_t *info);
void os_shutdown_thread_decr(os_shutdown_t *info);
int  os_shutdown_sleep(os_shutdown_t *info, unsigned int milliseconds);

os_pid_t os_getpid(void);

void os_argv_free(os_argv_t *argv);
int os_argv_make(os_argv_t *argv, const char *cmd);

#if defined(_WIN32)
#define os_spawn_init(c,a)
#define os_spawn_free(a)
#else
#define os_spawn_init(c,a) os_argv_make((a),(c))
#define os_spawn_free(a) os_argv_free((a))
#endif

os_pid_t os_spawn_nowait(const char *cmd, os_argv_t *argv, const char *new_env,
  ...);

int os_fexists(const char *fname);

#if defined(__cplusplus)
}
#endif

#endif
