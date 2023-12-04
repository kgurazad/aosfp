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

void clear_trailing_slash (char *path) {
    int len = strlen(path);
    if (path[len - 2] == '/') {
        bzero(path + len - 2, 1);
    }
}

char *get_dirname (char *path) {
    int len = strlen(path);
    char *ret;
    for (ret = path + len - 1; ret != path && *(ret - 1) != '/'; ret--);
    return ret;
}

int main (int argc, char **argv) {
    if (argc != 3) {
        return 1;
    }
    
    char *src = argv[1];
    char *dst = argv[2];
    clear_trailing_slash(src);
    clear_trailing_slash(dst);
    
    struct stat s_src;
    if (stat(src, &s_src) == 0) {
        if (!(s_src.st_mode & S_IFDIR)) {
            eprintf("%s: first argument should be a directory\n", argv[0]);
        }
    } else {
        perror(argv[0]);
        return 1;
    }

    struct stat s_dst;
    if (stat(dst, &s_dst) == 0) {
        if (!(s_dst.st_mode & S_IFDIR)) {
            eprintf("%s: second argument should be a directory\n", argv[0]);
        }

        char *dirname = get_dirname(src);
        int dst_len = strlen(dst) + 1 + strlen(dirname);
        char *dst_ = malloc(dst_len);
        snprintf(dst_, dst_len, "%s/%s", dst, dirname);
    }

    // dst is the final path for the root directory
    // dst 100% does not exist
    // create it... with io_uring!

    /*
    if (mkdir(dst, s_src.st_mode & 0777)) {
        perror(argv[0]);
        return 1;
    }
    */
    
    // well before we can do that we have to setup
    /*
    struct io_uring_params sq_params = {
        .sq_entries = SQ_DEPTH,
        .cq_entries = CQ_DEPTH,
        .flags = TODO,
        .sq_thread_cpu = TODO,
        .sq_thread_idle = TODO,
        .features = TODO,
        .wq_fd = TODO,
        .resv = TODO,
        .io_sqring_offsets = TODO,
        .io_cqring_offsets = TODO
    };
    */

    /*
    struct io_uring_params uq_params;
    bzero(&uq_params, sizeof(struct io_uring_params));
    int uq_fd = io_uring_setup(UQ_DEPTH, &uq_params);
    if (uq_fd < 0) {
        eprintf("could not get uq_fd, retcode %d\n", uq_fd);
        return 1;
    }

    int sq_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cq_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    void *sq_ptr = mmap(0, sq_sz, PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_POPULATE,
                        uq_fd, IORING_OFF_SQ_RING);
    void *cq_ptr = mmap(0, cq_sz, PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_POPULATE,
                        uq_fd, IORING_OFF_CQ_RING);
    
    void *sq_ent = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                        s->ring_fd, IORING_OFF_SQES);
    */
    struct io_uring ring;
    // TODO use iuqi_params to get interesting results should time permit
    if (io_uring_queue_init(UQ_DEPTH, &ring, 0) != 0) {
        perror(argv[0]);
        return 1;
    }
    
    struct io_uring_sqe *mk = io_uring_get_sqe(&ring);
    io_uring_prep_mkdir(mk, dst, s_src.st_mode & 0777);
    printf("%d requests completed\n", io_uring_submit_and_wait(&ring, 1));
}
