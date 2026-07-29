/*
 * ============================================================================
 * food_delivery_system.c
 *
 * A simulation of a multithreaded order processing system for an online
 * food delivery platform, using POSIX threads, mutexes, and condition
 * variables.
 *
 * SYSTEM OVERVIEW
 * ----------------------------------------------------------------------
 * Three threads cooperate around a single fixed-size shared queue
 * (capacity 5, implemented as a circular buffer):
 *
 *   1. Kitchen thread (producer)  - generates orders, takes 2s per order,
 *                                   waits when the queue is full.
 *   2. Delivery thread (consumer) - removes orders, takes 4s per order,
 *                                   waits when the queue is empty.
 *   3. Monitor thread             - every 5s, safely prints how many
 *                                   orders have been prepared/delivered
 *                                   and the current queue size.
 *
 * SYNCHRONIZATION DESIGN
 * ----------------------------------------------------------------------
 * All shared state (the queue array, head/tail/count indices, and the
 * prepared/delivered counters) lives inside a single SharedData struct.
 * Every access to ANY field of that struct - by ANY thread - must happen
 * while holding `lock` (a pthread_mutex_t). This is what prevents race
 * conditions: two threads can never read/write shared fields at the
 * same instant.
 *
 * Two condition variables coordinate waiting/waking instead of having
 * threads busy-loop (spin) checking the queue state repeatedly, which
 * would waste CPU and still be unsafe without the mutex:
 *
 *   - not_full  : the kitchen (producer) sleeps on this when the queue
 *                 is full (count == QUEUE_CAPACITY). The delivery thread
 *                 signals it after removing an order, since that frees
 *                 up a slot.
 *   - not_empty : the delivery (consumer) sleeps on this when the queue
 *                 is empty (count == 0). The kitchen thread signals it
 *                 after adding an order, since that gives the consumer
 *                 something to process.
 *
 * Both waits use a `while` loop (not `if`) around pthread_cond_wait().
 * This guards against spurious wakeups (POSIX permits a thread to wake
 * up even without a signal) and against the case where another thread
 * "steals" the resource between the wakeup and when this thread actually
 * resumes execution. Re-checking the condition after waking is what
 * makes the wait genuinely safe.
 *
 * GRACEFUL TERMINATION
 * ----------------------------------------------------------------------
 * The kitchen thread stops generating new orders once TOTAL_ORDERS have
 * been prepared. The delivery thread keeps draining the queue until it's
 * empty AND the kitchen is done, then also exits. Before exiting, both
 * threads broadcast on both condition variables so that no thread is
 * left sleeping forever waiting for a signal that will never come. The
 * monitor thread checks the same `done` condition and prints one final
 * report before exiting.
 *
 * Compile:  gcc -Wall -Wextra -pedantic food_delivery_system.c -o food_delivery_system -pthread
 * Run:      ./food_delivery_system
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>   /* sleep() */
#include <time.h>     /* time(), for simple timestamps in output */
#include <stdarg.h>   /* va_list, va_start, va_end */

#define QUEUE_CAPACITY   5    /* Max orders the kitchen can hold before waiting */
#define TOTAL_ORDERS     20   /* Program stops after this many orders are delivered */
#define KITCHEN_DELAY_SEC   2 /* Seconds to "prepare" one order */
#define DELIVERY_DELAY_SEC  4 /* Seconds to "deliver" one order */
#define MONITOR_INTERVAL_SEC 5 /* How often the monitor reports status */

/* A single customer order. */
typedef struct {
    int order_id;
} Order;

/*
 * All state shared between the Kitchen, Delivery, and Monitor threads.
 *
 * IMPORTANT: every field below must ONLY be read or written while
 * holding `lock`. This struct is the single source of truth for the
 * whole system's concurrent state.
 */
typedef struct {
    Order queue[QUEUE_CAPACITY]; /* Circular buffer storing pending orders */
    int head;                    /* Index of the next order to dequeue */
    int tail;                    /* Index where the next order will be enqueued */
    int count;                   /* Current number of orders in the queue */

    int orders_prepared;         /* Total orders kitchen has ever produced */
    int orders_delivered;        /* Total orders delivery has ever completed */
    int next_order_id;           /* Next unique ID to assign to a new order */

    int kitchen_done;            /* Set to 1 once kitchen has produced TOTAL_ORDERS */
    int all_done;                /* Set to 1 once delivery has drained everything after kitchen_done */

    pthread_mutex_t lock;        /* Guards every field above */
    pthread_cond_t  not_full;    /* Signaled when space frees up in the queue */
    pthread_cond_t  not_empty;   /* Signaled when a new order is added (or shutdown happens) */
} SharedData;

