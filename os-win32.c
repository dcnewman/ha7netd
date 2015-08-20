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
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <io.h>
#include <string.h>
#include <process.h>

int
os_sleep(unsigned int milliseconds)
{
     Sleep((DWORD)(0xffff & milliseconds));
     return(0);
}


const char *
os_basename(const char *path)
{
     const char *ptr = path;
     while (*path)
     {
	  if ((*path == ':' || *path == '/' || *path == '\\') && path[1])
	       ptr = ++path;
	  else
	       ++path;
     }
}


int
os_fexists(const char *fname)
{
     if (!fname)
	  return(0);
     return((0 == _access(fname, 0)) ? 1 : 0);
}


const char *
os_tzone(long *gmt_offset, const char **tzone, char buf, size_t buflen)
{
     WCHAR *name;
     long tm_offset;
     TIME_ZONE_INFORMATION zinfo;
    
     istat = GetTimeZoneInformation(&zinfo);
     tm_offset = zinfo.Bias;

     if (istat != TIME_ZONE_ID_STANDARD)
     {
	  name = zinfo.DaylightName
	  tm_offset += zone.DaylightBias;
     }
     else
     {
	  name = zinfo.StandardName;
	  tm_offset += zinfo.StandardBias;
     }
     tm_offset *= -60;  /* Convert to seconds */
     len = WideCharToMultiByte(CP_UTF8, 0, name, -1, buf, buflen, -1,
			       NULL, NULL);
     if (len <= 0)
	  buf[0] = '\0';

     if (gmt_offset)
	  *gmt_offset = tm_offset;
     if (tzone)
	  *tzone = buf;

     return(buf);
}


int
os_daemonize(int argc, char **argv, const char *extra_arg)
{
     pid_t pid;

     if ((pid = FORK()) < 0)
	  return(-1);
     else if (pid)
	  /*
	   *  Parent goes bye-bye
	   */
	  _EXIT(0);

     /*
      *  Daemon child continues
      */
#if !defined(__MUST_EXEC)
     return(0);
#else
     {
	  char **argv2;
	  int i, istat;

	  /*
	   *  Need to do an exec*() call
	   */

	  /*
	   *  Build a new argument list which contains an extra argument
	   */
	  argv2 = (char **)malloc(sizeof(char *) * (argc + 2));
	  if (!argv2)
	       return(-1);

	  /*
	   *  Copy the original argument list
	   */
	  for (i = 0; i < argc; i++)
	       argv2[i] = (char *)argv[i];

	  /*
	   *  Add in the extra argument
	   */
	  argv2[argc] = (char *)extra_arg;

	  /*
	   *  And terminate the list
	   */
	  argv2[argc+1] = NULL;

	  /*
	   *  And exec ourself
	   */
	  istat = execve(argv[0], argv2, environ);
	  if (istat != -1)
	       _EXIT(0);

	  /*
	   *  If we're here, there's been an error-o-la
	   */
	  {
	       int save_errno = errno;
	       free(argv2);
	       errno = save_errno;
	       return(-1);
	  }
     }
#endif
}


void
os_server_start_1(const char *wdir, int bg)
{
     /*
      *  Step 2 of daemonizing: to be done by the daemon child
      *  Note that we don't change the uid and gid until
      *  after we've opened our configuration file.  That way
      *  we can handle the Good Case whereby the daemonized
      *  server runs under a UID and GID which cannot write
      *  to its own configuration file.
      */
     if (wdir)
	  chdir(wdir);
}


int
os_server_start_2(const char *user, int close_stdfiles)
{
     /*
      *  Close all open files but stdout & stderr
      */
     if (close_stdfiles)
     {
	  if (stderr)
	  {
	       fflush(stderr);
	       fclose(stderr);
	  }
	  else
	  {
	       fsync(2);
	       close(2);
	  }
	  if (stdout)
	  {
	       fflush(stdout);
	       fclose(stdout);
	  }
	  else
	  {
	       fsync(1);
	       close(1);
	  }
	  if (stdin)
	       fclose(stdin);
	  else
	       close(0);
	  os_close_files(3, 20);
     }

     return(0);
}


static int iEventLogOpened = -1;
static HANDLE hEventLog    = NULL;

void
os_log_close(void *facility)
{
     if (facility)
	  free((char *)facility);

     if (hEventLog)
     {
	  DeregisterEventSource(hEventLog);
	  hEventLog = NULL;
     }
     iEventLogOpened = -1;
}


