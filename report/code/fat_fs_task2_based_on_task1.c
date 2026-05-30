#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define DISK_MAXLEN 2560
#define BLOCK_SIZE 128
#define BLOCK_NUM  (DISK_MAXLEN / BLOCK_SIZE)

#define FAT_FREE 0x0000
#define FAT_END  0xFFFF

#define ROOT_BLOCK 2
#define DATA_START 3

#define NAME_LEN 16
#define TYPE_FILE 1
#define TYPE_DIR  2

#define MAX_OPEN 8
#define MAX_FILE_LOCKS 8

char Disk[DISK_MAXLEN];

/* -------------------- 任务一：文件系统基础结构 -------------------- */

typedef struct {
    int block_size;
    int block_num;
    int fat_start;
    int root_dir_block;
    int data_start;
} SuperBlock;

typedef struct {
    char name[NAME_LEN];              // 文件名或目录名
    unsigned char used;               // 是否被使用
    unsigned char type;               // TYPE_FILE 或 TYPE_DIR
    unsigned short first_block;       // 起始块号
    unsigned short size;              // 文件大小（字节）
    char reserved[10];
} DirEntry;

typedef struct {
    int used;
    int dir_block;
    char filename[NAME_LEN];
    char mode;
} OpenFile;

OpenFile open_table[MAX_OPEN];

SuperBlock* get_super_block(void) {
    return (SuperBlock*)(&Disk[0]);
}

unsigned short* get_fat(void) {
    return (unsigned short*)(&Disk[BLOCK_SIZE]);
}

DirEntry* get_dir_entries(int block_id) {
    return (DirEntry*)(&Disk[block_id * BLOCK_SIZE]);
}

char* get_block_addr(int block_id) {
    return &Disk[block_id * BLOCK_SIZE];
}

void fs_format(void) {
    int i;

    memset(Disk, 0, sizeof(Disk));

    SuperBlock* sb = get_super_block();
    sb->block_size = BLOCK_SIZE;
    sb->block_num = BLOCK_NUM;
    sb->fat_start = 1;
    sb->root_dir_block = ROOT_BLOCK;
    sb->data_start = DATA_START;

    unsigned short* fat = get_fat();

    for (i = 0; i < BLOCK_NUM; i++) {
        fat[i] = FAT_FREE;
    }

    fat[0] = FAT_END;                 // 超级块占用
    fat[1] = FAT_END;                 // FAT 表占用
    fat[2] = FAT_END;                 // 根目录占用

    memset(get_block_addr(ROOT_BLOCK), 0, BLOCK_SIZE);

    printf("format success\n");
}

void print_fat(void) {
    unsigned short* fat = get_fat();

    printf("===== FAT Table =====\n");
    for (int i = 0; i < BLOCK_NUM; i++) {
        if (fat[i] == FAT_FREE) {
            printf("Block %2d: FREE\n", i);
        } else if (fat[i] == FAT_END) {
            printf("Block %2d: END\n", i);
        } else {
            printf("Block %2d: %d\n", i, fat[i]);
        }
    }
    printf("=====================\n");
}

int alloc_block(void) {
    unsigned short* fat = get_fat();

    for (int i = DATA_START; i < BLOCK_NUM; i++) {
        if (fat[i] == FAT_FREE) {
            fat[i] = FAT_END;
            memset(get_block_addr(i), 0, BLOCK_SIZE);
            return i;
        }
    }

    return -1;
}

void free_chain(int first_block) {
    unsigned short* fat = get_fat();
    int current = first_block;

    while (current != FAT_END && current != FAT_FREE &&
           current >= 0 && current < BLOCK_NUM) {
        int next = fat[current];
        fat[current] = FAT_FREE;
        memset(get_block_addr(current), 0, BLOCK_SIZE);

        if (next == FAT_END) {
            break;
        }

        current = next;
    }
}

int entries_per_block(void) {
    return BLOCK_SIZE / (int)sizeof(DirEntry);
}

