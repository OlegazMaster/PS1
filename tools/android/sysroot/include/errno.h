#ifndef _ERRNO_H
#define _ERRNO_H
int *__errno(void);
#define errno (*__errno())
#define EPERM 1
#define ENOENT 2
#define EACCES 13
#endif
