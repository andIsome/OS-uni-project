#include "common.h"
#include "fs/operations.h"

#include <fcntl.h>
#include <unistd.h>

void closeFile(FILE** f){
    if(f==NULL || *f == NULL) return;
    ALWAYS_ASSERT(fclose(*f)==0, "FAILED TO CLOSE FILE!");
    *f = NULL;
}

void close_fd(int* fd){
    if(*fd == -1) return;
    ALWAYS_ASSERT(close(*fd)==0, "FAILED TO CLOSE FD!");
    *fd = -1;
}

void mutex_unlock(pthread_mutex_t** mt) {
    ALWAYS_ASSERT(pthread_mutex_unlock(*mt)==0, "FAILED TO UNLOCK MUTEX!");
}