#ifndef _globals_h_
#define _globals_h_

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <linux/io_uring.h>
#include <liburing.h>
#include <sys/mman.h>
#include <assert.h> // bravo, great programming going on here!
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BLOCK
#define BLOCK 4096
#endif

#ifndef UQ_DEPTH
#define UQ_DEPTH 256
#endif

#ifndef SQ_DEPTH
#define SQ_DEPTH UQ_DEPTH
#endif

#ifndef CQ_DEPTH
#define CQ_DEPTH UQ_DEPTH
#endif

#define eprintf(...) fprintf (stderr, __VA_ARGS__)

#ifdef VERBOSE
#define DEBUG(...) printf (__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define KFREE 0
#define KREAD 1
#define KWRITE 2

extern struct io_uring ring;

extern void *bufs[UQ_DEPTH];
extern int bufstats[UQ_DEPTH];

#endif
