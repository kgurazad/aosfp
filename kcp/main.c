#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

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
            fprintf(stderr, "%s: first argument should be a directory\n", argv[0]);
        }
    } else {
        perror(argv[0]);
        return 1;
    }

    struct stat s_dst;
    if (stat(dst, &s_dst) == 0) {
        if (!(s_dst.st_mode & S_IFDIR)) {
            fprintf(stderr, "%s: second argument should be a directory\n", argv[0]);
        }

        char *dirname = get_dirname(src);
        int dst_len = strlen(dst) + 1 + strlen(dirname);
        char *dst_ = malloc(dst_len);
        snprintf(dst_, dst_len, "%s/%s", dst, dirname);
    }

    // dst is the final path for the root directory
    // create it
    if (mkdir(dst, s_src.st_mode & 0777)) {
        perror(argv[0]);
        return 1;
    }

    // copy time!
}
