# Multithreaded Order Processing System (Food Delivery Simulation)

A C program that simulates an online food delivery platform's order pipeline
using POSIX threads, mutexes, and condition variables. Three threads —
**Kitchen** (producer), **Delivery** (consumer), and **Monitor** — cooperate
around a fixed-size shared queue, with no race conditions and no busy-waiting.

## Overview

- The **Kitchen thread** generates orders continuously, assigning each an
  incremental ID, and takes 2 seconds to "prepare" each order before placing
  it in the shared queue.
- The **Delivery thread** removes orders from the queue and takes 4 seconds
  to "deliver" each one.
- The **Monitor thread** wakes up every 5 seconds and safely reports:
  - Orders prepared
  - Orders delivered
  - Current queue size
- The shared queue has a fixed capacity of **5 orders**. The kitchen waits
  when the queue is full; delivery waits when the queue is empty.
- The simulation runs until a fixed number of orders (**20**, configurable)
  have been prepared and delivered, then all three threads shut down cleanly.

## Files

| File | Purpose |
|---|---|
| `food_delivery_system.c` | Full source code |
| `sample_execution_output.txt` | Captured output from a complete run |
| `README.md` | This file |

## Building

Requires `gcc` and a POSIX threads (`pthread`) library — available by default
on Linux/macOS.

```bash
gcc -Wall -Wextra -pedantic food_delivery_system.c -o food_delivery_system -pthread
```

Compiles cleanly with no warnings.

## Running

```bash
./food_delivery_system
```

The program runs to completion automatically (no input required) and prints
timestamped log lines as orders are prepared, delivered, and monitored. A
full run takes a little over a minute, since delivery (4s/order) is the
bottleneck for 20 orders.

To adjust the simulation size or timing, edit the `#define` constants at the
top of `food_delivery_system.c`:

```c
#define QUEUE_CAPACITY   5    // Max orders the kitchen can hold before waiting
#define TOTAL_ORDERS     20   // Program stops after this many orders are delivered
#define KITCHEN_DELAY_SEC   2 // Seconds to "prepare" one order
#define DELIVERY_DELAY_SEC  4 // Seconds to "deliver" one order
#define MONITOR_INTERVAL_SEC 5 // How often the monitor reports status
```

## Design

### Shared state

All state shared between threads — the queue array, `head`/`tail`/`count`
indices, and the `orders_prepared`/`orders_delivered` counters — lives in a
single `SharedData` struct. **Every** field in that struct is only ever read
or written while holding `SharedData.lock` (a `pthread_mutex_t`). This is
what prevents race conditions: two threads can never touch shared data at
the same instant.

### The queue

The queue is a **circular buffer** of fixed capacity 5. Using `head`, `tail`,
and an explicit `count` (rather than deriving size purely from `head`/`tail`)
avoids the classic ambiguity where `head == tail` could mean either "empty"
or "full."

- `enqueue()` — locks, waits while `count == QUEUE_CAPACITY`, inserts at
  `tail`, advances `tail` with wraparound (`(tail + 1) % QUEUE_CAPACITY`),
  increments `count`, signals `not_empty`, unlocks.
- `dequeue()` — locks, waits while `count == 0` (and kitchen isn't finished),
  removes from `head`, advances `head` with wraparound, decrements `count`,
  signals `not_full`, unlocks. Returns `0` instead of blocking forever once
  the kitchen is done and the queue is empty, so the delivery thread can
  exit.

### Mutex and condition variable usage

- **`pthread_mutex_t lock`** — guards every field of `SharedData`. Locked
  before, and unlocked immediately after, any access to shared state.
- **`pthread_cond_t not_full`** — the kitchen thread waits on this when the
  queue is full; the delivery thread signals it after removing an order
  (freeing a slot).
- **`pthread_cond_t not_empty`** — the delivery thread waits on this when the
  queue is empty; the kitchen thread signals it after adding an order.
- Both waits use a **`while` loop** around `pthread_cond_wait()`, not an
  `if` statement. This protects against spurious wakeups (permitted by
  POSIX) and against another thread grabbing the resource between the
  wakeup and this thread resuming — the condition is always re-checked
  after waking.
- `pthread_cond_wait()` atomically unlocks the mutex while sleeping and
  re-locks it before returning, so the waiting thread never holds the lock
  while blocked (which would deadlock the whole system).

### Full / empty queue handling

- **Full queue**: kitchen calls `pthread_cond_wait(&not_full, &lock)` and
  sleeps until delivery frees a slot and signals `not_full`.
- **Empty queue**: delivery calls `pthread_cond_wait(&not_empty, &lock)` and
  sleeps until kitchen adds an order and signals `not_empty` — or until
  kitchen signals shutdown (see below), whichever comes first.

### Monitor thread safety

The monitor thread locks `lock`, copies out `orders_prepared`,
`orders_delivered`, and `count` into local variables, then **unlocks before
printing**. This keeps the critical section as short as possible — the
monitor never holds the lock while doing (comparatively slow) I/O, so it
can't stall the kitchen or delivery threads while printing a report.

### Graceful termination

The kitchen thread stops generating orders once `orders_prepared` reaches
`TOTAL_ORDERS`, then sets `kitchen_done = 1` and calls
`pthread_cond_broadcast(&not_empty)` so the delivery thread (if currently
waiting on an empty queue) wakes up and notices there's nothing left to
wait for, rather than sleeping forever. Delivery keeps draining the queue
until it's empty and `kitchen_done` is set, then exits. The monitor thread
checks an `all_done` flag after each report and exits once the whole system
has finished. `main()` joins all three threads and destroys the mutex and
condition variables before exiting.

### Thread-safe logging

All console output goes through a single `print_ts()` helper that:
- Uses `localtime_r()` instead of `localtime()`, since `localtime()` writes
  into a shared static buffer internally and is **not** thread-safe when
  called concurrently from multiple threads.
- Locks a dedicated `print_lock` mutex (separate from the queue's `lock`)
  around the actual `printf()` call, so log lines from different threads
  never interleave into a garbled line.

This was caught and fixed using **ThreadSanitizer**
(`gcc -fsanitize=thread`), which flagged a real data race in the original
`localtime()`-based implementation. After the fix, a full run under
ThreadSanitizer reports zero data races.

## Verification

- Compiles with `gcc -Wall -Wextra -pedantic` with no warnings.
- Ran to full completion (20 orders prepared and delivered); queue size in
  the monitor reports stays within `[0, 5]` throughout, confirming capacity
  is respected.
- Verified with ThreadSanitizer (`-fsanitize=thread`) across a full run —
  no data races detected.

## Sample output

See `sample_execution_output.txt` for a complete captured run. Excerpt:

```
[06:23:29] Delivery: delivered Order #19 (total delivered: 19)
[06:23:31] --- Monitor Report ---
[06:23:31] Orders prepared: 20
[06:23:31] Orders delivered: 19
[06:23:31] Current Queue size: 0
[06:23:31] ----------------------
[06:23:33] Delivery: delivered Order #20 (total delivered: 20)
[06:23:33] Delivery: no more orders coming. Shutting down.
[06:23:36] --- Monitor Report ---
[06:23:36] Orders prepared: 20
[06:23:36] Orders delivered: 20
[06:23:36] Current Queue size: 0
[06:23:36] ----------------------
[06:23:36] Monitor: system finished. Shutting down.
[06:23:36] All threads joined. Final counts -> prepared: 20, delivered: 20
```