DirEntry* find_entry(int dir_block, const char* name) {
    DirEntry* entries = get_dir_entries(dir_block);
    int count = entries_per_block();

    for (int i = 0; i < count; i++) {
        if (entries[i].used && strcmp(entries[i].name, name) == 0) {
            return &entries[i];
        }
    }

    return NULL;
}

DirEntry* find_empty_entry(int dir_block) {
    DirEntry* entries = get_dir_entries(dir_block);
    int count = entries_per_block();

    for (int i = 0; i < count; i++) {
        if (!entries[i].used) {
            return &entries[i];
        }
    }

    return NULL;
}

void fs_ls(int current_dir) {
    DirEntry* entries = get_dir_entries(current_dir);
    int count = entries_per_block();

    printf("===== ls block %d =====\n", current_dir);

    for (int i = 0; i < count; i++) {
        if (entries[i].used) {
            if (entries[i].type == TYPE_DIR) {
                printf("[DIR ] %-10s first=%d size=%d\n",
                       entries[i].name,
                       entries[i].first_block,
                       entries[i].size);
            } else if (entries[i].type == TYPE_FILE) {
                printf("[FILE] %-10s first=%d size=%d\n",
                       entries[i].name,
                       entries[i].first_block,
                       entries[i].size);
            }
        }
    }

    printf("=======================\n");
}

void init_open_table(void) {
    memset(open_table, 0, sizeof(open_table));
}

DirEntry* create_file_entry(int current_dir, const char* filename) {
    DirEntry* empty = find_empty_entry(current_dir);

    if (empty == NULL) {
        return NULL;
    }

    memset(empty, 0, sizeof(DirEntry));
    strncpy(empty->name, filename, NAME_LEN - 1);
    empty->name[NAME_LEN - 1] = '\0';
    empty->used = 1;
    empty->type = TYPE_FILE;
    empty->first_block = FAT_END;
    empty->size = 0;

    return empty;
}

/* -------------------- 任务二：同步控制结构 -------------------- */

/*
 * fs_mutex 保护 FAT 表、目录项以及 open_table 等共享元数据。
 * 其初值为 1，因此同一时刻只允许一个执行单元修改文件系统元数据。
 */
sem_t fs_mutex;

typedef struct {
    int used;
    int dir_block;
    char filename[NAME_LEN];
    int readers;                     // 当前正在读取该文件的读者数量
    sem_t readers_mutex;             // 保护 readers 计数器
    sem_t writer_mutex;              // 写锁：写者独占；第一个读者也会申请该锁
} FileLock;

FileLock file_locks[MAX_FILE_LOCKS];

typedef struct {
    int pid;
    int current_dir;                 // 每个并发执行单元独立维护当前目录
    FileLock* lock;
} ProcessContext;

void init_sync(void) {
    sem_init(&fs_mutex, 0, 1);
    memset(file_locks, 0, sizeof(file_locks));
}

void destroy_sync(void) {
    for (int i = 0; i < MAX_FILE_LOCKS; i++) {
        if (file_locks[i].used) {
            sem_destroy(&file_locks[i].readers_mutex);
            sem_destroy(&file_locks[i].writer_mutex);
        }
    }
    sem_destroy(&fs_mutex);
}

/* 调用本函数前，应先申请 fs_mutex。 */
FileLock* find_file_lock_nolock(int dir_block, const char* filename) {
    for (int i = 0; i < MAX_FILE_LOCKS; i++) {
        if (file_locks[i].used &&
            file_locks[i].dir_block == dir_block &&
            strcmp(file_locks[i].filename, filename) == 0) {
            return &file_locks[i];
        }
    }

    return NULL;
}

/* 调用本函数前，应先申请 fs_mutex。 */
FileLock* get_or_create_file_lock_nolock(int dir_block, const char* filename) {
    FileLock* lock = find_file_lock_nolock(dir_block, filename);
    if (lock != NULL) {
        return lock;
    }

    for (int i = 0; i < MAX_FILE_LOCKS; i++) {
        if (!file_locks[i].used) {
            file_locks[i].used = 1;
            file_locks[i].dir_block = dir_block;
            strncpy(file_locks[i].filename, filename, NAME_LEN - 1);
            file_locks[i].filename[NAME_LEN - 1] = '\0';
            file_locks[i].readers = 0;
            sem_init(&file_locks[i].readers_mutex, 0, 1);
            sem_init(&file_locks[i].writer_mutex, 0, 1);
            return &file_locks[i];
        }
    }

    return NULL;
}

