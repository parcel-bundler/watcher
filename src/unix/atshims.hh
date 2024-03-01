#ifdef __PASE__
/* see atshims.cc for purpose */

int ibmi_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
int ibmi_openat(int dirfd, const char *pathname, int flags);

#define fstatat ibmi_fstatat
#define openat ibmi_openat
#endif
