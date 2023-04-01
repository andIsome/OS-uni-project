#include "vector.h"
#include <memory.h>

void vector_create(vector* v, size_t initial_capacity){
    v->buff = malloc(sizeof(void*)*initial_capacity);
    ALWAYS_ASSERT(v->buff!=NULL, "NO MEMORY!\n");
    v->size=0;
    v->capacity = initial_capacity;
}

void vector_push(vector* v, void* data) {
    if(v == NULL || data == NULL) PANIC("Invalid vector pointer.");
    
    if(v->size == v->capacity) {
        size_t inc = v->capacity / 2;
        if (inc < 1) inc = 1;

        if (v->capacity + inc > 100000)
            PANIC("Vector capacity exceeded maximum limit.");
        
        v->buff = realloc(v->buff, (v->capacity + inc) * sizeof(void*));
        
        if(v->buff == NULL) PANIC("NO MEMORY!\n");
        
        v->capacity += inc;
    }
    v->buff[v->size++] = data;
}

void vector_sort(vector* v, int(cmp_func)(const void*, const void*)){
    if(v->size > 1) {
        qsort(v->buff, v->size, sizeof(void*), cmp_func);
    }
}

void vector_destory(vector* v){
    if(v->buff==NULL) return;
    for(int i=0;i<v->size;i++){
        free(v->buff[i]);
    }
    free(v->buff);
    v->size = 0;
    v->capacity = 0;
    v->buff = NULL;
}