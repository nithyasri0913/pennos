#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "p_errno.h"
#include "scheduler.h"
#include "utils/spthread.h"
#include "../FAT/s_functions.h"

// Global process table
static pcb_t* process_table[MAX_PROCESSES];

// Current running process (thread-local so each thread has its own current
// process)
static __thread pcb_t* current_process = NULL;

// INIT process
static pcb_t* init_process = NULL;

// PID allocation counter
static pid_t next_pid = 0;

// Kernel initialization flag
static bool kernel_initialized = false;

/**
 * @brief Initialize the kernel and process table
 */
void k_init(void) {
  if (kernel_initialized) {
    s_fprintf_stderr( "WARNING: Kernel already initialized\n");
    return;
  }

  // Initialize process table to NULL
  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_table[i] = NULL;
  }

  // Start PID counter at 0 (INIT will be 1)
  next_pid = 0;

  // Reset current process
  current_process = NULL;
  init_process = NULL;

  kernel_initialized = true;
}

/**
 * @brief Shutdown the kernel and clean up all resources
 */
void k_shutdown(void) {
  if (!kernel_initialized) {
    return;
  }

  // Clean up all remaining processes
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i] != NULL) {
      pcb_destroy(process_table[i]);
      process_table[i] = NULL;
    }
  }

  current_process = NULL;
  init_process = NULL;
  kernel_initialized = false;
}

/**
 * @brief Allocate a new unique PID
 *
 * @return New PID, or -1 if no PIDs available
 */
pid_t k_alloc_pid(void) {
  if (!kernel_initialized) {
    s_fprintf_stderr( "ERROR: Kernel not initialized\n");
    return -1;
  }

  // Simple linear search for available PID
  pid_t start_pid = next_pid;

  do {
    next_pid = (next_pid + 1) % MAX_PROCESSES;

    // Check if this PID is available
    if (process_table[next_pid] == NULL) {
      return next_pid;
    }
  } while (next_pid != start_pid);

  // No available PIDs
  s_fprintf_stderr( "ERROR: No available PIDs\n");
  return -1;
}

/**
 * @brief Add a process to the process table
 *
 * @param proc Process to add
 * @return 0 on success, -1 on failure
 */
int k_add_process(pcb_t* proc) {
  if (!kernel_initialized) {
    s_fprintf_stderr( "ERROR: Kernel not initialized\n");
    return -1;
  }

  if (proc == NULL) {
    s_fprintf_stderr( "ERROR: Cannot add NULL process\n");
    return -1;
  }

  if (proc->pid < 0 || proc->pid >= MAX_PROCESSES) {
    s_fprintf_stderr( "ERROR: Invalid PID %d\n", proc->pid);
    return -1;
  }

  if (process_table[proc->pid] != NULL) {
    s_fprintf_stderr( "ERROR: PID %d already in use\n", proc->pid);
    return -1;
  }

  process_table[proc->pid] = proc;

  // If this is PID 1, set as INIT process
  if (proc->pid == INIT_PID) {
    init_process = proc;
  }

  return 0;
}

/**
 * @brief Remove a process from the process table
 *
 * @param pid Process ID to remove
 */
void k_remove_process(pid_t pid) {
  if (!kernel_initialized) {
    return;
  }

  if (pid < 0 || pid >= MAX_PROCESSES) {
    return;
  }

  process_table[pid] = NULL;
}

/**
 * @brief Get a process by PID
 *
 * @param pid Process ID to look up
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* k_get_process(pid_t pid) {
  if (!kernel_initialized) {
    return NULL;
  }

  if (pid < 0 || pid >= MAX_PROCESSES) {
    return NULL;
  }

  return process_table[pid];
}

/**
 * @brief Get the currently running process
 *
 * @return Pointer to the current process's PCB
 */
pcb_t* k_get_current_process(void) {
  return current_process;
}

/**
 * @brief Set the currently running process
 *
 * @param proc Process to set as current
 */
void k_set_current_process(pcb_t* proc) {
  current_process = proc;
}

/**
 * @brief Get the INIT process
 *
 * @return Pointer to INIT process's PCB
 */
pcb_t* k_get_init_process(void) {
  return init_process;
}

/**
 * @brief Create a new child process, inheriting applicable properties from the
 * parent
 *
 * @param parent Parent PCB (can be NULL for INIT process)
 * @return Pointer to the child PCB, or NULL on failure
 */
