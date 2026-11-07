#ifndef _PTHREAD_H
#define _PTHREAD_H
typedef long pthread_t;
typedef struct {
    unsigned int flags;
    void *stack_base;
    unsigned int stack_size;
    unsigned int guard_size;
    int sched_policy;
    int sched_priority;
} pthread_attr_t;
int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_setstacksize(pthread_attr_t *a, unsigned int size);
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*func)(void *), void *arg);
int pthread_kill(pthread_t t, int sig);
pthread_t pthread_self(void);
#endif
