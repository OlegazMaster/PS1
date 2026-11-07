#ifndef _TIME_H
#define _TIME_H
typedef long time_t;
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
#define CLOCK_MONOTONIC 1
int clock_gettime(int clock, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
#endif
