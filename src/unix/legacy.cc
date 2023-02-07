#include <string>

// weird error on linux
#ifdef __THROW
#undef __THROW
#endif
#define __THROW

#ifdef _LIBC
# include <include/sys/stat.h>
#else
# include <sys/stat.h>
#endif
#include <dirent.h>
#include <unistd.h>

#include "../DirTree.hh"
#include "../shared/BruteForceBackend.hh"

#define CONVERT_TIME(ts) ((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec)
#if __APPLE__
#define st_mtim st_mtimespec
#endif
#define ISDOT(a) (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

#ifdef __PASE__
/*
 * IBM i has defective *at functions (only supports AT_FDCWD). These shims were
 * provided to me from IBM, developed for some other software.
 *
 * The shim had openat, unlinkat, and renameat. We need openat and fstatat, so
 * the shim has been modified to provide this set of functions.
 *
 * These were also provided to me so that they could be relicensed under the
 * same license that Watcher is under.
 *
 * Copyright (c) 2022-2023 IBM Corporation
 */

/* This code begins */
/*                                                                                                */
/* File............: atfuncs.cinc                                                                     */
/* Purpose.........: Provide PASE for i alternative functions for unlinkat1, openat, and renameat.*/
/*                                                                                                */
/* Usage Notes.....: These functions work in the following way:                                   */
/*                     (1) If the path is absolute (i.e. starts with '/'), pass the parameters to */
/*                         the non-alternative function (i.e. openat2 -> open, etc.).             */
/*                     (2) If the 'AT_FDCWD' flag is specified as the directory file descriptor,  */
/*                         pass the parameters to the non-alternative function. This usage        */
/*                         relies on underlying support for relative paths in the non-alternative */
/*                         functions. Note that for renameat, both files descriptors must be      */
/*                         supplied the 'AT_FDCWD' flag to perform this usage.                    */
/*                     (3) For openat and unlinkat, if neither (1) or (2) are performed, then     */
/*                         the process's current working directory is generated, and then changed */
/*                         to the supplied directory file descriptor and the non-alternative      */
/*                         function  is called using relative pathnames like (2). For renameat,   */
/*                         the current working directory is generated. For renameat, if a valid   */
/*                         (i.e. not 'AT_FDCWD') is directory file descriptor is supplied, an     */
/*                         absolute file path is generated for that filename.                     */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>

/* Function.......: getcwdpath                                                                    */
/* Purpose........: This function will get the current working directory up to the maximum        */
/*                  pathname length.                                                              */
/*                                                                                                */
/* Inputs.........: const char *, unsigned int, char **, int *                                    */
/*                                                                                                */
/*   buf1(output)....: A pointer to a character buffer that that the current working directory    */
/*                     MAY be placed in. The buffer pointer to is typically allocated from stack  */
/*                     storage.                                                                   */
/*   buf1Size(input): The size in bytes of buf1.                                                  */
/*   buf2(output)...: A pointer to a pointer to a character buffer that MAY point to allocated    */
/*                    storage during this function invocation. The allocated storage will be from */
/*                    the heap storage. The caller is responsible to free this storage.           */
/*   cwdPathLoc(output): A pointer to an integer which determines where the current working       */
/*                       directory output was placed.                                             */
/*                       The integer has the following possible values:                           */
/*                         0 - the output was placed in buf1.                                     */
/*                         1 - the output was placed in buf2. This storage was malloc'd and must  */
/*                             be freed by the caller.                                            */
/*                                                                                                */
/* Return Value....: 0 - success                                                                  */
/*                   -1 - failure, check errno for more information                               */
/*                                                                                                */
/* Notes...........: None                                                                         */
int getcwdpath(const char * buf1, unsigned int buf1Size,
               char ** buf2, int * cwdPathLoc)
{
  if((buf1 == (const char *)NULL) ||       /* safety/sanity check      */
     (cwdPathLoc == (int *)NULL))
  {
    errno = EINVAL;
    return -1;
  }
  
  int curpathrc   = 0; /* default to success.                           */
  *cwdPathLoc = 0;     /* default to passed in buffer.                  */
  
  /* first try to get the cwd path using the supplied buffer            */
  if(getcwd((char *)buf1, buf1Size) == NULL)
  {
    /* failed to get the current working directory.                     */
    /* check if the failure was because the input buffer was too small. */
    
    /* Note that this code exists, but apparently AIX's PATH_MAX limit  */
    /* is 512 bytes which is below the 4096 that is used for the stack  */
    /* buffer in the at-functions. This code will never be executed on  */
    /* AIX. See IBM Documenation for AIX 7.3 for the limits.h header    */
    /* for more information:                                            */
    /* https://www.ibm.com/docs/en/aix/7.3?topic=files-limitsh-file     */
    
    if((errno == ERANGE) ||
       (errno == E2BIG))
    {
      unsigned int buf2Size = buf1Size;
      char * bp             = NULL;
      do
      {
        buf2Size *= 2;                   /* double the buffer size      */
        if(buf2Size > PATH_MAX)          /* if the buffer size is above */
          buf2Size = PATH_MAX;           /* the max, set size to max    */
        
        if(bp != NULL)
          free((void *)bp);              /* free previous buffer        */
        
        bp = (char *)malloc(buf2Size);   /* malloc new storage          */
        if(bp == NULL)
        {
          /* errno will be set by malloc() failure.                     */
          /* Nothing was malloc'd so nothing to free before returning.  */
          curpathrc = -1;
          break;
        }
      /* loop while: */
      /*  getcwd() fails with errno ERANGE or E2BIG, and the current    */
      /*  size of the buffer is less than the max path length.          */
      } while((getcwd(bp, buf2Size) != NULL) &&
              ((errno == ERANGE) || (errno == E2BIG)) &&
              (buf2Size < PATH_MAX));
      
      *buf2 = bp;         /* set output buffer to newly malloc'd space  */
      *cwdPathLoc = 1;    /* set output indicator                       */
    }
    else
    {
      /* getcwd() didn't fail because the buffer was too small.         */
      /* errno will be set by getcwd() failure.                         */
      curpathrc = -1;
    }
  }
  /* else */
  /*   all the work was done by the call to cwd, and the output should  */
  /*   be in buf1.                                                      */
  
  return curpathrc;
}

