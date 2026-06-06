#include "fat.h"

bool mkdir(int pid, const char *dirname)
{
    if (pid < 0 || pid >= MAX_PROCESS) return false;
    int cur_dir = ProcessTable[pid].current_dir;

    pthread_mutex_lock(&fs_mutex);//创建文件需要加锁

    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 0) {
            strncpy(Directory[i].name, dirname, 16);
            Directory[i].size = 0;
            Directory[i].start_block = FAT_END;
            Directory[i].ac = 1;
            Directory[i].type = 1;
            Directory[i].parent = cur_dir;
            Directory[i].reader_count = 0;
            sem_init(&Directory[i].rw_lock, 0, 1);
            pthread_mutex_init(&Directory[i].mutex, NULL);
            pthread_mutex_unlock(&fs_mutex);
            return true;
        }
    }

    pthread_mutex_unlock(&fs_mutex);
    return false;
}

static int get_dir_size(int dir_index)
{
    int total = 0;
    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 1 && Directory[i].parent == dir_index) {
            if (Directory[i].type == 0) {
                total += Directory[i].size;
            } else if (Directory[i].type == 1) {
                total += get_dir_size(i);
            }
        }
    }
    return total;
}

bool ls(int pid)
{
    if (pid < 0 || pid >= MAX_PROCESS) return false;
    int cur_dir = ProcessTable[pid].current_dir;

    pthread_mutex_lock(&fs_mutex);

    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 1 && Directory[i].parent == cur_dir) {
            int show_size;
            if (Directory[i].type == 1) {
                show_size = get_dir_size(i);
            } else {
                show_size = Directory[i].size;
            }
            printf("%s\t%s\t%d bytes\n",
                   Directory[i].type == 1 ? "DIR" : "FILE",
                   Directory[i].name,
                   show_size);
        }
    }

    pthread_mutex_unlock(&fs_mutex);
    return true;
}

bool cd(int pid, const char *dirname)
{
    if (pid < 0 || pid >= MAX_PROCESS || dirname == NULL) return false;

    int cur_dir = ProcessTable[pid].current_dir;

    if (strcmp(dirname, "/") == 0) {
        ProcessTable[pid].current_dir = 0;
        return true;
    }

    if (strcmp(dirname, "..") == 0) {
        if (Directory[cur_dir].parent != -1) {
            ProcessTable[pid].current_dir = Directory[cur_dir].parent;
        }
        return true;
    }

    //遍历以在当前目录下找子目录(对Directory[]只读),避免其他进程此时mkdir/touch,加锁保证一致性
    pthread_mutex_lock(&fs_mutex);

    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 1 &&
            Directory[i].parent == cur_dir &&
            Directory[i].type == 1 &&
            strcmp(Directory[i].name, dirname) == 0) {
            ProcessTable[pid].current_dir = i;
            pthread_mutex_unlock(&fs_mutex);
            return true;
        }
    }

    pthread_mutex_unlock(&fs_mutex);
    return false;
}
