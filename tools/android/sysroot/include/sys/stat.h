/* the part of <sys/stat.h> of bionic the port uses */
#ifndef _SYS_STAT_H_
#define _SYS_STAT_H_

typedef unsigned int mode_t;

int mkdir(const char *path, mode_t mode);

#endif
