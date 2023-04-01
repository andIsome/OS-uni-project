#pragma once

#include "common.h"

typedef struct{
    void** buff;
    size_t size, capacity;
} vector;

void vector_create(vector* v, size_t initial_capacity);

void vector_push(vector* v, void* data);

void vector_sort(vector* v, int(cmp_func)(const void*, const void*));

void vector_destory(vector* v);