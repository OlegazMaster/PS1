#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#include <time.h>
struct timeval {
    time_t tv_sec;
    long tv_usec;
};
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#define ITIMER_REAL 0
int setitimer(int which, const struct itimerval *v, struct itimerval *old);
#endif
