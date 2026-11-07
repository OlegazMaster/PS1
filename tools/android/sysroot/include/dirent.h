#ifndef _DIRENT_H
#define _DIRENT_H
#include <stdint.h>
/* bionic: 64-bit inode and offset on LP32 too */
struct dirent {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};
typedef struct DIR DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#endif
