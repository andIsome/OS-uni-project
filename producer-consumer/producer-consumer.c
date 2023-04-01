#include "producer-consumer.h"
#include "common.h"

int pcq_create(pc_queue_t* queue, size_t capacity){
    queue->pcq_buffer = malloc(sizeof(void*)*capacity);

    if(queue->pcq_buffer==NULL) return -1;

    MTX_INIT(queue->pcq_current_size_lock);
    MTX_INIT(queue->pcq_head_lock);
    MTX_INIT(queue->pcq_tail_lock);
    MTX_INIT(queue->pcq_pusher_condvar_lock);
    MTX_INIT(queue->pcq_popper_condvar_lock);

    COND_INIT(queue->pcq_pusher_condvar);
    COND_INIT(queue->pcq_popper_condvar);

    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;

    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    return 0;
}

int pcq_destroy(pc_queue_t* queue){
    if(queue->pcq_buffer==NULL) return -1;

    MTX_DESTORY(queue->pcq_current_size_lock);
    MTX_DESTORY(queue->pcq_head_lock);
    MTX_DESTORY(queue->pcq_tail_lock);
    MTX_DESTORY(queue->pcq_pusher_condvar_lock);
    MTX_DESTORY(queue->pcq_popper_condvar_lock);

    COND_DESTROY(queue->pcq_pusher_condvar);
    COND_DESTROY(queue->pcq_popper_condvar);

    queue->pcq_capacity = 0;
    queue->pcq_current_size = 0;

    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    free(queue->pcq_buffer);
    queue->pcq_buffer = NULL;

    return 0;
}



int pcq_enqueue(pc_queue_t* queue, void* elem){

    {
        SCOPED_LOCK(queue->pcq_current_size_lock);

        // When waiting for there to be space make sure, that not only the capacity
        // has been decremented (1rst cond) but also that the element has been removed (2nd cond),
        // as they use different locks
        while(queue->pcq_current_size==queue->pcq_capacity ||
         (queue->pcq_head==queue->pcq_tail && queue->pcq_current_size!=0)){
            ALWAYS_ASSERT(pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_current_size_lock)==0, "Failed to Cond wait");
        }

        queue->pcq_current_size++;
    }


    {
        SCOPED_LOCK(queue->pcq_head_lock);
        queue->pcq_buffer[queue->pcq_head] = elem;
        queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;
    }


    SCOPED_LOCK(queue->pcq_popper_condvar_lock);
    ALWAYS_ASSERT(pthread_cond_signal(&queue->pcq_popper_condvar)==0, "FAILED TO SIGNAL!");

    return 0;
}

void* pcq_dequeue(pc_queue_t *queue){
    void* data = NULL;

    {
        SCOPED_LOCK(queue->pcq_current_size_lock);
        while(queue->pcq_current_size==0 || (queue->pcq_tail==queue->pcq_head && queue->pcq_current_size!=queue->pcq_capacity)){
            ALWAYS_ASSERT(pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_current_size_lock)==0, "Failed to Cond wait");
        }
        queue->pcq_current_size--;
    }

    {
        SCOPED_LOCK(queue->pcq_tail_lock);
        data = queue->pcq_buffer[queue->pcq_tail];
        queue->pcq_buffer[queue->pcq_tail] = NULL;
        queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    }

    SCOPED_LOCK(queue->pcq_pusher_condvar_lock);
    ALWAYS_ASSERT(pthread_cond_signal(&queue->pcq_pusher_condvar)==0, "FAILED TO SIGNAL!");

    return data;
}
