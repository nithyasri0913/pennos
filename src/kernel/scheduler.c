#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "scheduler.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "kernel.h"
#include "logging.h"
#include "pcb.h"
#include "utils/Vec.h"
#include "utils/spthread.h"
#include "../FAT/s_functions.h"

static Vec processes_queue[NUM_PRIORITY];
static const int quantum = 100000;  // 100 milliseconds
static const int priority_max_count[NUM_PRIORITY] = {9, 6, 4};
volatile sig_atomic_t priority_counter[NUM_PRIORITY] = {0};
volatile sig_atomic_t tick_count = 0;

static bool done = false;

// signal handler for sigalarm of quantum
static void alarm_handler(int signum) {}

// initialize priority queues
void scheduler_init() {
  for (int p = 0; p < NUM_PRIORITY; p++) {
    processes_queue[p] = vec_new(5, NULL);
  }
}

static void cancel_and_join(spthread_t thread) {
  int result;

  result = spthread_cancel(thread);
  if (result != 0) {
    s_fprintf_stderr( "ERROR: scheduler_cleanup - failed to cancel thread (error %d)\n", result);
  }

  result = spthread_continue(thread);
  if (result != 0 && result != ESRCH) {
    s_fprintf_stderr( "ERROR: scheduler_cleanup - failed to continue thread (error %d)\n", result);
  }

  result = spthread_suspend(thread);  // forces the spthread to hit a cancellation point
  if (result != 0 && result != ESRCH) {
    s_fprintf_stderr( "ERROR: scheduler_cleanup - failed to suspend thread (error %d)\n", result);
  }

  fputs("waiting on a thread to exit.\n", stderr);
  result = spthread_join(thread, NULL);
  if (result != 0) {
    s_fprintf_stderr( "ERROR: scheduler_cleanup - failed to join thread (error %d)\n", result);
  }
}

void scheduler_cleanup() {
  for (int p = 0; p < NUM_PRIORITY; p++) {
    while (!vec_is_empty(&processes_queue[p])) {
      pcb_t* pcb = (pcb_t*)vec_get(&processes_queue[p], 0);
      vec_erase(&processes_queue[p], 0);
      if (pcb != NULL) {
        cancel_and_join(pcb->thread);
      }
    }
    vec_destroy(&processes_queue[p]);
  }
}

bool scheduler_insert(pcb_t* pcb) {
  // check if it is valid pcb
  if (pcb == NULL) {
    s_fprintf_stderr( "scheduler_insert(): pcb is NULL\n");
    return false;
  }
  if (pcb->priority < 0 || pcb->priority >= NUM_PRIORITY) {
    s_fprintf_stderr(
            "ERROR: scheduler_insert() - invalid priority %d (must be 0, 1, or "
            "2)\n",
            pcb->priority);
    return false;
  }
  // check if it is not blocked
  if (pcb->state != BLOCKED && pcb->state != STOPPED && pcb->state != ZOMBIE) {
    vec_push_back(&processes_queue[pcb->priority], pcb);
    log_schedule(pcb->pid, pcb->priority, pcb->name);
    return true;
  }
  return false;
}

bool scheduler_remove(pcb_t* pcb) {
  // check if it is valid pcb
  if (pcb == NULL) {
    s_fprintf_stderr( "scheduler_remove(): pcb is NULL\n");
    return false;
  }
  if (pcb->priority < 0 || pcb->priority >= NUM_PRIORITY) {
    s_fprintf_stderr(
            "ERROR: scheduler_remove() - invalid priority %d (must be 0, 1, or "
            "2)\n",
            pcb->priority);
    return false;
  }
  // remove the pcb from scheduler queue
  Vec* curr_queue = &processes_queue[pcb->priority];
  for (int i = 0; i < vec_len(curr_queue); i++) {
    pcb_t* p = (pcb_t*)vec_get(curr_queue, i);
    if (p != NULL && p->pid == pcb->pid) {
      vec_erase(curr_queue, i);
      return true;
    }
  }
  return false;
}

pcb_t* scheduler_pop(int priority) {
  if (priority < 0 || priority >= NUM_PRIORITY) {
    return NULL;
  }
  if (!vec_is_empty(&processes_queue[priority])) {
    // get the first process from the queue
    pcb_t* next = (pcb_t*)vec_get(&processes_queue[priority], 0);
    vec_erase(&processes_queue[priority], 0);

    return next;
  }
  return NULL;
}

void scheduler_stop(void) {
  done = true;
}

