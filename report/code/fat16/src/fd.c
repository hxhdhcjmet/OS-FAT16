 #include "fat.h"

int fs_open(const char *filename,int mode){
    if (filename == NULL) return -1;

    //在当前目录下找普通文件
    int  dir_idx = -1;
    for(int i = 0; i < MAX_ENTRY; i++){
        if(Directory[i].ac == 1 &&
            Directory[i].parent == current_dir &&
            Directory[i].type == 0&&
            strcmp(Directory[i].name,filename) == 0
        ){
            dir_idx = i;
            break;
        }
    }
    if (dir_idx == -1){
        //没有找到
        printf("open failed : file not found : %s \n",filename);
        return -1;
    }

    // 找空闲fd
    for (int fd = 0; fd < MAX_FD; fd ++){
        if(FDTable[fd].used == 0){
            FDTable[fd].used = 1;
            FDTable[fd].dir_index = dir_idx;
            FDTable[fd].offset = 0;
            FDTable[fd].mode = mode;
            printf("open success : %s,fd = %d\n",filename,fd);
            return fd;
        }  
    }
    printf("open failed:too many open files\n");
    return -1;
}

bool fs_close(int fd){
    //无效fd
    if (fd < 0 || fd > MAX_FD){
        printf("close failed : invalid fd\n");
        return false;
    }
    //文件还未打开
    if(FDTable[fd].used == 0){
        printf("close failed : fd is not open yet\n");
        return false;
    }
    //正常关闭文件
    FDTable[fd].used = 0;
    FDTable[fd].dir_index = -1;
    FDTable[fd].offset = 0;
    FDTable[fd].mode = 0;
    printf("close success: fd = %d\n", fd);
    return true;
}

bool fs_seek(int fd,int offset){
    if(fd < 0 || fd >= MAX_FD) return false;
    if(FDTable[fd].used == 0) return false;
    if(offset < 0) return false;

    int dir_idx = FDTable[fd].dir_index;
    int file_size = Directory[dir_idx].size;

    // 读模式不允许超过文件大小
    if(FDTable[fd].mode == MODE_READ || FDTable[fd].mode == MODE_RDWR){
        if(offset > file_size){
            printf("seek failed: offset %d exceeds file size %d\n", offset, file_size);
            return false;
        }
    }

    FDTable[fd].offset = offset;
    return true;
}