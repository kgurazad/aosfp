// hi guys!

#include "globals.h"

typedef uint64_t req_n;

void *bufs[UQ_DEPTH]; // one for each request
int bufstats[UQ_DEPTH];
struct io_uring ring;
int submitted;
int completed;

struct req_t {
    int fd;
    int block;
    int buf;
    bool is_read;
};

req_n deflate_req (struct req_t *r) {
    req_n ret = 0;
    ret += r->fd;
    ret = ret << 24;
    ret += r->block;
    ret = ret << 20;
    ret += r->buf;
    ret = ret << 1;
    ret += r->is_read ? 1 : 0;
    return ret;
}

void inflate_req (req_n n, struct req_t *r) {
    r->is_read = n & 1;
    n = n >> 1;
    r->buf = n & 0xFFFFF;
    n = n >> 20;
    r->block = n & 0xFFFFFF;
    n = n >> 24;
    r->fd = n;
}

struct io_uring_sqe *get_sqe () {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    while (sqe == NULL) {
        io_uring_submit(&ring);
        handle_cq(true);
        sqe = io_uring_get_sqe(&ring);
    }
    return sqe;
}

void submit_write (struct req_t *r, int sz) {
    struct io_uring_sqe *sqe = get_sqe();
    sqe->user_data = deflate_req(r);
    io_uring_prep_write(sqe, r->fd, bufs[r->buf], sz, r->block * BLOCK);
    submitted++;
}

void handle_cq (bool block) {
    if (completed == submitted) { return; }

    struct io_uring_cqe *cqe;
    if (block) {
        io_uring_wait_cqe(&ring, &cqe);
    } else {
        io_uring_peek_cqe(&ring, &cqe);
    }

    assert(cqe); // lol
    completed++;
    
    struct req_t r;
    inflate_req(cqe->user_data, &r);

    if (r.is_read) {
        int wsz = cqe->res;
        io_uring_cqe_seen(&ring, cqe);
        bufstats[r.buf] = KWRITE;
        r.is_read = false;
        submit_write(&r, wsz);
    } else {
        bufstats[r.buf] = KFREE;
    }
}

int get_buf (int suggested) {
    if (bufstats[suggested] == KFREE) {
        return suggested;
    }

    if (suggested & 1) {
        for (int i = UQ_DEPTH - 1; i >= 0; i--) {
            if (bufstats[i] == KFREE) {
                return i;
            }
        }
    } else {
        for (int i = 0; i < UQ_DEPTH; i++) {
            if (bufstats[i] == KFREE) {
                return i;
            }
        }
    }

    eprintf("i thought buffer accounting was supposed to work??\n");
    assert(false);
    return 1;
}

void submit_read (struct req_t *r) {
    if (submitted - completed > UQ_DEPTH / 2) {
        handle_cq(true);
    }
    
    assert(r->is_read);
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (r->buf == UQ_DEPTH) {
        int bufid = get_buf(r->block);
        while (bufid == UQ_DEPTH) {
            handle_cq(true);
        }
        r->buf = bufid;
    }
    sqe->user_data = deflate_req(r);
    bufstats[r->buf] = KREAD;
    io_uring_prep_read(sqe, r->fd, bufs[r->buf], BLOCK, r->block * BLOCK);
    submitted++;
}
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

int create_dir (char *path, mode_t mode) {
    // oh fuck we need to maintain global io_uring state
    // uhhh
    // typo!!

#ifdef USE_IO_URING
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    return sqe ? io_uring_prep_mkdir(sqe, dst, s_src.st_mode & 0777), 0 : 1;
#else
    // oh jeez forgot about permissions
    // welp pass them in i guess
    return mkdir(path, mode & 0777) != 0 ? errno : 0;
    // u suck
#endif
    
    // yknow... someone has to run these requests...
    // fine, pass that flag to uring init
    // this is extemely dangerous lmao lets just do it later
    // get the preprocessor control flow cuffs on his ass!
    // nooo i didnt do anything i was just trying to increase efficiency loll
}

// lame fstat wrapper
mode_t get_mode (int fd) {
    struct stat s;
    assert(fstat(fd, &s) == 0);
    return s.st_mode;
}

void fcp_uring2 (int sfd, int dfd) {
    struct stat ssrc;
    assert(fstat(sfd, &ssrc) == 0);
    int len = ssrc.st_size;

    int nreads = len / BLOCK;
    if (len % BLOCK) { nreads++; }
    
    for (int i = 0; i < nreads; i++) {
        struct req_t r = {
            .fd = sfd,
            .buf = UQ_DEPTH,
            .block = i,
            .is_read = true
        };
        submit_read(&r);
    }

    if (completed < submitted) {
        io_uring_submit(&ring);
    }
}

int copy (char *src, char *dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) { return errno; }

    int src_len = strlen(src);
    int dst_len = strlen(dst);
    
    int err = 0;
    if (err = create_dir(dst, get_mode(src_fd))) { return err; }

    DIR *src_dir = fdopendir(src_fd);
    assert(src_dir);
    for (struct dirent *dent = readdir(src_dir); dent; dent = readdir(src_dir)) {
        if (dent->d_type == DT_REG || dent->d_type == DT_DIR) {
            if (dent->d_name[0] == '.') { continue; }
            
            int src_fpath_len = src_len + 2 + strlen(dent->d_name);
            char *src_fpath = malloc(src_fpath_len);
            snprintf(src_fpath, src_fpath_len, "%s/%s", src, dent->d_name);
            
            int dst_fpath_len = dst_len + 2 + strlen(dent->d_name);
            char *dst_fpath = malloc(dst_fpath_len);
            snprintf(dst_fpath, dst_fpath_len, "%s/%s", dst, dent->d_name);

            if (dent->d_type == DT_REG) {
                int src_ffd = open(src_fpath, O_RDONLY);
                if (src_ffd < 0) { return errno; }
                
                int dst_ffd = open(dst_fpath, O_WRONLY | O_CREAT, get_mode(src_ffd) & 0777);
                if (dst_ffd < 0) { return errno; }

                fcp_uring2(src_ffd, dst_ffd);
            } else {
                err = copy(src_fpath, dst_fpath);
                if (err) { return err; }
            }
        }
    }

    if (completed < submitted) {
        io_uring_submit(&ring);
    }

    return 0;
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
        int dst_len = strlen(dst) + 2 + strlen(dirname);
        char *dst_ = malloc(dst_len);
        snprintf(dst_, dst_len, "%s/%s", dst, dirname);
    }

    // TODO use iuqi_params to get interesting results should time permit
    if (io_uring_queue_init(UQ_DEPTH, &ring, 0) != 0) {
        perror(argv[0]);
        return 1;
    }

    // END SETUP

    void *bufbacker = mmap(0, BLOCK * UQ_DEPTH, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE | MAP_ANONYMOUS, 0, 0);
    for (int i = 0; i < UQ_DEPTH; i++) {
        bufs[i] = bufbacker + BLOCK*i;
    }
    
    int err = copy(src, dst);
    while (completed < submitted) { io_uring_submit(&ring); handle_cq(true); }
    return err;
}