FileLock* get_or_create_file_lock(int dir_block, const char* filename) {
    FileLock* lock;

    sem_wait(&fs_mutex);
    lock = get_or_create_file_lock_nolock(dir_block, filename);
    sem_post(&fs_mutex);

    return lock;
}

void reader_enter(FileLock* lock, int pid) {
    sem_wait(&lock->readers_mutex);

    lock->readers++;
    if (lock->readers == 1) {
        /* 第一个读者阻止写者进入。 */
        sem_wait(&lock->writer_mutex);
    }

    printf("[Reader %d] starts reading, active_readers=%d\n",
           pid, lock->readers);

    sem_post(&lock->readers_mutex);
}

void reader_exit(FileLock* lock, int pid) {
    sem_wait(&lock->readers_mutex);

    lock->readers--;
    printf("[Reader %d] finishes reading, active_readers=%d\n",
           pid, lock->readers);

    if (lock->readers == 0) {
        /* 最后一个读者离开后，允许写者进入。 */
        sem_post(&lock->writer_mutex);
    }

    sem_post(&lock->readers_mutex);
}

void writer_enter(FileLock* lock, int pid) {
    printf("[Writer %d] wants to write\n", pid);
    sem_wait(&lock->writer_mutex);
    printf("[Writer %d] starts writing\n", pid);
}

void writer_exit(FileLock* lock, int pid) {
    printf("[Writer %d] finishes writing\n", pid);
    sem_post(&lock->writer_mutex);
}

/* -------------------- 加锁后的文件系统接口 -------------------- */

int fs_mkdir(int current_dir, const char* dirname) {
    int result = -1;

    sem_wait(&fs_mutex);

    if (find_entry(current_dir, dirname) != NULL) {
        printf("mkdir failed: %s already exists\n", dirname);
        goto out;
    }

    DirEntry* empty = find_empty_entry(current_dir);
    if (empty == NULL) {
        printf("mkdir failed: directory is full\n");
        goto out;
    }

    int block = alloc_block();
    if (block == -1) {
        printf("mkdir failed: no free block\n");
        goto out;
    }

    memset(empty, 0, sizeof(DirEntry));
    strncpy(empty->name, dirname, NAME_LEN - 1);
    empty->name[NAME_LEN - 1] = '\0';
    empty->used = 1;
    empty->type = TYPE_DIR;
    empty->first_block = block;
    empty->size = 0;

    memset(get_block_addr(block), 0, BLOCK_SIZE);

    printf("mkdir success: %s, block=%d\n", dirname, block);
    result = 0;

out:
    sem_post(&fs_mutex);
    return result;
}

int fs_open(int current_dir, const char* filename, char mode) {
    int fd = -1;

    sem_wait(&fs_mutex);

    DirEntry* entry = find_entry(current_dir, filename);

    if (mode == 'r') {
        if (entry == NULL || entry->type != TYPE_FILE) {
            printf("open failed: %s not found\n", filename);
            goto out;
        }
    } else if (mode == 'w') {
        if (entry == NULL) {
            entry = create_file_entry(current_dir, filename);
            if (entry == NULL) {
                printf("open failed: no empty directory entry\n");
                goto out;
            }
        } else if (entry->type != TYPE_FILE) {
            printf("open failed: %s is not a file\n", filename);
            goto out;
        }

        if (get_or_create_file_lock_nolock(current_dir, filename) == NULL) {
            printf("open failed: no empty file lock entry\n");
            goto out;
        }
    } else {
        printf("open failed: invalid mode\n");
        goto out;
    }

    for (int i = 0; i < MAX_OPEN; i++) {
        if (!open_table[i].used) {
            open_table[i].used = 1;
            open_table[i].dir_block = current_dir;
            strncpy(open_table[i].filename, filename, NAME_LEN - 1);
            open_table[i].filename[NAME_LEN - 1] = '\0';
            open_table[i].mode = mode;

            printf("open success: %s, fd=%d, mode=%c\n", filename, i, mode);
            fd = i;
            goto out;
        }
    }

    printf("open failed: open table full\n");

out:
    sem_post(&fs_mutex);
    return fd;
}

