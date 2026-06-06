#include "fat.h"
#include<string.h>

// 全局变量定义
char Disk[DISK_SIZE];
int FAT[BLOCK_NUM];
int current_dir;
DirEntry Directory[MAX_ENTRY];
FileDescriptor FDTable[MAX_FD];

void init_fd_table(void){
    memset(FDTable,0,sizeof(FDTable));
    for (int i = 0; i < MAX_FD;i++){
        FDTable[i].dir_index = -1;
    }
}

void init_fs(void){
    memset(Disk,0,sizeof(Disk));
    memset(FAT,0,sizeof(FAT));
    memset(Directory,0,sizeof(Directory));
    for (int i = 0;i < MAX_ENTRY; i++){
        Directory[i].start_block = -1;
        Directory[i].parent = -1;
    }
    strcpy(Directory[0].name,"/");
    Directory[0].size = 0;
    Directory[0].start_block = -1;
    Directory[0].ac = 1;
    Directory[0].type = 1;
    Directory[0].parent = -1;
    current_dir = 0;
    init_fd_table();
}