int ibmi_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
  int _fstatrc = -1;                        /* default to fail          */
  const unsigned int cwdBufSize = 4096;     /* cwd init buffer size     */
  char cwdbuf[cwdBufSize];                  /* cwd init buffer          */
  char ** cwdpath = NULL;                   /* cwd malloc'd path        */
  unsigned int cwdPathLoc = 0;              /* cwd path location        */
  
  /* AT_SYMLINK_NOFOLLOW indicates if lstat or stat */
  int (*underfunc)(const char *, struct stat *) = NULL;
  
  if(pathname == (const char *)NULL)        /* safety/sanity check      */
  {
    errno = EINVAL;
    return -1;
  }
  
  if(flags & AT_SYMLINK_NOFOLLOW)  /* stat on a symbolic link           */
    underfunc = lstat;
  else                      /* stat on a file                           */
    underfunc = stat;
  
  /* path is absolute, stat it.                                         */
  if(*pathname == '/')
    return underfunc(pathname, statbuf);
  
  /* path is relative, stat in current working directory of process.    */
  if(dirfd == AT_FDCWD)
    return underfunc(pathname, statbuf);
  
  /* path is relative, but caller doesn't want to use process's current */
  /* working directory. Build the current working directory, so it      */
  /* can be changed back to.                                            */
  _fstatrc = getcwdpath((const char *)&cwdbuf, cwdBufSize,
                         cwdpath, &cwdPathLoc);
  
  if(_fstatrc == -1)
  {
    /* errno will be set by getcwdpath() failure.                       */
    _fstatrc = -1;
    goto leave;
  }
  
  /* path is relative, change process's current working directory to    */
  /* the directory file descriptor.                                     */
  if(fchdir(dirfd) == -1)
  {
    /* errno will be set by fchdir() failure.                           */
    _fstatrc = -1;
    goto leave;
  }
  
  _fstatrc = underfunc(pathname, statbuf);
  
  /* change working directory back to original process's working        */
  /* directory.                                                         */
  if(cwdPathLoc == 0)
  {
     if(chdir(cwdbuf) == -1)
     {
       /* errno will be set by chdir() failure.                         */
       _fstatrc = -1;
     }
  }
  else
  {
    if(chdir(*cwdpath) == -1)
    {
      /* errno will be set by chdir() failure.                          */
      _fstatrc = -1;
    }
  }
  
leave:
  if(*cwdpath != NULL)  /* if malloc'd storage, free it now.            */
    free(*cwdpath);
    
  return _fstatrc;
}

int ibmi_openat(int dirfd, const char *pathname, int flags)
{
  int _openrc = -1;                         /* default to fail          */
  const unsigned int cwdBufSize = 4096;     /* cwd init buffer size     */
  char cwdbuf[cwdBufSize];                  /* cwd init buffer          */
  char ** cwdpath = NULL;                   /* cwd malloc'd path        */
  unsigned int cwdPathLoc = 0;              /* cwd path location        */
  
  if(pathname == (const char *)NULL)        /* safety/sanity check      */
  {
    errno = EINVAL;
    return -1;
  }
  
  /* path is absolute, open it.                                         */
  if(*pathname == '/')
    return open(pathname, flags);
  
  /* path is relative, open in current working directory of process.    */
  if(dirfd == AT_FDCWD)
    return open(pathname, flags);
  
  /* path is relative, but caller doesn't want to use process's current */
  /* working directory. Build the current working directory, so it      */
  /* can be changed back to.                                            */
  _openrc = getcwdpath((const char *)&cwdbuf, cwdBufSize,
                        cwdpath, &cwdPathLoc);
  
  if(_openrc == -1)
  {
    /* errno will be set by getcwdpath() failure.                       */
    return -1;
  }
  
  /* path is relative, change process's current working directory to    */
  /* the directory file descriptor.                                     */
  if(fchdir(dirfd) == -1)
  {
    _openrc = -1;
    goto leave;
  }
  
  /* path is relative, open it relative to directory file descriptor.  */
  _openrc = open(pathname, flags);
  
  /* change working directory back to original process's working        */
  /* directory.                                                         */
  if(cwdPathLoc == 0)
  {
     if(chdir(cwdbuf) == -1)
     {
       /* errno will be set by chdir() failure.                         */
       _openrc = -1;
     }
  }
  else
  {
    if(chdir(*cwdpath) == -1)
    {
      /* errno will be set by chdir() failure.                          */
      _openrc = -1;
    }
  }
  
leave:
  if(*cwdpath != NULL) /* if malloc'd storage, free it now.             */
    free(*cwdpath);
    
  return _openrc;
}

