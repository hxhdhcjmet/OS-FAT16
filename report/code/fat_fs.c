#include <stdio.h>
#include <string.h>

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

char Disk[DISK_MAXLEN];

typedef struct {
    int block_size;
    int block_num;
    int fat_start;
    int root_dir_block;
    int data_start;
} SuperBlock;

typedef struct {
    char name[NAME_LEN]; //文件名
    unsigned char used; //是否被使用
    unsigned char type; //文件类型：文件或目录
    unsigned short first_block; //第一个数据块的索引（起始块号）
    unsigned short size; //文件大小（字节）
    char reserved[10];
} DirEntry;

SuperBlock* get_super_block() {
    return (SuperBlock*)(&Disk[0]);
}

unsigned short* get_fat() {
    return (unsigned short*)(&Disk[BLOCK_SIZE]);
}

DirEntry* get_dir_entries(int block_id) {
    return (DirEntry*)(&Disk[block_id * BLOCK_SIZE]);
}

char* get_block_addr(int block_id) {
    return &Disk[block_id * BLOCK_SIZE];
}

void fs_format() {
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

    fat[0] = FAT_END;        // 超级块占用
    fat[1] = FAT_END;        // FAT 表占用
    fat[2] = FAT_END;        // 根目录占用

    memset(get_block_addr(ROOT_BLOCK), 0, BLOCK_SIZE);

    printf("format success\n");
}

