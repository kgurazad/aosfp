#ifndef _globals_h_
#define _globals_h_

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

#define KFREE 0
#define KREAD 1
#define KWRITE 2

extern struct io_uring ring;

extern void *bufs[UQ_DEPTH];
extern int bufstats[UQ_DEPTH];

#endif