int ibmi_openat2(int dirfd, const char * pathname, int flags, mode_t mode)
{
  int _openrc = -1;                         /* default to fail          */
  const unsigned int cwdBufSize = 4096;     /* cwd init buffer size     */
  char cwdbuf[cwdBufSize];                  /* cwd init buffer          */
  char ** cwdpath = NULL;                   /* cwd malloc'd path        */
  unsigned int cwdPathLoc = 0;              /* cwd path location        */
  
  if(pathname == (const char *)NULL)        /* safety/sanity check      */
  {
    errno = EINVAL;
    return -1;
  }
  
  /* path is absolute, open it.                                         */
  if(*pathname == '/')
    return open(pathname, flags, mode);
  
  /* path is relative, open in current working directory of process.    */
  if(dirfd == AT_FDCWD)
    return open(pathname, flags, mode);
  
  /* path is relative, but caller doesn't want to use process's current */
  /* working directory. Build the current working directory, so it      */
  /* can be changed back to.                                            */
  _openrc = getcwdpath((const char *)&cwdbuf, cwdBufSize,
                        cwdpath, &cwdPathLoc);
  
  if(_openrc == -1)
  {
    /* errno will be set by getcwdpath() failure.                       */
    return -1;
  }
  
  /* path is relative, change process's current working directory to    */
  /* the directory file descriptor.                                     */
  if(fchdir(dirfd) == -1)
  {
    _openrc = -1;
    goto leave;
  }
  
  /* path is relative, open it relative to directory file descriptor.   */
  _openrc = open(pathname, flags, mode);
  
  /* change working directory back to original process's working        */
  /* directory.                                                         */
  if(cwdPathLoc == 0)
  {
     if(chdir(cwdbuf) == -1)
     {
       /* errno will be set by chdir() failure.                         */
       _openrc = -1;
     }
  }
  else
  {
    if(chdir(*cwdpath) == -1)
    {
      /* errno will be set by chdir() failure.                          */
      _openrc = -1;
    }
  }
  
leave:
  if(*cwdpath != NULL) /* if malloc'd storage, free it now.             */
    free(*cwdpath);
    
  return _openrc;
}

#define fstatat ibmi_fstatat
#define openat ibmi_openat
#endif

void iterateDir(Watcher &watcher, const std::shared_ptr <DirTree> tree, const char *relative, int parent_fd, const std::string &dirname) {
    int open_flags = (O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_NONBLOCK | O_NOFOLLOW);
    int new_fd = openat(parent_fd, relative, open_flags);
    if (new_fd == -1) {
        if (errno == EACCES) {
            return; // ignore insufficient permissions
        }

        throw WatcherError(strerror(errno), &watcher);
    }

    struct stat rootAttributes;
    fstatat(new_fd, ".", &rootAttributes, AT_SYMLINK_NOFOLLOW);
    tree->add(dirname, CONVERT_TIME(rootAttributes.st_mtim), true);

    if (DIR *dir = fdopendir(new_fd)) {
        while (struct dirent *ent = (errno = 0, readdir(dir))) {
            if (ISDOT(ent->d_name)) continue;

            std::string fullPath = dirname + "/" + ent->d_name;

            if (!watcher.isIgnored(fullPath)) {
                struct stat attrib;
                fstatat(new_fd, ent->d_name, &attrib, AT_SYMLINK_NOFOLLOW);
                bool isDir = ent->d_type == DT_DIR;

                if (isDir) {
                    iterateDir(watcher, tree, ent->d_name, new_fd, fullPath);
                } else {
                    tree->add(fullPath, CONVERT_TIME(attrib.st_mtim), isDir);
                }
            }
        }

        closedir(dir);
    } else {
        close(new_fd);
    }

    if (errno) {
        throw WatcherError(strerror(errno), &watcher);
    }
}

void BruteForceBackend::readTree(Watcher &watcher, std::shared_ptr <DirTree> tree) {
    int fd = open(watcher.mDir.c_str(), O_RDONLY);
    if (fd) {
        iterateDir(watcher, tree, ".", fd, watcher.mDir);
        close(fd);
    }
}
