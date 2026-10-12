#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <signal.h>
#include "pcb.h"
#include "utils/Vec.h"

#define NUM_PRIORITY 3

/**
 * @brief Initialize the scheduler and its priority queues
 *
 * Creates three priority queues (one for each priority level 0, 1, 2).
 * Must be called once during kernel initialization before any other
 * scheduler functions are used.
 */
void scheduler_init(void);

/**
 * @brief Clean up scheduler resources and terminate all threads
 *
 * Iterates through all priority queues, cancels and joins all remaining
 * threads, and destroys the queue data structures. Should be called
 * during kernel shutdown.
 */
void scheduler_cleanup(void);

/**
 * @brief Insert a process into the appropriate priority queue
 *
 * Adds the process to the scheduler queue based on its priority level.
 * Only inserts if the process is in RUNNING state (not BLOCKED,
 * STOPPED, or ZOMBIE). Validates priority is in range [0, 2]. Logs a SCHEDULE
 * event upon successful insertion.
 *
 * @param pcb Process control block to insert
 * @return true if successfully inserted, false otherwise
 */
bool scheduler_insert(pcb_t* pcb);

/**
 * @brief Remove a process from its priority queue
 *
 * Searches for the process in its priority queue by PID and removes it.
 * Used when a process needs to be taken out of scheduling (e.g., when
 * it becomes blocked, stopped, or terminated). Validates priority is
 * in range [0, 2].
 *
 * @param pcb Process control block to remove
 * @return true if successfully removed, false if not found or invalid
 */
bool scheduler_remove(pcb_t* pcb);

/**
 * @brief Pop the first process from a specific priority queue
 *
 * Removes and returns the first process from the queue at the given
 * priority level. Uses FIFO ordering within each priority level.
 *
 * @param priority Priority level (0, 1, or 2)
 * @return Pointer to the process, or NULL if queue is empty or invalid priority
 */
pcb_t* scheduler_pop(int priority);

/**
 * @brief Get the next process to schedule based on priority ratios
 *
 * Implements weighted round-robin scheduling with ratios 9:6:4 for
 * priority levels 0:1:2. This means:
 * - Priority 0 runs 1.5x more than priority 1
 * - Priority 1 runs 1.5x more than priority 2
 * - Priority 0 runs 2.25x more than priority 2
 *
 * The algorithm uses counters to track how many times each priority
 * has been scheduled, resetting when all reach their max counts.
 *
 * @return Pointer to next process to run, or NULL if all queues empty
 */
pcb_t* get_next_proc(void);

/**
 * @brief Main scheduler loop - implements preemptive multitasking
 *
 * 1. Sets up signal handling for SIGALRM (quantum timer)
 * 2. Configures the interval timer for 100ms quantums
 * 3. Continuously schedules processes in round-robin with priority weighting
 * 4. Handles context switching between processes
 * 5. Manages sleeping processes (decrements sleep ticks each quantum)
 * 6. Logs all scheduling events (CONTINUED, STOPPED, SCHEDULE, UNBLOCKED)
 *
 * The scheduler runs until scheduler_stop() is called. Each iteration:
 * - Gets next process based on priority ratios (9:6:4)
 * - Sets process state to RUNNING
 * - Increments tick counter and sets logger tick
 * - Handles sleeping process wake-ups (sets them to RUNNING state)
 * - Continues the selected process thread
 * - Waits for SIGALRM (quantum expiration)
 * - Suspends the process thread
 * - Re-inserts running processes back into queue
 *
 * If all processes are blocked, the scheduler suspends itself until
 * a signal arrives (e.g., from I/O completion or process wake-up).
 */
void scheduler(void);

/**
 * @brief Signal the scheduler to stop
 *
 * Sets the internal done flag to true, causing the scheduler main loop
 * to exit after the current quantum completes. Does not immediately
 * interrupt the scheduler.
 */
void scheduler_stop(void);

#endif  // SCHEDULER_H_
