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

#include "globals.h"

void fcp_uring (int sfd, int dfd) {
    struct stat ssrc;
    assert(fstat(sfd, &ssrc) == 0);
    int len = ssrc.st_size;

    int nreads_ = len / BLOCK;
    if (len % BLOCK) { nreads_++; }
    int nreads = nreads_;

    int nhandled = 0;
    
    for (int nl = 0; nreads; nl++) {
        // read from src
        int nreads_submitted = 0;
        for (int i = 0; i < UQ_DEPTH && nreads; i++, nreads--, nreads_submitted++) {
            int read_number = nl * UQ_DEPTH + i;
            int offset = BLOCK * read_number;
            // might be faster to use readv but who cares rn just get it done
            if (nl) { // is a write done w a buf?
                // idk if this or if allocating new bufs every time is stupider
                struct io_uring_cqe *cqe;
                assert(io_uring_wait_cqe(&ring, &cqe) == 0);
                int write_number = (cqe->user_data) >> 1;
                int bufind = write_number % UQ_DEPTH;
                assert(bufstats[bufind] == KWRITE);
                bufstats[bufind] = KREAD;
                // printf("cqe n %d %d ret %d\n", cqe->user_data >> 1, cqe->user_data & 1, cqe->res);
                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, sfd, bufs[bufind], BLOCK, offset);
                sqe->user_data = (read_number) << 1;
                io_uring_cqe_seen(&ring, cqe);
                nhandled++;
            } else {
                assert(bufstats[i] == KFREE);
                bufstats[i] = KREAD;
                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe, sfd, bufs[i], BLOCK, offset);
                sqe->user_data = (read_number) << 1;
            }
        }

        // another kernel thread is cheating i think
        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            eprintf("yikes! ret %d errno %d\n", ret, errno);
            return 0;
        } else {
            // printf("submitted %d\n", ret);
        }

        for (int i = 0; i < nreads_submitted; i++) {
            struct io_uring_cqe *cqe;
            assert(io_uring_wait_cqe(&ring, &cqe) == 0);
            int read_number = (cqe->user_data) >> 1;
            int bufind = read_number % UQ_DEPTH;
            assert(bufstats[bufind] == KREAD);
            bufstats[bufind] = KWRITE;
            int offset = BLOCK * read_number;
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            io_uring_prep_write(sqe, dfd, bufs[bufind], cqe->res, offset);
            sqe->user_data = cqe->user_data + 1;
            // printf("cqe n %d %d ret %d\n", cqe->user_data >> 1, cqe->user_data & 1, cqe->res);
            io_uring_cqe_seen(&ring, cqe);
        }

        ret = io_uring_submit(&ring);
        if (ret < 0) {
            eprintf("yikes! ret %d errno %d\n", ret, errno);
            return 1;
        } else {
            // printf("submitted %d\n", ret);
        }
    }

    while (nhandled < nreads_) {
        struct io_uring_cqe *cqe;
        assert(io_uring_wait_cqe(&ring, &cqe) == 0);
        // printf("cqe n %d %d ret %d\n", cqe->user_data >> 1, cqe->user_data & 1, cqe->res);
        io_uring_cqe_seen(&ring, cqe);
        nhandled++;
    }

    for (int i = 0; i < UQ_DEPTH; i++) {
        bufstats[i] = KFREE;
    }
    
    return 0;
}
