#include "fat.h"

void print_path_recursive(int dir)
{
    if (dir == 0) {
        printf("/");
        return;
    }

    int parent = Directory[dir].parent;

    print_path_recursive(parent);

    if (parent != 0) {
        printf("/");
    }

    printf("%s", Directory[dir].name);
}

void print_prompt(int pid)
{
    printf("miniFS[%d]:", pid);
    print_path_recursive(ProcessTable[pid].current_dir);
    printf("$ ");
}

int main()
{
    char line[256];
    char cmd[32];
    char arg1[64];
    char arg2[128];
    char buffer[1024];
    int current_pid = 0;   // 默认使用进程 0

    init_fs();

    while (1) {
        print_prompt(current_pid);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        cmd[0] = '\0';
        arg1[0] = '\0';
        arg2[0] = '\0';

        sscanf(line, "%31s %63s %127[^\n]", cmd, arg1, arg2);

        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        //切换当前操作的进程 
        else if (strcmp(cmd, "pid") == 0) {
            int new_pid;
            if (sscanf(line, "%31s %d", cmd, &new_pid) == 2) {
                if (new_pid >= 0 && new_pid < MAX_PROCESS) {
                    current_pid = new_pid;
                    printf("switched to pid %d\n", current_pid);
                } else {
                    printf("invalid pid (0-%d)\n", MAX_PROCESS - 1);
                }
            } else {
                printf("usage: pid <0-%d>\n", MAX_PROCESS - 1);
            }
        }

        else if (strcmp(cmd, "ls") == 0) {
            ls(current_pid);
        }

        else if (strcmp(cmd, "mkdir") == 0) {
            if (strlen(arg1) == 0) {
                printf("usage: mkdir dirname\n");
            } else if (!mkdir(current_pid, arg1)) {
                printf("mkdir failed: %s\n", arg1);
            }
        }

        else if (strcmp(cmd, "cd") == 0) {
            if (strlen(arg1) == 0) {
                printf("usage: cd dirname\n");
            } else if (!cd(current_pid, arg1)) {
                printf("cd failed: %s\n", arg1);
            }
        }

        else if (strcmp(cmd, "touch") == 0) {
            if (strlen(arg1) == 0) {
                printf("usage: touch filename\n");
            } else {
                fs_create(current_pid, arg1);
            }
        }

        else if (strcmp(cmd, "rm") == 0) {
            if (strlen(arg1) == 0) {
                printf("usage: rm filename\n");
            } else {
                fs_delete(current_pid, arg1);
            }
        }

        /*
         * open  filename mode
         * 例如：
         * open  a.txt rw
         * （使用当前 pid）
         */
        else if (strcmp(cmd, "open") == 0) {
            char mode_str[16];
            char filename[64];

            if (sscanf(line, "%31s %15s %63s", cmd,  filename,mode_str) != 3) {
                printf("usage: open  filename r|w|rw\n");
                continue;
            }

            int mode;

            if (strcmp(mode_str, "r") == 0) {
                mode = MODE_READ;
            } else if (strcmp(mode_str, "w") == 0) {
                mode = MODE_WRITE;
            } else if (strcmp(mode_str, "rw") == 0) {
                mode = MODE_RDWR;
            } else {
                printf("invalid mode: %s\n", mode_str);
                continue;
            }

            fs_open(current_pid, filename, mode);
        }

        /*
         * close fd
         * 例如：
         * close 0
         * （使用当前 pid）
         */
        else if (strcmp(cmd, "close") == 0) {
            int fd;

            if (sscanf(line, "%31s %d", cmd, &fd) != 2) {
                printf("usage: close fd\n");
                continue;
            }

            fs_close(current_pid, fd);
        }

        /*
         * write fd content
         * 例如：
         * write 0 hello world
         * （使用当前 pid）
         */
        else if (strcmp(cmd, "write") == 0) {
            int fd;
            char content[128];

            if (sscanf(line, "%31s %d %127[^\n]", cmd, &fd, content) != 3) {
                printf("usage: write fd content\n");
                continue;
            }

            fs_write(current_pid, fd, content, strlen(content));
        }

        /*
         * read fd size
         * 例如：
         * read 0 5
         * （使用当前 pid）
         */
        else if (strcmp(cmd, "read") == 0) {
            int fd, size;

            if (strlen(arg1) == 0) {
                printf("usage: read fd [size]\n");
                printf("  if size omitted, reads entire file from current offset\n");
                continue;
            }

            fd = atoi(arg1);

            if (strlen(arg2) == 0) {
                // 未指定 size：读到文件尾
                int dir_idx = ProcessTable[current_pid].fd_table[fd].dir_index;
                int file_size = Directory[dir_idx].size;
                int offset = ProcessTable[current_pid].fd_table[fd].offset;
                size = file_size - offset;
                if (size <= 0) size = 0;
                if (size > (int)sizeof(buffer) - 1)
                    size = (int)sizeof(buffer) - 1;
            } else {
                size = atoi(arg2);
                if (size > (int)sizeof(buffer) - 1)
                    size = (int)sizeof(buffer) - 1;
            }

            int n = fs_read(current_pid, fd, buffer, size);

            if (n >= 0) {
                buffer[n] = '\0';
                printf("%s\n", buffer);
            }
        }

        /*
         * seek fd offset
         * 例如：
         * seek 0 0
         * （使用当前 pid）
         */
        else if (strcmp(cmd, "seek") == 0) {
            int fd;
            int offset;

            if (sscanf(line, "%31s %d %d", cmd, &fd, &offset) != 3) {
                printf("usage: seek fd offset\n");
                continue;
            }

            if (!fs_seek(current_pid, fd, offset)) {
                printf("seek failed\n");
            }
        }

        else if (strcmp(cmd, "cls") == 0) {
            printf("\033[2J\033[H");
            fflush(stdout);
        }

        else if (strcmp(cmd, "help") == 0) {
            printf("commands:\n");
            printf("  pid <0-%d>       — switch process\n", MAX_PROCESS - 1);
            printf("  ls\n");
            printf("  mkdir dirname\n");
            printf("  cd dirname\n");
            printf("  touch filename\n");
            printf("  rm filename\n");
            printf("  open r|w|rw filename\n");
            printf("  close fd\n");
            printf("  write fd content\n");
            printf("  read fd [size]         — omit size to read entire file from current offset\n");
            printf("  seek fd offset\n");
            printf("  cls                    — clear screen\n");
            printf("  exit\n");
        }

        else {
            printf("unknown command: %s\n", cmd);
            printf("type 'help' for help\n");
        }
    }

    return 0;
}
