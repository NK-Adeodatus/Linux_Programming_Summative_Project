#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define QUEUE_CAPACITY   5   // Max orders the kitchen can hold before waiting
#define TOTAL_ORDERS     20  // Program stops after this many orders are delivered

// Represents a single customer order
typedef struct {
    int order_id;
} Order;

// All state shared between the Kitchen, Delivery, and Monitor threads.
// Every field below must ONLY be accessed while holding 'lock'.
typedef struct {
    Order queue[QUEUE_CAPACITY]; // Circular buffer storing pending orders
    int head;                    // Index of the next order to dequeue
    int tail;                    // Index where the next order will be enqueued
    int count;                   // Current number of orders in the queue

    int orders_prepared;         // Total orders kitchen has ever produced
    int orders_delivered;        // Total orders delivery has ever completed
    int next_order_id;           // Next unique ID to assign to a new order

    int done;                    // Set to 1 once TOTAL_ORDERS have been delivered

    pthread_mutex_t lock;        // Guards every field above
    pthread_cond_t  not_full;    // Signaled when space frees up in the queue
    pthread_cond_t  not_empty;   // Signaled when a new order is added
} SharedData;