pcb_t* k_proc_create(pcb_t* parent) {
  if (!kernel_initialized) {
    s_fprintf_stderr( "ERROR: Kernel not initialized\n");
    return NULL;
  }

  // Allocate a new PID
  pid_t new_pid = k_alloc_pid();
  if (new_pid == -1) {
    return NULL;
  }

  // Determine parent PID and priority
  pid_t parent_pid = (parent != NULL) ? parent->pid : 0;
  int priority = 1;  // Child threads created by k_proc_create should have a
                     // default priority level of 1

  // Create the PCB
  pcb_t* child = pcb_create(new_pid, parent_pid, "", priority);
  if (child == NULL) {
    return NULL;
  }

  // Inherit file descriptors from parent
  if (parent != NULL) {
    for (int i = 0; i < MAX_FDS; i++) {
      child->fd_table[i] = parent->fd_table[i];
    }
  }

  // Add to process table
  if (k_add_process(child) != 0) {
    pcb_destroy(child);
    return NULL;
  }

  // Add child to parent's children list
  if (parent != NULL) {
    pcb_add_child(parent, new_pid);
  }

  return child;
}

/**
 * @brief Reparent children to INIT process
 *
 * Called when a parent process exits with living children.
 *
 * @param parent_pid PID of the parent that is exiting
 */
void k_reparent_children_to_init(pid_t parent_pid) {
  if (!kernel_initialized || init_process == NULL) {
    return;
  }

  pcb_t* parent = k_get_process(parent_pid);
  if (parent == NULL) {
    return;
  }

  // Reparent all living children
  for (int i = 0; i < parent->num_children; i++) {
    pid_t child_pid = parent->children[i];
    pcb_t* child = k_get_process(child_pid);

    if (child != NULL) {
      // Skip zombie children - they will be handled in the zombie loop below
      if (child->state == ZOMBIE) {
        continue;
      }

      // Change child's parent to INIT
      child->ppid = INIT_PID;

      // Log ORPHAN event
      log_orphan(child->pid, child->priority, child->name);

      // Add to INIT's children list
      pcb_add_child(init_process, child_pid);
    }
  }

  // Reparent all zombie children
  for (int i = 0; i < parent->num_zombies; i++) {
    pid_t zombie_pid = parent->zombie_children[i];
    int exit_status = parent->exit_statuses[i];

    // Log ORPHAN event for zombie children too
    pcb_t* zombie = k_get_process(zombie_pid);
    if (zombie != NULL) {
      log_orphan(zombie->pid, zombie->priority, zombie->name);
    }

    // Add to INIT's zombie queue
    pcb_add_zombie(init_process, zombie_pid, exit_status);
  }

  // Notify INIT if it's waiting for children
  if (parent->num_zombies > 0) {
    k_notify_parent(-1, init_process);
  }

  // Clear parent's children lists
  parent->num_children = 0;
  parent->num_zombies = 0;
}

/**
 * @brief Notify parent that a child has changed state
 *
 * This is called when a child process:
 * - Exits (becomes zombie)
 * - Stops (P_SIGSTOP)
 * - Continues (P_SIGCONT)
 *
 * @param child_pid Child process ID
 * @param parent Parent PCB
 */
void k_notify_parent(pid_t child_pid, pcb_t* parent) {
  if (parent == NULL) {
    return;
  }

  // If parent is waiting on this child (or any child), wake it up
  if (parent->is_waiting) {
    if (parent->waiting_for_pid == -1 || parent->waiting_for_pid == child_pid) {
      parent->is_waiting = false;

      // Change parent state from BLOCKED to RUNNING
      if (parent->state == BLOCKED) {
        parent->state = RUNNING;
        log_unblocked(parent->pid, parent->priority, parent->name);

        // Add parent back to scheduler queue so it can be scheduled again
        // Parent was removed from queue when it blocked, so this won't create
        // duplicates
        scheduler_insert(parent);
      }
    }
  }
}

/**
 * @brief Clean up a terminated/finished process's resources
 *
 * This handles:
 * - Reparenting children to INIT
 * - Notifying parent of termination
 * - Removing from parent's children list
 * - Removing from process table
 * - Freeing the PCB
 *
 * @param proc Process to clean up
 */
