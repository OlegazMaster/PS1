#ifndef TYPES_H
#define TYPES_H

/* Fixed-size integer types. The PlayStation CPU (R3000A) is 32-bit little
 * endian with no FPU, so int and long are 32 bits and pointers are 32 bits.
 * A future PC build must keep these sizes (pointers stored inside game data
 * are 32-bit). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;

typedef s32 bool32;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define TRUE 1
#define FALSE 0

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* The scratchpad (1 KB of fast RAM, then the I/O ports) is at 0x1F800000.
 * The Android port cannot have memory there (the Java heap is reserved
 * around it) and puts it after the RAM. */
#ifndef PSX_SCRATCH
#if defined(TARGET_PC) && defined(__ANDROID__)
#define PSX_SCRATCH 0x80200000u
#else
#define PSX_SCRATCH 0x1F800000u
#endif
#endif

#endif /* TYPES_H */