void *
os_log_open(const char *facility)
{
     return(strdup(facility ? facility : "(unknown)"));
}


void
os_log(void *facility, int iSeverity, const char *lpszFmt, va_list ap)
{
     DWORD dwId;
     char szMsg[4096], *lpszMsgArray[1];
     WORD wType;

     if (lpszFmt)
	  return;

     /*
      *  iEventLogOpened == -1 -> Try to open the event log
      *                  ==  0 -> Attempt to open event log failed; give up
      *                  ==  1 -> Event log is open for business
      */
     if (!iEventLogOpened)
	  return;
     else if (iEventLogOpened == -1)
     {
	  /*
	   *  Open the event log
	   */
	  hEventLog = RegisterEventSource(NULL, (char *)facility);
	  if (!hEventLog)
	  {
	       iEventLogOpened = 0;
	       return;
	  }
	  else
	       iEventLogOpened = 1;
     }
     /*
      *  _LOG_SUCCESS, _LOG_ERROR, _LOG_WARNING, and _LOG_INFO
      *  are identifiers for message strings in our *.mc file.
      *  That file is compiled by the DOS MC utility to produce
      *  a resource file as well as madman_win32_msg.h which
      *  defines these _LOG_* values.
      */
     switch(iSeverity)
     {
     case default :
     case ERR_LOG_ERR :
	  wType = EVENTLOG_ERROR_TYPE;
	  dwId = _LOG_ERROR;
	  break;

	  break;

     case ERR_LOG_DEBUG :
	  wType = EVENTLOG_INFORMATION_TYPE;
	  dwId = _LOG_INFO;
	  break;
     }
     _vsnprintf(szMsg, sizeof(szMsg), lpszFmt, ap);
     lpszMsgArray[0] = szMsg;
     ReportEvent(hEventLog, wType, 0, dwId, NULL, 1, 0,
		 (LPTSTR *)lpszMsgArray, NULL); 
}


