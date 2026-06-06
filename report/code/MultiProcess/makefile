# 编译器
CC = gcc
# 编译选项：仅包含本项目的 include 目录，输出警告，调试信息
CFLAGS = -Iinclude -Wall -g
# 链接选项（无需线程库）
LDFLAGS = -lpthread

# 获取本目录下 src/ 中的所有 .c 源文件
SRCS = $(wildcard src/*.c)
# 将源文件替换为 obj/ 下的 .o 文件
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))
# 最终可执行文件（放在 bin/ 下）
TARGET = bin/app

# 默认目标
all: $(TARGET)

# 链接阶段
$(TARGET): $(OBJS) | bin
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# 编译阶段：将 src/*.c 编译为 obj/*.o
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# 创建 bin 和 obj 目录
bin obj:
	mkdir -p $@

# 清理
clean:
	rm -rf obj bin

.PHONY: all clean