/*
 * A separate mutex, independent of `lock`, used only to keep console
 * output from different threads from interleaving mid-line. This is
 * NOT part of SharedData's producer/consumer synchronization - it exists
 * purely so printf() calls from different threads don't race with each
 * other or garble the terminal.
 */
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initializes all shared state to a known starting point.
 * Must be called once, before any thread is created.
 */
void queue_init(SharedData *data) {
    data->head = 0;
    data->tail = 0;
    data->count = 0;

    data->orders_prepared = 0;
    data->orders_delivered = 0;
    data->next_order_id = 1;     /* Orders are labeled starting from #1 */

    data->kitchen_done = 0;
    data->all_done = 0;

    /* NULL = use default attributes; no special mutex/condvar behavior needed */
    pthread_mutex_init(&data->lock, NULL);
    pthread_cond_init(&data->not_full, NULL);
    pthread_cond_init(&data->not_empty, NULL);
}

/* Releases OS resources held by the mutex/condition variables. */
void queue_destroy(SharedData *data) {
    pthread_mutex_destroy(&data->lock);
    pthread_cond_destroy(&data->not_full);
    pthread_cond_destroy(&data->not_empty);
}

/*
 * Small helper for readable, timestamped console output.
 *
 * Thread-safety notes:
 *   - localtime() is NOT thread-safe (it writes into a shared static
 *     buffer internally), so we use localtime_r(), which writes into a
 *     buffer we own (`tm_buf`) instead. Each thread's call gets its own
 *     stack-local struct, avoiding any shared state there.
 *   - We still lock `print_lock` around the actual printing so that
 *     output from different threads doesn't interleave mid-line (e.g.
 *     two threads' printf calls mixing into one garbled line).
 */
static void print_ts(const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);

    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&print_lock);
    printf("[%s] ", buf);
    vprintf(fmt, args);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);

    va_end(args);
}

/*
 * enqueue(): called by the Kitchen (producer) thread to add a freshly
 * prepared order to the shared queue.
 *
 * Locking behavior:
 *   1. Lock the mutex - no one else may touch shared state while we work.
 *   2. While the queue is full, wait on `not_full`. pthread_cond_wait()
 *      atomically unlocks the mutex and puts this thread to sleep, so
 *      the delivery thread can still acquire the lock to dequeue and
 *      wake us up. When we wake, the mutex is automatically re-locked
 *      before we continue - we then re-check the `while` condition in
 *      case of a spurious wakeup or another producer sneaking in first.
 *   3. Insert the order at `tail`, advance `tail` circularly (wrapping
 *      back to 0 after reaching QUEUE_CAPACITY - 1), and increment `count`.
 *   4. Signal `not_empty` so a sleeping delivery thread (if any) wakes up.
 *   5. Unlock the mutex.
 */
void enqueue(SharedData *data, Order order) {
    pthread_mutex_lock(&data->lock);

    while (data->count == QUEUE_CAPACITY) {
        /* Queue is full - kitchen must wait for delivery to free a slot. */
        pthread_cond_wait(&data->not_full, &data->lock);
    }

    data->queue[data->tail] = order;
    data->tail = (data->tail + 1) % QUEUE_CAPACITY;  /* circular wrap-around */
    data->count++;

    /* A new order is available - wake a waiting delivery thread, if any. */
    pthread_cond_signal(&data->not_empty);

    pthread_mutex_unlock(&data->lock);
}

/*
 * dequeue(): called by the Delivery (consumer) thread to remove the next
 * order from the shared queue.
 *
 * Returns 1 if an order was successfully dequeued into *out_order.
 * Returns 0 if the system is fully shut down and there will never be
 * another order to dequeue (used to let the consumer exit cleanly).
 *
 * Locking behavior mirrors enqueue(): lock, wait-while-empty (checking
 * for shutdown too), remove from `head`, advance `head` circularly,
 * decrement `count`, signal `not_full`, unlock.
 */
int dequeue(SharedData *data, Order *out_order) {
    pthread_mutex_lock(&data->lock);

    while (data->count == 0 && !data->kitchen_done) {
        /* Queue is empty and kitchen might still produce more - wait. */
        pthread_cond_wait(&data->not_empty, &data->lock);
    }

    if (data->count == 0 && data->kitchen_done) {
        /* Nothing left to deliver, and kitchen will never add more. */
        pthread_mutex_unlock(&data->lock);
        return 0;
    }

    *out_order = data->queue[data->head];
    data->head = (data->head + 1) % QUEUE_CAPACITY;  /* circular wrap-around */
    data->count--;

    /* A slot just freed up - wake a waiting kitchen thread, if any. */
    pthread_cond_signal(&data->not_full);

    pthread_mutex_unlock(&data->lock);
    return 1;
}

/*
 * Kitchen (producer) thread function.
 *
 * Generates orders with unique incremental IDs, "prepares" each one by
 * sleeping KITCHEN_DELAY_SEC seconds, then enqueues it. Stops once
 * TOTAL_ORDERS have been prepared, then marks kitchen_done and wakes
 * any consumer that might be waiting forever on an empty queue.
 */
