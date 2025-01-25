
#include "file.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief delete dir recursively
 *
 * @param path
 * @return int
 */
int del_dir(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    char filepath[1024];

    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 忽略 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构造文件或目录的完整路径
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(filepath, &statbuf) == -1) {
            perror("stat");
            closedir(dir);
            return -1;
        }

        // 如果是目录,递归删除
        if (S_ISDIR(statbuf.st_mode)) {
            if (del_dir(filepath) == -1) {
                closedir(dir);
                return -1;
            }
        } else {  // 如果是文件,删除文件
            if (remove(filepath) == -1) {
                perror("remove");
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);

    // 删除当前目录
    if (rmdir(path) == -1) {
        perror("rmdir");
        return -1;
    }

    return 0;
}

int path_exist(const char *path) {
    return access(path, F_OK) == 0;
}