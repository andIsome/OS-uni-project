#pragma once

#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include "betterassert.h"

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Macro string contatination
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b

void closeFile(FILE** f);

void close_fd(int* fd);

#define AUTO_CLOSE_FILE __attribute__((cleanup(closeFile)))

#define AUTO_CLOSE_FD __attribute__((cleanup(close_fd)))

#define OPEN_FILE_FD(variable_name, file_name, options) int variable_name AUTO_CLOSE_FD = open(file_name, options);\
    ALWAYS_ASSERT(variable_name!=-1, "FAILED TO OPEN FILE: %s (%i)", file_name, errno);

#define MTX_INIT(mtx) ALWAYS_ASSERT(pthread_mutex_init(&mtx, NULL)==0, "FAILED TO INITIALIZE MUTEX!")
#define MTX_DESTORY(mtx) ALWAYS_ASSERT(pthread_mutex_destroy(&mtx)==0, "FAILED TO DESTROY MUTEX!")

#define COND_INIT(cond) ALWAYS_ASSERT(pthread_cond_init(&cond, NULL)==0, "FAILED TO INITIALIZE COND VAR!")
#define COND_DESTROY(cond) ALWAYS_ASSERT(pthread_cond_destroy(&cond)==0, "FAILED TO DESTROY COND VAR!")


void mutex_unlock(pthread_mutex_t** mt);

#define INTERNAL_SCOPED_LOCK(mutex, c)\
    pthread_mutex_t* CONCAT(lock, c) __attribute__((cleanup(mutex_unlock)))=&mutex;\
    ALWAYS_ASSERT(pthread_mutex_lock(CONCAT(lock, c))==0, "FAILED TO LOCK MUTEX!")

#define SCOPED_LOCK(mutex) INTERNAL_SCOPED_LOCK(mutex, __COUNTER__)
