#include "fat.h"

//mkdir
bool mkdir(const char *dirname){
    for (int i = 0; i < MAX_ENTRY;i++){
        if (Directory[i].ac == 0)//找到空闲目录项
        {
            strncpy(Directory[i].name,dirname,DIR_NAME_LENGTH);
            Directory[i].size = 0;
            Directory[i].start_block = -1;
            Directory[i].ac = 1;
            Directory[i].type = 1;//1表示目录
            Directory[i].parent = current_dir;//设置父目录索引
            return true;
        }
    }
    return false; // 目录创建失败返回false
}

int get_dir_size(int dir_index){
    int total = 0;
    
    for (int i = 0; i < MAX_ENTRY; i++){
        if (Directory[i].ac == 1 && Directory[i].parent == dir_index){
            if (Directory[i].type == 0){
                //普通文件
                total += Directory[i].size;            
            }else if (Directory[i].type == 1){
                //目录文件,递归统计
                total += get_dir_size(i);
            }
        }
    }
    return total;//统计dir_index号目录下的文件、目录项总个数
}

//ls
bool ls(){
    for (int i = 0; i < MAX_ENTRY;i++){
        //遍历所有Directory,已分配且在当前文件下的文件和目录list出来
        if (Directory[i].ac == 1 && Directory[i].parent == current_dir){
            int show_size;
            if (Directory[i].type == 1){
                show_size = get_dir_size(i);
            }else{
                show_size = Directory[i].size;
            }

            printf("%s\t%s\t%d bytes\n",
            Directory[i].type == 1 ? "DIR":"FILE",
            Directory[i].name,
            show_size
            );
        }
    }
    return true;
}

// cd 
bool cd(const char *dirname){
    if (dirname == NULL) return false;

    // 回到根目录
    if (strcmp(dirname,"/") == 0){
        current_dir = 0;
        return true;
    }
    
    // 回到父目录
    if (strcmp(dirname,"..") == 0){
        if(Directory[current_dir].parent != -1){//根目录往上翻还是在根目录
            current_dir = Directory[current_dir].parent;
        }
        return true;
    }
    

    //进入下级目录
    for (int i = 0; i < MAX_ENTRY; i++){
        if (Directory[i].ac == 1 && Directory[i].parent == current_dir && Directory[i].type == 1 && 
            strcmp(Directory[i].name,dirname) == 0){
                current_dir = i;
                return true;
            }
    }
    return false;
}