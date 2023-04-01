#include "producer-consumer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2
#define NUM_ELEMENTS 3

pc_queue_t queue;

// producer_thread_func: function that will be run by the producer threads
void *producer_thread_func(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        int *elem = malloc(sizeof(int));
        *elem = i;
        int res = pcq_enqueue(&queue, elem);
        assert(res == 0);
    }
    return NULL;
}

// consumer_thread_func: function that will be run by the consumer threads
void *consumer_thread_func(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        int *elem = pcq_dequeue(&queue);
        assert(0<=*elem && *elem<NUM_ELEMENTS);
        free(elem);
    }
    return NULL;
}

int main() {
    int res = pcq_create(&queue, 2);
    assert(res == 0);

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    // create the producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        int res2 = pthread_create(&producers[i], NULL, producer_thread_func, NULL);
        assert(res2 == 0);
    }

    // create the consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        int res2 = pthread_create(&consumers[i], NULL, consumer_thread_func, NULL);
        assert(res2 == 0);
    }

    // wait for the producer threads to finish
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        int res2 = pthread_join(producers[i], NULL);
        assert(res2 == 0);
    }

    // wait for the consumer threads to finish
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        int res2 = pthread_join(consumers[i], NULL);
        assert(res2 == 0);
    }

    // verify that the queue is empty and in a consistent state
    assert(queue.pcq_current_size == 0);
    assert(queue.pcq_head == 0);
    assert(queue.pcq_tail == 0);

    res = pcq_destroy(&queue);
    assert(res == 0);

    return 0;
}