int fs_close(int fd) {
    int result = -1;

    sem_wait(&fs_mutex);

    if (fd < 0 || fd >= MAX_OPEN || !open_table[fd].used) {
        printf("close failed: invalid fd\n");
        goto out;
    }

    printf("close success: fd=%d\n", fd);
    open_table[fd].used = 0;
    result = 0;

out:
    sem_post(&fs_mutex);
    return result;
}

/*
 * fs_write() 只负责元数据和数据块更新。
 * 调用者必须先通过 writer_enter() 获得文件写锁。
 */
int fs_write(int fd, const char* data) {
    int result = -1;

    sem_wait(&fs_mutex);

    if (fd < 0 || fd >= MAX_OPEN || !open_table[fd].used) {
        printf("write failed: invalid fd\n");
        goto out;
    }

    if (open_table[fd].mode != 'w') {
        printf("write failed: file not opened in write mode\n");
        goto out;
    }

    DirEntry* entry = find_entry(open_table[fd].dir_block, open_table[fd].filename);
    if (entry == NULL || entry->type != TYPE_FILE) {
        printf("write failed: file entry not found\n");
        goto out;
    }

    if (entry->first_block != FAT_END) {
        free_chain(entry->first_block);
    }

    int len = (int)strlen(data);
    int remaining = len;
    int offset = 0;
    int first = -1;
    int prev = -1;

    unsigned short* fat = get_fat();

    while (remaining > 0) {
        int block = alloc_block();
        if (block == -1) {
            printf("write failed: no free block\n");
            goto out;
        }

        if (first == -1) {
            first = block;
        }

        if (prev != -1) {
            fat[prev] = (unsigned short)block;
        }

        int write_size = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        memcpy(get_block_addr(block), data + offset, (size_t)write_size);

        offset += write_size;
        remaining -= write_size;
        prev = block;
    }

    entry->first_block = (first == -1) ? FAT_END : (unsigned short)first;
    entry->size = (unsigned short)len;

    printf("write success: fd=%d, size=%d bytes\n", fd, len);
    result = len;

out:
    sem_post(&fs_mutex);
    return result;
}

/*
 * fs_read() 不申请 fs_mutex，以便多个读者可以并发读取文件内容。
 * 调用者必须先通过 reader_enter() 获得文件读权限。
 */
int fs_read(int fd, char* buf, int len) {
    if (fd < 0 || fd >= MAX_OPEN || !open_table[fd].used) {
        printf("read failed: invalid fd\n");
        return -1;
    }

    if (open_table[fd].mode != 'r') {
        printf("read failed: file not opened in read mode\n");
        return -1;
    }

    DirEntry* entry = find_entry(open_table[fd].dir_block, open_table[fd].filename);
    if (entry == NULL || entry->type != TYPE_FILE) {
        printf("read failed: file entry not found\n");
        return -1;
    }

    unsigned short* fat = get_fat();

    int current = entry->first_block;
    int remaining = entry->size;
    int offset = 0;

    if (remaining > len - 1) {
        remaining = len - 1;
    }

    while (current != FAT_END && remaining > 0) {
        int read_size = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        memcpy(buf + offset, get_block_addr(current), (size_t)read_size);

        offset += read_size;
        remaining -= read_size;

        if (fat[current] == FAT_END) {
            break;
        }

        current = fat[current];
    }

    buf[offset] = '\0';

    printf("read success: fd=%d, size=%d bytes\n", fd, offset);
    return offset;
}

