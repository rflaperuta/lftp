/*
 * lftp and utils
 *
 * Copyright (c) 1996-1997 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id$ */

#include <config.h>
#include "xmalloc.h"
#include "xstring.h"
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include "misc.h"
#include "ProcWait.h"
#include "SignalHook.h"

const char *dir_file(const char *dir,const char *file)
{
   if(dir==0 || dir[0]==0)
      return file;
   if(file==0 || file[0]==0)
      return dir;
   if(file[0]=='/')
      return file;

   static char *buf=0;
   static int buf_size=0;

   if(buf && dir==buf) // it is possible to dir_file(dir_file(dir,dir),file)
      dir=alloca_strdup(dir);

   int len=strlen(dir)+1+strlen(file)+1;
   if(buf_size<len)
      buf=(char*)xrealloc(buf,buf_size=len);
   len=strlen(dir);
   if(len==0)
      sprintf(buf,"%s",file);
   else if(dir[len-1]=='/')
      sprintf(buf,"%s%s",dir,file);
   else
      sprintf(buf,"%s/%s",dir,file);
   return buf;
}


const char *basename_ptr(const char *s)
{
   const char *s1=s+strlen(s);
   while(s1>s && s1[-1]=='/')
      s1--;
   while(s1>s && s1[-1]!='/')
      s1--;
   return s1;
}

const char *expand_home_relative(const char *s)
{
   if(s[0]=='~')
   {
      const char *home=0;
      const char *sl=strchr(s+1,'/');;
      static char *ret_path=0;

      if(s[1]==0 || s[1]=='/')
      {
	 home=getenv("HOME");
      }
      else
      {
	 // extract user name and find the home
	 int name_len=(sl?sl-s-1:strlen(s+1));
	 char *name=(char*)alloca(name_len+1);
	 strncpy(name,s+1,name_len);
	 name[name_len]=0;

	 struct passwd *pw=getpwnam(name);
	 if(pw)
	    home=pw->pw_dir;
      }
      if(home==0)
	 return s;

      if(sl)
      {
	 ret_path=(char*)xrealloc(ret_path,strlen(sl)+strlen(home)+1);
	 strcpy(ret_path,home);
	 strcat(ret_path,sl);
	 return ret_path;
      }
      return home;
   }
   return s;
}

int   create_directories(char *path)
{
   char  *sl=path;
   int	 res;

   if(access(path,0)==0)
      return 0;

   for(;;)
   {
      sl=strchr(sl,'/');
      if(sl==path)
      {
	 sl++;
	 continue;
      }
      if(sl)
	 *sl=0;
      if(access(path,0)==-1)
      {
	 res=mkdir(path,0777);
	 if(res==-1)
	 {
	    if(errno!=EEXIST)
	    {
	       fprintf(stderr,"mkdir(%s): %s\n",path,strerror(errno));
	       if(sl)
		  *sl='/';
	       return(-1);
	    }
	 }
      }
      if(sl)
	 *sl++='/';
      else
	 break;
   }
   return 0;
}

void  truncate_file_tree(const char *dir)
{
   fflush(stderr);
   pid_t pid;
   switch(pid=fork())
   {
   case(0): // child
      SignalHook::Ignore(SIGINT);
      SignalHook::Ignore(SIGTSTP);
      SignalHook::Ignore(SIGQUIT);
      SignalHook::Ignore(SIGHUP);
      execlp("rm","rm","-rf",dir,NULL);
      perror("execlp(rm)");
      fflush(stderr);
      _exit(1);
   case(-1):   // error
      perror("fork()");
      return;
   default: // parent
      (new ProcWait(pid))->Auto();  // don't wait for termination
   }
}

char *xgetcwd()
{
   int size=256;
   for(;;)
   {
      char *cwd=getcwd(0,size);
      if(cwd)
	 return cwd;
      if(errno!=ERANGE)
	 return 0;
      size*=2;
   }
}

int parse_perms(const char *s)
{
   int p=0;

   if(strlen(s)!=9)
      bad: return -1;

   switch(s[0])
   {
   case('r'): p|=S_IRUSR; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[1])
   {
   case('w'): p|=S_IWUSR; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[2])
   {
   case('S'): p|=S_ISUID; break;
   case('s'): p|=S_ISUID; // fall-through
   case('x'): p|=S_IXUSR; break;
   case('-'): break;
   default: goto bad;
   }
   s+=3;
   switch(s[0])
   {
   case('r'): p|=S_IRGRP; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[1])
   {
   case('w'): p|=S_IWGRP; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[2])
   {
   case('S'): p|=S_ISGID; break;
   case('s'): p|=S_ISGID; // fall-through
   case('x'): p|=S_IXGRP; break;
   case('-'): break;
   default: goto bad;
   }
   s+=3;
   switch(s[0])
   {
   case('r'): p|=S_IROTH; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[1])
   {
   case('w'): p|=S_IWOTH; break;
   case('-'): break;
   default: goto bad;
   }
   switch(s[2])
   {
   case('T'): case('t'): p|=S_ISVTX; break;
   case('l'): case('L'): p|=S_ISGID; p&=~S_IXGRP; break;
   case('x'): p|=S_IXOTH; break;
   case('-'): break;
   default: goto bad;
   }

   return p;
}

int parse_month(const char *m)
{
   static const char *months[]={
      "Jan","Feb","Mar","Apr","May","Jun",
      "Jul","Aug","Sep","Oct","Nov","Dec",0
   };
   for(int i=0; months[i]; i++)
      if(!strcasecmp(months[i],m))
	 return(i%12);
   return -1;
}

int parse_year_or_time(const char *year_or_time,int *year,int *hour,int *minute)
{
   if(year_or_time[2]==':')
   {
      if(2!=sscanf(year_or_time,"%2d:%2d",hour,minute))
	 return -1;
      *year=-1;
   }
   else
   {
      if(1!=sscanf(year_or_time,"%d",year))
	 return -1;;
      *hour=*minute=0;
   }
   return 0;
}
int guess_year(int month,int day,int hour,int minute)
{
   time_t curr=time(0);
   struct tm &now=*localtime(&curr);
   int year=now.tm_year+1900;
   if(((month     *32+        day)*64+       hour)*64+       minute
    > ((now.tm_mon*32+now.tm_mday)*64+now.tm_hour)*64+now.tm_min)
      year--;
   return year;
}