void print_fat() {
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

int alloc_block() {
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

    while (current != FAT_END && current != FAT_FREE && current >= 0 && current < BLOCK_NUM) {
        int next = fat[current];
        fat[current] = FAT_FREE;
        memset(get_block_addr(current), 0, BLOCK_SIZE);

        if (next == FAT_END) {
            break;
        }

        current = next;
    }
}


int entries_per_block() {
    return BLOCK_SIZE / sizeof(DirEntry);
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

int fs_mkdir(int current_dir, const char* dirname) {
    if (find_entry(current_dir, dirname) != NULL) {
        printf("mkdir failed: %s already exists\n", dirname);
        return -1;
    }

    DirEntry* empty = find_empty_entry(current_dir);
    if (empty == NULL) {
        printf("mkdir failed: directory is full\n");
        return -1;
    }

    int block = alloc_block();
    if (block == -1) {
        printf("mkdir failed: no free block\n");
        return -1;
    }

    memset(empty, 0, sizeof(DirEntry));
    strncpy(empty->name, dirname, NAME_LEN - 1);
    empty->used = 1;
    empty->type = TYPE_DIR;
    empty->first_block = block;
    empty->size = 0;

    memset(get_block_addr(block), 0, BLOCK_SIZE);

    printf("mkdir success: %s, block=%d\n", dirname, block);
    return 0;
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


#define MAX_OPEN 8

typedef struct {
    int used;
    int dir_block;
    char filename[NAME_LEN];
    char mode;
} OpenFile;

OpenFile open_table[MAX_OPEN];

void init_open_table() {
    memset(open_table, 0, sizeof(open_table));
}

DirEntry* create_file_entry(int current_dir, const char* filename) {
    DirEntry* empty = find_empty_entry(current_dir);

    if (empty == NULL) {
        return NULL;
    }

    memset(empty, 0, sizeof(DirEntry));
    strncpy(empty->name, filename, NAME_LEN - 1);
    empty->used = 1;
    empty->type = TYPE_FILE;
    empty->first_block = FAT_END;
    empty->size = 0;

    return empty;
}

int fs_open(int current_dir, const char* filename, char mode) {
    DirEntry* entry = find_entry(current_dir, filename);

    if (mode == 'r') {
        if (entry == NULL || entry->type != TYPE_FILE) {
            printf("open failed: %s not found\n", filename);
            return -1;
        }
    } else if (mode == 'w') {
        if (entry == NULL) {
            entry = create_file_entry(current_dir, filename);
            if (entry == NULL) {
                printf("open failed: no empty directory entry\n");
                return -1;
            }
        } else if (entry->type != TYPE_FILE) {
            printf("open failed: %s is not a file\n", filename);
            return -1;
        }
    } else {
        printf("open failed: invalid mode\n");
        return -1;
    }

    for (int i = 0; i < MAX_OPEN; i++) {
        if (!open_table[i].used) {
            open_table[i].used = 1;
            open_table[i].dir_block = current_dir;
            strncpy(open_table[i].filename, filename, NAME_LEN - 1);
            open_table[i].mode = mode;

            printf("open success: %s, fd=%d, mode=%c\n", filename, i, mode);
            return i;
        }
    }

    printf("open failed: open table full\n");
    return -1;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN || !open_table[fd].used) {
        printf("close failed: invalid fd\n");
        return -1;
    }

    printf("close success: fd=%d\n", fd);
    open_table[fd].used = 0;
    return 0;
}


int fs_write(int fd, const char* data) {
    if (fd < 0 || fd >= MAX_OPEN || !open_table[fd].used) {
        printf("write failed: invalid fd\n");
        return -1;
    }

    if (open_table[fd].mode != 'w') {
        printf("write failed: file not opened in write mode\n");
        return -1;
    }

    DirEntry* entry = find_entry(open_table[fd].dir_block, open_table[fd].filename);
    if (entry == NULL || entry->type != TYPE_FILE) {
        printf("write failed: file entry not found\n");
        return -1;
    }

    if (entry->first_block != FAT_END) {
        free_chain(entry->first_block);
    }

    int len = strlen(data);
    int remaining = len;
    int offset = 0;
    int first = -1;
    int prev = -1;

    unsigned short* fat = get_fat();

    while (remaining > 0) {
        int block = alloc_block();
        if (block == -1) {
            printf("write failed: no free block\n");
            return -1;
        }

        if (first == -1) {
            first = block;
        }

        if (prev != -1) {
            fat[prev] = block;
        }

        int write_size = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        memcpy(get_block_addr(block), data + offset, write_size);

        offset += write_size;
        remaining -= write_size;
        prev = block;
    }

    if (first == -1) {
        entry->first_block = FAT_END;
    } else {
        entry->first_block = first;
    }

    entry->size = len;

    printf("write success: fd=%d, size=%d bytes\n", fd, len);
    return len;
}


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
        memcpy(buf + offset, get_block_addr(current), read_size);

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
    DirEntry* entry = find_entry(current_dir, name);

    if (entry == NULL) {
        printf("delete failed: %s not found\n", name);
        return -1;
    }

    if (entry->type == TYPE_FILE) {
        if (entry->first_block != FAT_END) {
            free_chain(entry->first_block);
        }

        printf("delete file success: %s\n", name);
        memset(entry, 0, sizeof(DirEntry));
        return 0;
    }

    if (entry->type == TYPE_DIR) {
        DirEntry* sub_entries = get_dir_entries(entry->first_block);
        int count = entries_per_block();

        for (int i = 0; i < count; i++) {
            if (sub_entries[i].used) {
                printf("delete failed: directory %s is not empty\n", name);
                return -1;
            }
        }

        free_chain(entry->first_block);
        printf("delete directory success: %s\n", name);
        memset(entry, 0, sizeof(DirEntry));
        return 0;
    }

    return -1;
}

int main() {
    char buf[512];
    int fd;

    fs_format();
    init_open_table();

    printf("\n[1] initial ls\n");
    fs_ls(ROOT_BLOCK);
    print_fat();

    printf("\n[2] mkdir dir1\n");
    fs_mkdir(ROOT_BLOCK, "dir1");
    fs_ls(ROOT_BLOCK);
    print_fat();

    printf("\n[3] open and write a.txt\n");
    fd = fs_open(ROOT_BLOCK, "a.txt", 'w');
    fs_write(fd, "Hello FAT File System!");
    fs_close(fd);

    fs_ls(ROOT_BLOCK);
    print_fat();

    printf("\n[4] read a.txt\n");
    fd = fs_open(ROOT_BLOCK, "a.txt", 'r');
    fs_read(fd, buf, sizeof(buf));
    printf("read result: %s\n", buf);
    fs_close(fd);

    printf("\n[5] delete a.txt\n");
    fs_delete(ROOT_BLOCK, "a.txt");
    fs_ls(ROOT_BLOCK);
    print_fat();


    printf("\n[6] write long file big.txt\n");

    char big_data[300];
    for (int i = 0; i < 299; i++) {
        big_data[i] = 'A' + (i % 26);
    }
    big_data[299] = '\0';

    fd = fs_open(ROOT_BLOCK, "big.txt", 'w');
    fs_write(fd, big_data);
    fs_close(fd);

    fs_ls(ROOT_BLOCK);
    print_fat();

    printf("\n[7] read long file big.txt\n");
    fd = fs_open(ROOT_BLOCK, "big.txt", 'r');
    fs_read(fd, buf, sizeof(buf));
    printf("read big.txt first part: %.100s\n", buf);
    fs_close(fd);


    return 0;
}