int fs_delete(int current_dir, const char* name) {
    int result = -1;

    sem_wait(&fs_mutex);

    DirEntry* entry = find_entry(current_dir, name);

    if (entry == NULL) {
        printf("delete failed: %s not found\n", name);
        goto out;
    }

    if (entry->type == TYPE_FILE) {
        if (entry->first_block != FAT_END) {
            free_chain(entry->first_block);
        }

        printf("delete file success: %s\n", name);
        memset(entry, 0, sizeof(DirEntry));
        result = 0;
        goto out;
    }

    if (entry->type == TYPE_DIR) {
        DirEntry* sub_entries = get_dir_entries(entry->first_block);
        int count = entries_per_block();

        for (int i = 0; i < count; i++) {
            if (sub_entries[i].used) {
                printf("delete failed: directory %s is not empty\n", name);
                goto out;
            }
        }

        free_chain(entry->first_block);
        printf("delete directory success: %s\n", name);
        memset(entry, 0, sizeof(DirEntry));
        result = 0;
    }

out:
    sem_post(&fs_mutex);
    return result;
}

/* -------------------- 并发测试线程 -------------------- */

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void* reader_task(void* arg) {
    ProcessContext* process = (ProcessContext*)arg;
    char buf[256];

    reader_enter(process->lock, process->pid);

    int fd = fs_open(process->current_dir, "shared.txt", 'r');
    if (fd >= 0) {
        fs_read(fd, buf, sizeof(buf));
        printf("[Reader %d] read result: %s\n", process->pid, buf);

        /* 延长读操作，便于观察多个读者并发以及写者等待。 */
        sleep(2);

        fs_close(fd);
    }

    reader_exit(process->lock, process->pid);
    return NULL;
}

void* writer_task(void* arg) {
    ProcessContext* process = (ProcessContext*)arg;

    writer_enter(process->lock, process->pid);

    int fd = fs_open(process->current_dir, "shared.txt", 'w');
    if (fd >= 0) {
        fs_write(fd, "New content written by Writer 3.");
        fs_close(fd);
    }

    writer_exit(process->lock, process->pid);
    return NULL;
}

int main(void) {
    pthread_t reader1;
    pthread_t reader2;
    pthread_t writer3;
    char verify_buf[256];

    fs_format();
    init_open_table();
    init_sync();

    printf("\n[1] create shared.txt\n");
    FileLock* shared_lock = get_or_create_file_lock(ROOT_BLOCK, "shared.txt");
    if (shared_lock == NULL) {
        printf("failed to create file lock\n");
        destroy_sync();
        return 1;
    }

    writer_enter(shared_lock, 0);
    int fd = fs_open(ROOT_BLOCK, "shared.txt", 'w');
    if (fd < 0) {
        writer_exit(shared_lock, 0);
        destroy_sync();
        return 1;
    }
    fs_write(fd, "Initial shared file content.");
    fs_close(fd);
    writer_exit(shared_lock, 0);

    fs_ls(ROOT_BLOCK);
    print_fat();

    printf("\n[2] start two readers\n");
    ProcessContext process1 = {1, ROOT_BLOCK, shared_lock};
    ProcessContext process2 = {2, ROOT_BLOCK, shared_lock};
    ProcessContext process3 = {3, ROOT_BLOCK, shared_lock};

    pthread_create(&reader1, NULL, reader_task, &process1);
    sleep_ms(100);
    pthread_create(&reader2, NULL, reader_task, &process2);

    printf("\n[3] start one writer while readers are active\n");
    sleep_ms(100);
    pthread_create(&writer3, NULL, writer_task, &process3);

    pthread_join(reader1, NULL);
    pthread_join(reader2, NULL);
    pthread_join(writer3, NULL);

    printf("\n[4] verify final content\n");
    reader_enter(shared_lock, 4);
    fd = fs_open(ROOT_BLOCK, "shared.txt", 'r');
    if (fd >= 0) {
        fs_read(fd, verify_buf, sizeof(verify_buf));
        printf("[Reader 4] final result: %s\n", verify_buf);
        fs_close(fd);
    }
    reader_exit(shared_lock, 4);

    printf("\n[5] final ls and FAT table\n");
    fs_ls(ROOT_BLOCK);
    print_fat();

    destroy_sync();
    return 0;
}
