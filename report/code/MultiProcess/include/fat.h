/*系统是基于内存的：建⽴⼀个数组：把这个数组当成硬盘，实现⽂件系统。
假设只有⼀个进程使⽤。*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>


//定义所用常量
#define DISK_MAXLEN 2560
#define BLOCK_SIZE 64
#define BLOCK_NUM (DISK_MAXLEN / BLOCK_SIZE)
#define MAX_ENTRY 32
#define MAX_OPEN_FILE 32
#define MAX_FD 16
#define FAT_FREE (-2)
#define FAT_END  (-1)

//最大进程数
#define MAX_PROCESS 8

//模式定义
#define MODE_READ  1
#define MODE_WRITE 2
#define MODE_RDWR  3
#define MODE_WAPPEND 4

//目录项
typedef struct {
    char name[16];
    int size;
    int start_block;
    int ac;
    int type;
    int parent;
    int reader_count;           /* 当前读者数量 */
    sem_t rw_lock;              /* 文件资源信号量，初值=1 */
    pthread_mutex_t mutex;      /* 保护 reader_count 的互斥锁 */
} DirEntry;

//文件描述符
typedef struct {
    int used;       // 0 空闲，1 已使用
    int dir_index;  // 对应 Directory[] 下标
    int offset;     // 当前读写偏移
    int mode;       // 打开模式
} FileDescriptor;

//pcb
typedef struct {
    int pid;
    FileDescriptor fd_table[MAX_FD];
    int current_dir;          // 每个进程维护的当前目录索引
} PCB;


extern char Disk[DISK_MAXLEN];
extern int FAT[BLOCK_NUM];
extern DirEntry Directory[MAX_ENTRY];
extern pthread_mutex_t fs_mutex;
extern PCB ProcessTable[MAX_PROCESS];


void init_fs(void);
void init_process_table(void);

//目录操作
bool mkdir(int pid, const char *dirname);
bool ls(int pid);
bool cd(int pid, const char *dirname);

//文件创建与删除
bool fs_create(int pid, const char *filename);
bool fs_delete(int pid, const char *filename);

//文件打开关闭与读写
int  fs_open(int pid, const char *filename, int mode);
bool fs_close(int pid, int fd);
int  fs_read(int pid, int fd, char *buffer, int size);
int  fs_write(int pid, int fd, const char *data, int size);
bool fs_seek(int pid, int fd, int offset);


void print_prompt(int pid);
