#include "fat.h"
#include <string.h>

char Disk[DISK_MAXLEN];
int FAT[BLOCK_NUM];
DirEntry Directory[MAX_ENTRY];
pthread_mutex_t fs_mutex = PTHREAD_MUTEX_INITIALIZER;
PCB ProcessTable[MAX_PROCESS];

void init_process_table(void)
{
    memset(ProcessTable, 0, sizeof(ProcessTable));

    for (int p = 0; p < MAX_PROCESS; p++) {
        ProcessTable[p].pid = p;
        ProcessTable[p].current_dir = 0;   // 每个进程初始在根目录

        for (int fd = 0; fd < MAX_FD; fd++) {
            ProcessTable[p].fd_table[fd].dir_index = -1;
            ProcessTable[p].fd_table[fd].offset = 0;
            ProcessTable[p].fd_table[fd].mode = 0;
            ProcessTable[p].fd_table[fd].used = 0;
        }
    }
}
void init_fs(void)
{
    memset(Disk, 0, sizeof(Disk));
    for (int i = 0; i < BLOCK_NUM; i++) {
    FAT[i] = FAT_FREE;
    }
    memset(Directory, 0, sizeof(Directory));

    for (int i = 0; i < MAX_ENTRY; i++) {
        Directory[i].start_block = FAT_END;
        Directory[i].parent = -1;
        Directory[i].reader_count = 0;
        sem_init(&Directory[i].rw_lock, 0, 1);
        pthread_mutex_init(&Directory[i].mutex, NULL);
    }

    strcpy(Directory[0].name, "/");
    Directory[0].size = 0;
    Directory[0].start_block = FAT_END;
    Directory[0].ac = 1;
    Directory[0].type = 1;
    Directory[0].parent = -1;

    init_process_table();
}