// return next process and pop it from queue
pcb_t* get_next_proc() {
  // lv 0 run 1.5 times more than lv1, lv 1 runs 1.5 times more than lv2, lv 0
  // runs 2.25 time more than lv2 2.25:1.5:1 = 9:6:4

  // check for each priority queue
  for (int p = 0; p < NUM_PRIORITY; p++) {
    // if it didn't exceed the max count
    if (priority_counter[p] < priority_max_count[p]) {
      pcb_t* next = scheduler_pop(p);
      if (next != NULL) {
        // increment the counter
        priority_counter[p]++;
        return next;
      }
    }
  }

  // when all the counter is reached max, reset the counter
  for (int p = 0; p < NUM_PRIORITY; p++) {
    priority_counter[p] = 0;
  }

  // try again after reset
  for (int p = 0; p < NUM_PRIORITY; p++) {
    pcb_t* next = scheduler_pop(p);
    if (next != NULL) {
      // increment the counter
      priority_counter[p]++;
      return next;
    }
  }
  // even after reset, there is no process in queue (every process are blocked)
  return NULL;
}

void scheduler(void) {
  // mask for while scheduler is waiting for alarm to go off
  sigset_t suspend_set;
  if (sigfillset(&suspend_set) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - sigfillset failed\n");
  }
  if (sigdelset(&suspend_set, SIGALRM) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - sigdelset failed\n");
  }

  // just to make sure that sigalrm doesn't terminate the process
  struct sigaction act = (struct sigaction){
      .sa_handler = alarm_handler,
      .sa_mask = suspend_set,
      .sa_flags = SA_RESTART,
  };
  if (sigaction(SIGALRM, &act, NULL) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - sigaction failed\n");
  }

  // make sure SIGALRM is unblocked
  sigset_t alarm_set;
  if (sigemptyset(&alarm_set) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - sigemptyset failed\n");
  }
  if (sigaddset(&alarm_set, SIGALRM) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - sigaddset failed\n");
  }
  if (pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - pthread_sigmask failed\n");
  }

  struct itimerval it;
  it.it_interval = (struct timeval){.tv_usec = quantum};
  it.it_value = it.it_interval;
  if (setitimer(ITIMER_REAL, &it, NULL) != 0) {
    s_fprintf_stderr( "ERROR: scheduler - setitimer failed\n");
  }

  // locks to check the global value done
  while (!done) {
    pcb_t* curr_proc = get_next_proc();
    if (curr_proc == NULL) {
      // no process in queue, everything blocked
      sigsuspend(&suspend_set);
      continue;
    }
    // change state of current process to RUNNING
    curr_proc->state = RUNNING;
    k_set_current_process(curr_proc);
    tick_count++;

    // pass in the tick counter to logger
    logger_set_ticks(tick_count);

    // Handle sleeping processes - decrement sleep ticks for all blocked processes
    // Do this AFTER tick increment so logging is correct
    pcb_t** process_table = k_get_process_table();
    for (int i = 0; i < MAX_PROCESSES; i++) {
      pcb_t* proc = process_table[i];
      if (proc != NULL && proc->state == BLOCKED && proc->sleep_ticks_remaining > 0) {
        proc->sleep_ticks_remaining--;
        if (proc->sleep_ticks_remaining == 0) {
          // Sleep finished, wake up the process
          proc->state = RUNNING;
          log_unblocked(proc->pid, proc->priority, proc->name);

          int continue_result = spthread_continue(proc->thread);
          if (continue_result != 0) {
            s_fprintf_stderr( "ERROR: scheduler - failed to wake process %d from sleep (error %d)\n",
                    proc->pid, continue_result);
          }

          if (!scheduler_insert(proc)) {
            // This can happen if process exited immediately after waking up
            // Only report if it's not a zombie/stopped/blocked
            if (proc->state != ZOMBIE && proc->state != BLOCKED && proc->state != STOPPED) {
              s_fprintf_stderr( "WARNING: scheduler - could not re-insert woken process %d (state: %d)\n",
                      proc->pid, proc->state);
            }
          }
        }
      }
    }

    // Log CONTINUED before actually continuing the thread to ensure correct log order
    // (the thread will start running immediately and may log events like BLOCKED)
    log_continued(curr_proc->pid, curr_proc->priority, curr_proc->name);

    // Continue the thread
    int continue_result = spthread_continue((curr_proc->thread));
    if (continue_result != 0) {
      s_fprintf_stderr( "ERROR: scheduler - failed to continue process %d (error %d)\n",
              curr_proc->pid, continue_result);
    }

    sigsuspend(&suspend_set);

    // Suspend the thread and only log if successful
    int suspend_result = spthread_suspend((curr_proc->thread));
    if (suspend_result == 0) {
      log_stopped(curr_proc->pid, curr_proc->priority, curr_proc->name);
    } else if (suspend_result != ESRCH) {
      // Only print error if it's not ESRCH (it can happen when thread exited before quantum ends)
      s_fprintf_stderr( "ERROR: scheduler - failed to suspend process %d (error %d)\n",
              curr_proc->pid, suspend_result);
    }

    // check if the state is still running and add back to queue
    if (curr_proc->state == RUNNING) {
      if (!scheduler_insert(curr_proc)) {
        s_fprintf_stderr( "ERROR: scheduler - failed to re-insert running process %d to queue\n",
                curr_proc->pid);
      }
    }
    // If not running, process changed state during execution (blocked, stopped, or exited)
    // no need to add to sheduler
  }
}