void k_proc_cleanup(pcb_t* proc) {
  if (!kernel_initialized || proc == NULL) {
    return;
  }

  pid_t pid = proc->pid;
  pid_t ppid = proc->ppid;

  // Reparent any living or zombie children to INIT
  if (proc->num_children > 0 || proc->num_zombies > 0) {
    k_reparent_children_to_init(pid);
  }

  // Get parent process
  pcb_t* parent = k_get_process(ppid);

  // Remove from parent's children list
  if (parent != NULL) {
    pcb_remove_child(parent, pid);
  }

  // Remove from scheduler queue (important: do this before canceling thread)
  scheduler_remove(proc);

  // Remove from process table
  k_remove_process(pid);

  // Cancel and join the thread if it exists (to prevent segfault from running
  // threads)
  if (proc->thread_created) {
    // Only cancel/join if this is not the current thread
    pcb_t* current = k_get_current_process();
    if (current == NULL || current->pid != proc->pid) {
      spthread_cancel(proc->thread);
      spthread_continue(proc->thread);
      spthread_suspend(proc->thread);  // Force thread to hit cancellation point
      spthread_join(proc->thread, NULL);
    }
    // If cleaning up current thread, it will exit naturally via s_exit
  }

  // Destroy the PCB
  pcb_destroy(proc);
}

/**
 * @brief Handle pending signals for a process
 *
 * Should be called by the scheduler before context switching to check
 * if the process has any pending signals that need to be handled.
 *
 * @param proc Process to check
 */
void k_handle_pending_signals(pcb_t* proc) {
  if (proc == NULL || proc->pending_signal == -1) {
    return;
  }

  pcb_t* parent = k_get_process(proc->ppid);

  switch (proc->pending_signal) {
    case P_SIGSTOP:
      if (proc->state != STOPPED) {
        proc->state = STOPPED;
        log_stopped(proc->pid, proc->priority, proc->name);

        // Remove from scheduler queue
        scheduler_remove(proc);

        // Add to parent's stopped children list
        if (parent != NULL) {
          pcb_add_stopped_child(parent, proc->pid);
        }

        // Notify parent
        k_notify_parent(proc->pid, parent);
      }
      break;

    case P_SIGCONT:
      if (proc->state == STOPPED) {
        proc->state = RUNNING;
        log_continued(proc->pid, proc->priority, proc->name);
        // Remove from parent's stopped children list
        if (parent != NULL) {
          pcb_remove_stopped_child(parent, proc->pid);
        }

        // Add process back to scheduler queue
        scheduler_insert(proc);

        // Notify parent
        k_notify_parent(proc->pid, parent);
      }
      break;

    case P_SIGTERM:
      // Mark process as terminated by signal
      proc->exit_status = 0x7F;  // Terminated by signal
      proc->state = ZOMBIE;
      log_signaled(proc->pid, proc->priority, proc->name);
      log_zombie(proc->pid, proc->priority, proc->name);
      // Reparent any children to INIT before adding to parent's zombie queue
      // BUT: Don't reparent if this IS the INIT process (would create duplicates)
      if ((proc->num_children > 0 || proc->num_zombies > 0) &&
          proc->pid != INIT_PID) {
        k_reparent_children_to_init(proc->pid);
      }

      // Add to parent's zombie queue
      if (parent != NULL) {
        pcb_add_zombie(parent, proc->pid, proc->exit_status);
      }

      // Notify parent
      k_notify_parent(proc->pid, parent);
      break;

    default:
      s_fprintf_stderr( "WARNING: Unknown signal %d for process %d\n",
              proc->pending_signal, proc->pid);
      break;
  }

  // Clear the pending signal
  proc->pending_signal = -1;
}

/**
 * @brief Get total number of active processes
 *
 * @return Number of processes in the system (excluding zombies)
 */
int k_get_process_count(void) {
  if (!kernel_initialized) {
    return 0;
  }

  int count = 0;
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i] != NULL && process_table[i]->state != ZOMBIE) {
      count++;
    }
  }

  return count;
}

/**
 * @brief To give scheduler access to process table
 *
 * @return pcb_t** ptr to process table
 */
pcb_t** k_get_process_table(void) {
  if (!kernel_initialized) {
    return NULL;
  }
  return process_table;
}

/**
 * @brief Get the error number for the current process
 *
 * This function returns the p_errno value for the currently running process.
 * If there is no current process, it returns 0 (P_SUCCESS).
 *
 * @return Current process's p_errno value, or 0 if no current process
 */
int get_p_errno(void) {
  pcb_t* proc = k_get_current_process();
  if (proc == NULL) {
    return 0;  // No current process, return success
  }
  return proc->p_errno;
}

/**
 * @brief Set the error number for the current process
 *
 * This function sets the p_errno value for the currently running process.
 * System calls should use this to indicate what error occurred.
 * If there is no current process, this function does nothing.
 *
 * @param err Error code to set (one of the P_E* constants from p_errno.h)
 */
void set_p_errno(int err) {
  pcb_t* proc = k_get_current_process();
  if (proc != NULL) {
    proc->p_errno = err;
  }
}
