#include "fat.h"

int fs_open(int pid, const char *filename, int mode)
{
    if (pid < 0 || pid >= MAX_PROCESS || filename == NULL) {
        return -1;
    }

    int cur_dir = ProcessTable[pid].current_dir;
    int dir_index = -1;

    //互斥访问Directory,查找要打开的文件
    pthread_mutex_lock(&fs_mutex);

    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 1 &&
            Directory[i].parent == cur_dir &&
            Directory[i].type == 0 &&
            strcmp(Directory[i].name, filename) == 0) {
            dir_index = i;
            break;
        }
    }

    if (dir_index == -1) {
        pthread_mutex_unlock(&fs_mutex);
        printf("open failed: file not found: %s\n", filename);
        return -1;
    }

    pthread_mutex_unlock(&fs_mutex);

    //只读、读写打开文件需要修改reader_count数量(互斥修改)

    if (mode == MODE_READ) {
        pthread_mutex_lock(&Directory[dir_index].mutex);//互斥修改reader_count
        Directory[dir_index].reader_count++;
        if (Directory[dir_index].reader_count == 1) {
            if (sem_trywait(&Directory[dir_index].rw_lock) != 0) { //有写者时打开失败
                Directory[dir_index].reader_count--;
                pthread_mutex_unlock(&Directory[dir_index].mutex);
                printf("open failed: file is busy\n");
                return -1;
            }
        }
        pthread_mutex_unlock(&Directory[dir_index].mutex);//修改reader_count结束
    }
    else if (mode == MODE_WRITE || mode == MODE_RDWR) {
        if (sem_trywait(&Directory[dir_index].rw_lock) != 0) {  //文件被读者或写者占用时访问失败
            printf("open failed: file is busy\n");
            return -1;
        }
    }
    else {
        printf("open failed: invalid mode\n");
        return -1;
    }

    //分配fd
    for (int fd = 0; fd < MAX_FD; fd++) {
        if (ProcessTable[pid].fd_table[fd].used == 0) {
            ProcessTable[pid].fd_table[fd].used = 1;
            ProcessTable[pid].fd_table[fd].dir_index = dir_index;
            ProcessTable[pid].fd_table[fd].offset = 0;
            ProcessTable[pid].fd_table[fd].mode = mode;

            printf("open success: pid=%d, file=%s, fd=%d\n",
                   pid, filename, fd);
            return fd;
        }
    }

    //fd分配失败,回退已经做的修改
    if (mode == MODE_READ) {
        pthread_mutex_lock(&Directory[dir_index].mutex);
        Directory[dir_index].reader_count--;
        if (Directory[dir_index].reader_count == 0) {
            sem_post(&Directory[dir_index].rw_lock);  // 最后一个读者释放文件 
        }
        pthread_mutex_unlock(&Directory[dir_index].mutex);
    } else {
        sem_post(&Directory[dir_index].rw_lock);  // 写者释放文件 
    }

    printf("open failed: fd table full\n");
    return -1;
}

bool fs_close(int pid, int fd)
{
    if (pid < 0 || pid >= MAX_PROCESS || fd < 0 || fd >= MAX_FD) {
        return false;
    }

    if (ProcessTable[pid].fd_table[fd].used == 0) {
        printf("close failed: invalid fd\n");
        return false;
    }

    int dir_index = ProcessTable[pid].fd_table[fd].dir_index;
    int mode = ProcessTable[pid].fd_table[fd].mode;


    if (mode == MODE_READ) {
        pthread_mutex_lock(&Directory[dir_index].mutex);
        Directory[dir_index].reader_count--;
        if (Directory[dir_index].reader_count == 0) {
            sem_post(&Directory[dir_index].rw_lock);  // 最后一个读者释放文件 
        }
        pthread_mutex_unlock(&Directory[dir_index].mutex);
    }
    else if (mode == MODE_WRITE || mode == MODE_RDWR) {
        sem_post(&Directory[dir_index].rw_lock);  // 写者释放文件 
    }
    
    //释放已分配的fd
    ProcessTable[pid].fd_table[fd].used = 0;
    ProcessTable[pid].fd_table[fd].dir_index = -1;
    ProcessTable[pid].fd_table[fd].offset = 0;
    ProcessTable[pid].fd_table[fd].mode = 0;

    printf("close success: pid=%d, fd=%d\n", pid, fd);
    return true;
}
