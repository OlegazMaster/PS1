#ifndef _SIGNAL_H
#define _SIGNAL_H
/* bionic on LP32: a 32-bit sigset_t and the kernel's struct sigaction */
typedef unsigned long sigset_t;
struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};
#define SIGALRM 14
#define SA_RESTART 0x10000000
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
int sigaction(int sig, const struct sigaction *act, struct sigaction *old);
int sigprocmask(int how, const sigset_t *set, sigset_t *old);
int pthread_sigmask(int how, const sigset_t *set, sigset_t *old);
int sigemptyset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
#endif
