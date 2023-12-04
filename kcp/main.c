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

struct io_uring ring;

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

int copy (char *src, char *dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) { return errno; }

    int src_len = strlen(src); // critical
    int dst_len = strlen(dst); // critical
    
    int err = 0;
    if (err = create_dir(dst, get_mode(src_fd))) { return err; }

    // cool now read the directory
    // okay i am NOT using fdopendir or whatever here
    // sounds like i need to use some syscall to read the dir aaaa
    // well apparently uring doesnt like getdents!

    // okay good news for u! uring doesnt handle directories, which means...
    // this code is gonna get used after all! fdopendir time
    DIR *src_dir = fdopendir(src_fd);
    assert(src_dir);
    for (struct dirent *dent = readdir(src_dir); dent; dent = readdir(src_dir)) {
        if (dent->d_type == DT_REG || dent->d_type == DT_DIR) {
            // is it hidden? if so ignore
            if (dent->d_name[0] == '.') { continue; }
            
            // open it
            int src_fpath_len = src_len + 2 + strlen(dent->d_name); // 2 bc slash, null
            char *src_fpath = malloc(src_fpath_len);
            snprintf(src_fpath, src_fpath_len, "%s/%s", src, dent->d_name);
            
            // TODO could rly use a helper for this!
            // TODO need to remove a trailing slash here? idts right
            int dst_fpath_len = dst_len + 2 + strlen(dent->d_name);
            char *dst_fpath = malloc(dst_fpath_len);
            snprintf(dst_fpath, dst_fpath_len, "%s/%s", dst, dent->d_name);

            if (dent->d_type == DT_REG) {
                int src_ffd = open(src_fpath, O_RDONLY);
                if (src_ffd < 0) { return errno; }
                
                int dst_ffd = open(dst_fpath, O_WRONLY | O_CREAT, get_mode(src_ffd) & 0777);
                if (dst_ffd < 0) { return errno; }
                
                // TODO uring lol
                void *buf = malloc(BLOCK);
                while (true) {
                    int nread = read(src_ffd, buf, BLOCK);
                    write(dst_ffd, buf, nread);
                    if (nread < BLOCK) { break; }
                }
            } else {
                // i assure u NO ONE CARES that u call strlen twice!!
                err = copy(src_fpath, dst_fpath);
                if (err) { return err; }
            }
            // uh okay now copy the data bruh
        } // else ignore loll
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

    // yes we call stat on src twice no please i dont want to care about it
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

    struct io_uring ring;
    // TODO use iuqi_params to get interesting results should time permit
    if (io_uring_queue_init(UQ_DEPTH, &ring, 0) != 0) {
        perror(argv[0]);
        return 1;
    }

    // END SETUP

#ifdef USE_IO_URING
#else
    // implement the whole damn thing in normal fucking syscalls!
    // src exists, dst exists
    // list the contents of src
    // foreach regular file, copy it normal like [helper 0]
    // foreach directory, recurse [helper -1 loll]
    // create_dir should be a helper ig [helper 1 loll]
#endif

    return copy(src, dst);
}