void *kitchen_thread(void *arg) {
    SharedData *data = (SharedData *)arg;

    for (;;) {
        pthread_mutex_lock(&data->lock);
        if (data->orders_prepared >= TOTAL_ORDERS) {
            pthread_mutex_unlock(&data->lock);
            break;
        }
        pthread_mutex_unlock(&data->lock);

        /* Simulate the time it takes to prepare an order. This happens
         * OUTSIDE the lock, since it doesn't touch shared state - holding
         * the mutex during a 2-second sleep would needlessly block the
         * other threads from making progress. */
        sleep(KITCHEN_DELAY_SEC);

        pthread_mutex_lock(&data->lock);
        Order order;
        order.order_id = data->next_order_id++;
        data->orders_prepared++;
        int prepared_count = data->orders_prepared;
        pthread_mutex_unlock(&data->lock);

        print_ts("Kitchen: prepared Order #%d (total prepared: %d)\n",
                  order.order_id, prepared_count);

        enqueue(data, order);
    }

    /* No more orders will ever be produced. Mark done and wake anyone
     * waiting on not_empty so they can notice and exit instead of
     * sleeping forever. */
    pthread_mutex_lock(&data->lock);
    data->kitchen_done = 1;
    pthread_cond_broadcast(&data->not_empty);
    pthread_mutex_unlock(&data->lock);

    print_ts("Kitchen: all %d orders prepared. Shutting down.\n", TOTAL_ORDERS);
    return NULL;
}

/*
 * Delivery (consumer) thread function.
 *
 * Repeatedly dequeues an order and "delivers" it by sleeping
 * DELIVERY_DELAY_SEC seconds, then updates the delivered counter.
 * Exits once dequeue() reports there is nothing left to deliver
 * (queue empty AND kitchen done).
 */
void *delivery_thread(void *arg) {
    SharedData *data = (SharedData *)arg;
    Order order;

    while (dequeue(data, &order)) {
        /* Simulate delivery time outside the lock. */
        sleep(DELIVERY_DELAY_SEC);

        pthread_mutex_lock(&data->lock);
        data->orders_delivered++;
        int delivered_count = data->orders_delivered;
        int all_done = (data->kitchen_done && data->count == 0 &&
                         data->orders_delivered >= TOTAL_ORDERS);
        if (all_done) {
            data->all_done = 1;
        }
        pthread_mutex_unlock(&data->lock);

        print_ts("Delivery: delivered Order #%d (total delivered: %d)\n",
                  order.order_id, delivered_count);
    }

    print_ts("Delivery: no more orders coming. Shutting down.\n");
    return NULL;
}

/*
 * Monitor thread function.
 *
 * Every MONITOR_INTERVAL_SEC seconds, safely takes a snapshot of the
 * shared counters and queue size (while holding the lock just long
 * enough to copy the values out, not while printing) and reports them.
 * Exits once the whole system has finished (all_done flag set).
 */
void *monitor_thread(void *arg) {
    SharedData *data = (SharedData *)arg;

    for (;;) {
        sleep(MONITOR_INTERVAL_SEC);

        pthread_mutex_lock(&data->lock);
        int prepared = data->orders_prepared;
        int delivered = data->orders_delivered;
        int queue_size = data->count;
        int finished = data->all_done;
        pthread_mutex_unlock(&data->lock);

        print_ts("--- Monitor Report ---\n");
        print_ts("Orders prepared: %d\n", prepared);
        print_ts("Orders delivered: %d\n", delivered);
        print_ts("Current Queue size: %d\n", queue_size);
        print_ts("----------------------\n");

        if (finished) {
            break;
        }
    }

    print_ts("Monitor: system finished. Shutting down.\n");
    return NULL;
}

int main(void) {
    SharedData data;
    queue_init(&data);

    pthread_t kitchen_tid, delivery_tid, monitor_tid;

    print_ts("Starting food delivery simulation "
              "(capacity=%d, total_orders=%d)\n", QUEUE_CAPACITY, TOTAL_ORDERS);

    pthread_create(&kitchen_tid, NULL, kitchen_thread, &data);
    pthread_create(&delivery_tid, NULL, delivery_thread, &data);
    pthread_create(&monitor_tid, NULL, monitor_thread, &data);

    /* Wait for all threads to finish before cleaning up and exiting. */
    pthread_join(kitchen_tid, NULL);
    pthread_join(delivery_tid, NULL);
    pthread_join(monitor_tid, NULL);

    print_ts("All threads joined. Final counts -> prepared: %d, delivered: %d\n",
              data.orders_prepared, data.orders_delivered);

    queue_destroy(&data);
    return 0;
}