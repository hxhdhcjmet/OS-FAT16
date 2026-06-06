// 定义基于内存的文件系统,数组代表硬盘
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// 定义所用的常量
#define DISK_SIZE 2560
#define BLOCK_SIZE 64
#define BLOCK_NUM  (DISK_SIZE / BLOCK_SIZE)
#define MAX_ENTRY 32
#define MAX_OPENFILE 32
#define MAX_FD 16
#define DIR_NAME_LENGTH 16

// 模式定义
#define MODE_READ  1
#define MODE_WRITE 2
#define MODE_RDWR  3
#define MODE_WAPPEND 4

//目录项
typedef struct {
    char name[DIR_NAME_LENGTH];
    int size;
    size_t start_block;
    int ac;               // 0=未使用, 1=已使用
    int type;             // 0=文件, 1=目录
    int parent;
} DirEntry;

// 文件描述符 
typedef struct {
    int used;             // 0=空闲, 1=已使用
    int dir_index;        // 文件下标
    int offset;           // 读写偏移
    int mode;             // 打开模式
} FileDescriptor;

//全局变量声明 
extern char Disk[DISK_SIZE];
extern int FAT[BLOCK_NUM];
extern int current_dir;
extern DirEntry Directory[MAX_ENTRY];
extern FileDescriptor FDTable[MAX_FD];

//  初始化 
void init_fs(void);

//  目录操作 
bool mkdir(const char *dirname);
bool ls(void);
bool cd(const char *dirname);

//  文件创建与删除 
bool fs_create(const char *filename);
bool fs_delete(const char *filename);

// 文件操作（读写） 
int  fs_open(const char *filename, int mode);
bool fs_close(int fd);
int  fs_write(int fd, const char *data, int size);
int  fs_read(int fd, char *buffer, int size);
bool fs_seek(int fd, int offset);


void print_prompt(void);