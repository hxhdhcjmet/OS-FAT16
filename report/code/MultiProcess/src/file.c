bool fs_delete(int pid, const char *filename)
{
    if (pid < 0 || pid >= MAX_PROCESS || filename == NULL) {
        return false;
    }

    int cur_dir = ProcessTable[pid].current_dir;

    pthread_mutex_lock(&fs_mutex);

    for (int i = 0; i < MAX_ENTRY; i++) {
        if (Directory[i].ac == 1 &&
            Directory[i].parent == cur_dir &&
            Directory[i].type == 0 &&
            strcmp(Directory[i].name, filename) == 0) {

            pthread_mutex_lock(&Directory[i].mutex);

            if (Directory[i].reader_count > 0) {
                pthread_mutex_unlock(&Directory[i].mutex);
                pthread_mutex_unlock(&fs_mutex);

                printf("Failed to delete file: %s is in use\n", filename);
                return false;
            }

            pthread_mutex_unlock(&Directory[i].mutex);

            if (sem_trywait(&Directory[i].rw_lock) != 0) {
                pthread_mutex_unlock(&fs_mutex);

                printf("Failed to delete file: %s is in use\n", filename);
                return false;
            }

            int cur = Directory[i].start_block;

            while (cur != FAT_END) {
                int next = FAT[cur];
                FAT[cur] = FAT_FREE;
                cur = next;
            }

            Directory[i].ac = 0;
            Directory[i].size = 0;
            Directory[i].start_block = FAT_END;

            sem_post(&Directory[i].rw_lock);
            pthread_mutex_unlock(&fs_mutex);

            printf("File deleted successfully: %s\n", filename);
            return true;
        }
    }

    pthread_mutex_unlock(&fs_mutex);

    printf("Failed to delete file: %s\n", filename);
    return false;
}