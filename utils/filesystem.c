#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
bool pv_fs_path_exist(const char *path)
{
    return access(path, F_OK) == 0;
}

bool pv_fs_path_is_directory(const char *path)
{
    DIR *tmp = opendir(path);
    if (tmp) {
        closedir(tmp);
        return true;
    }

    return false;
}

static int get_directory(char *dir, const char *path)
{
    if (!pv_fs_path_exist(path)) {
        dir = NULL;
        return -1;
    }

    if (pv_fs_path_is_directory(path)) {
        strncpy(dir, path, strnlen(path, PATH_MAX));
        return 0;
    } else if (errno == ENOTDIR) {
        char copy[PATH_MAX];
        strncpy(copy, path, strnlen(path, PATH_MAX));

        char *tmp = dirname(copy);
        strncpy(dir, tmp, strnlen(path, PATH_MAX));
    }
    return 0;
}

void pv_fs_path_sync(const char *path)
{
    char dir[PATH_MAX] = { 0 };

    if (get_directory(dir, path) != 0)
        return;

    int fd = open(dir, O_RDONLY);
    if (fd > -1) {
        fsync(fd);
        close(fd);
    }
}

int pv_fs_mkdir_p(const char *path, mode_t mode)
{
    if (!path)
        return -1;

    if (pv_fs_path_exist(path)) {
        errno = EEXIST;
        return -1;
    }

    if (errno != 0 && errno != ENOENT)
        return -1;

    char cur_path[PATH_MAX];

    size_t created = 0;
    unsigned long i = 0;
    while (path[i]) {
        if (path[i] == '/') {
            memcpy(cur_path, path, i + 1);
            cur_path[i + 2] = '\0';
            if (mkdir(cur_path, mode) != 0 && errno != EEXIST)
                return -1;
            created = i + 1;
        }
        ++i;
    }

    // create the last directory, this happend when the given path
    // doesn't finish with '/'
    if (created < strlen(path)) {
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            return -1;
    }

    return 0;
}

void pv_fs_path_join(char *buf, int size, ...)
{
    char fmt[PATH_MAX] = { 0 };

    for (int i = 0; i < size; ++i) {
        fmt[i * 3 + 0] = '%';
        fmt[i * 3 + 1] = 's';
        fmt[i * 3 + 2] = '/';
    }

    va_list list;
    va_start(list, size);

    vsnprintf(buf, PATH_MAX, fmt, list);
    buf[strnlen(buf, PATH_MAX) - 1] = '\0';

    va_end(list);
}