pid_t
os_spawn_nowait(const char *cmd, argv_t *argv, const char *new_env, ...)
{
     int istat;
     size_t len;
     char *new_environ;
     pid_t pid;
     PROCESS_INFORMATION procinfo;
     STARTUPINFO startinfo;

     /*
      *  Sanity checks
      */
     if (!argv || !argv->argv[0] || !argv->argv[0][0] || !argv->argc)
     {
	  errno = EFAULT;
	  return((pid_t)-1);
     }

     /*
      *  Add to the environment?
      */
     new_environ = NULL;
     if (env_val)
     {
	  /*
	   *  First see how much space we need for the current environ
	   *  We will compute a value larger than we need as we won't
	   *  bother to check to see if we're being asked to replace
	   *  any values.  We assume that all the values will be added.
	   */
	  va_list ap;
	  const char **env;
	  char *nam, *val;
	  extern char **environ;

	  /*
	   *  Add up the amount of space needed for the current
	   *  environment list
	   */
	  len = 1;  /* closing block terminator is a NUL */
	  for (env = environ; *env != NULL; env++)
	       len += 1 + strlen(*env);

	  /*
	   *  And the space needed for the new environment variables
	   */
	  va_start(ap, env_val);
	  nam = env_val;
	  while (nam)
	  {
	       val = va_next(ap, const char *);
	       if (!val)
	       {
		    errno = EFAULT;
		    return((pid_t)-1);
	       }
	       /* 
		*  <name> "=" <val> "\0"
		*/
	       len += strlen(nam) + 1 + strlen(val) + 1;
	       nam = va_next(ap, const char *);
	  }
	  va_end(ap);

	  /*
	   *  Allocate memory for the new environment block: we here
	   *  allocate too much when new environment variables will
	   *  be replacing old ones.
	   */
	  new_environ = (char **)malloc(len);
	  if (!new_environ)
	  {
	       errno = ENOMEM;
	       return((pid_t)-1);
	  }

	  /*
	   *  Copy the old environment to the new environment
	   *  We assume that no other threads have just enlarged
	   *  the environ list!!!!
	   *
	   *  Our insertion process below assumes that the caller
	   *  supplied list of new environment variables is
	   *  alphabetized by env. variable name.
	   */
	  va_start(ap, env_val);
	  nam = env_val;
	  ptr = new_environ;
	  for (env = environ; *env != NULL; env++)
	  {
	       char *p;

	       if (!(p = strchr(*env, '=')))
		    /*
		     *  Skip this as it is not of the form name=value
		     */
		    continue;
	  next_nam:
	       if (nam)
	       {
		    cmp = _memicmp(env, nam, p - *env);
		    if (cmp >= 0)
		    {
			 /* 
			  *  cmp = 0: do a replace
			  *  cmp > 0: do an insert before
			  *
			  *  YES, we're here relying upon the list being
			  *  alphabetized....
			  */

			 /*
			  *  Insert <name> "="
			  */
			 len = strlen(nam);
			 memcpy(ptr, nam, len);
			 ptr += len;
			 *ptr++ = '=';

			 /*
			  *  Insert <value> "\0"
			  */
			 val = va_next(ap, const char *);
			 len = strlen(val);
			 memcpy(ptr, val, len + 1);
			 ptr += len + 1;

			 /*
			  *  Prepare for the next <name>
			  */
			 nam = va_next(ap, const char *);
			 if (cmp)
			      /*
			       *  We did an insert: see if there are any
			       *  other new values to push in before this
			       *  one.
			       */
			      goto next_nam;
			 else
			      /*
			       *  We did a replace.  Move on to the next,
			       *  pre-existing environment variable.
			       */
			      continue;
		    }
	       }

	       /*
		*  Just copy this pre-existing environment variable
		*/
	       len = 1 + strlen(*env);
	       memcpy(ptr, *env, len);
	       ptr += len;
	  }

	  /*
	   *  We missed some: means that all the stuff in the original
	   *  environ had no "=" in it or the list of new environment
	   *  variables was not in alphabetical order.
	   */
	  while(nam)
	  {
	       /*
		*  Copy <name> "="
		*/
	       len = strlen(nam);
	       memcpy(ptr, nam, len);
	       ptr += len;
	       *ptr++ = '=';

	       /*
		*  Copy <value> "\0"
		*/
	       val = va_next(ap, const char *);
	       len = strlen(val);
	       memcpy(ptr, val, len + 1);
	       ptr += len;

	       /*
		*  And move on to the next <name>
		*/
	       nam = va_next(ap, const char *);
	  }
	  va_end(ap);

	  /*
	   *  Final terminator
	   */
	  *ptr = '\0';
     }

     /*
      *  Prepare to create the new process
      */
     startinfo.cb = sizeof(startinfo);
     GetStartupInfo(&startinfo);

     istat = strlen(argv->argv[0]) - 4;
     if (istat >= 0 && !memicmp(argv->argv[0] + istat, ".exe", 4))
	  /*
	   *  We're launching a .EXE
	   */
	  istat = CreateProcess(argv->argv[0], cmd, NULL, NULL, FALSE,
				NORMAL_PRIORITY_CLASS, new_environ, NULL,
				&startinfo, &procinfo);
     else
     {
	  /*
	   *  Need to fire up an interpreter....
	   */
	  size_t len;
	  char *p, *cmd1 *cmd2;

	  p = getenv("SystemRoot");
	  len = p ? strlen(p) : 0;
	  cmd1 = (char *)malloc(len + 20);
	  if (!cmd1)
	  {
	       if (new_environ)
		    free(new_environ);
	       errno = ENOMEM;
	       return((pid_t)-1);
	  }
	  if (len)
	       strcpy(cmd1, p);
	  else
	       *cmd1 = '\0';
	  strcat(cmd1, "\\system32\\cmd.exe");

	  len = strlen(cmd);
	  cmd2 = (char *)malloc(len + 32);
	  if (!cmd2)
	  {
	       free(cmd1);
	       if (new_environ)
		    free(new_environ);
	       errno = ENOMEM;
	       return((pid_t)-1);
	  }
	  strcpy(cmd2, "cmd /a /q /c ");
	  strcat(cmd2, cmd);
	  istat = CreateProcess(cmd1, cmd2, NULL, NULL, FALSE,
				NORMAL_PRIORITY_CLASS, new_environ, NULL,
				&startinfo, &procinfo);
	  free(cmd1);
	  free(cmd2);
     }
     if (istat)
     {
	  pid = procinfo.dwProcessId;
	  CloseHandle(procinfo.hProcess);
	  CloseHandle(procinfo.hThread);
     }
     else
	  pid = (pid_t)-1;
     if (new_environment)
	  free(new_environment);

     /*
      *  And return
      */
     return(pid);
}
