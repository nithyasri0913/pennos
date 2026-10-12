#include "syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "logging.h"
#include "p_errno.h"
#include "pcb.h"
#include "scheduler.h"
#include "utils/spthread.h"
#include "../FAT/s_functions.h"
/**
 * @brief Wrapper structure for passing arguments to spawned thread
 */
typedef struct {
  void* (*func)(void*);
  char** argv;
  pid_t pid;
} spawn_args_t;

/**
 * @brief Wrapper function for spawned threads
 * Sets the current process and executes the user function
 */
static void* spawn_wrapper(void* arg) {
  spawn_args_t* spawn_args = (spawn_args_t*)arg;

  // Get the PCB for this process
  pcb_t* proc = k_get_process(spawn_args->pid);
  if (proc == NULL) {
    s_fprintf_stderr("ERROR: spawn_wrapper could not find process %d\n",
            spawn_args->pid);
    free(spawn_args);
    return NULL;
  }

  // Set this as the current process
  k_set_current_process(proc);

  // Execute the user function
  void* result = spawn_args->func((void*)spawn_args->argv);

  // Clean up
  free(spawn_args);

  // Process is returning, call s_exit
  s_exit();

  return result;
}

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as
 * argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The process ID of the created child process, or -1 on error.
 */
pid_t s_spawn(void* (*func)(void*), char* argv[], int fd0, int fd1) {
  if (func == NULL) {
    set_p_errno(P_EINVAL);  // Invalid argument
    return -1;
  }

  // Disable interrupts for critical section
  spthread_disable_interrupts_self();

  // Get current process (parent)
  pcb_t* parent = k_get_current_process();

  // Create child PCB
  pcb_t* child = k_proc_create(parent);
  if (child == NULL) {
    // Creation failed
    set_p_errno(P_EAGAIN);  // Resource temporarily unavailable
    spthread_enable_interrupts_self();
    return -1;
  }

  // Set the process name from argv[0] if available
  if (argv != NULL && argv[0] != NULL) {
    strncpy(child->name, argv[0], sizeof(child->name) - 1);
    child->name[sizeof(child->name) - 1] = '\0';
  }

  // Log process creation (after name is set)
  log_create(child->pid, child->priority, child->name);

  // Override fd0 and fd1 if specified (>= 0)
  // If fd is -1, clear the inherited fd_table entry (no redirection wanted)

  if (fd0 >= 0) {
    // Set stdin
    child->fd_table[0] = fd0;
  } else {
    // Clear inherited stdin - no redirection
    child->fd_table[0] = -1;
  }

  if (fd1 >= 0) {
    // Set stdout
    child->fd_table[1] = fd1;
  } else {
    // Clear inherited stdout - no redirection
    child->fd_table[1] = -1;
  }


  // Prepare spawn arguments
  spawn_args_t* spawn_args = (spawn_args_t*)malloc(sizeof(spawn_args_t));
  if (spawn_args == NULL) {
    k_proc_cleanup(child);
    set_p_errno(P_ENOMEM);  // Out of memory
    spthread_enable_interrupts_self();
    return -1;
  }

  spawn_args->func = func;
  spawn_args->argv = argv;
  spawn_args->pid = child->pid;

  // Create the thread
  if (spthread_create(&child->thread, NULL, spawn_wrapper, spawn_args) != 0) {
    free(spawn_args);
    k_proc_cleanup(child);
    set_p_errno(P_EAGAIN);  // Resource temporarily unavailable
    spthread_enable_interrupts_self();
    return -1;
  }

  // Start the thread (spthread_create creates threads in suspended state)
  if (!scheduler_insert(child)) {
    free(spawn_args);
    k_proc_cleanup(child);
    set_p_errno(P_EAGAIN);  // Resource temporarily unavailable
    spthread_enable_interrupts_self();
    return -1;
  }

  pid_t child_pid = child->pid;

  set_p_errno(P_SUCCESS);  // Success
  spthread_enable_interrupts_self();

  return child_pid;
}

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return
 * immediately.
 *
 * @param pid Process ID of the child to wait for (-1 for any child).
 * @param wstatus Pointer to an integer variable where the status will be
 * stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success,
 * -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
  spthread_disable_interrupts_self();

  pcb_t* parent = k_get_current_process();
  if (parent == NULL) {
    set_p_errno(P_ESRCH);  // No such process (internal error)
    spthread_enable_interrupts_self();
    return -1;
  }

  // Check if there are any children to wait for
  if (pid == -1) {
    // Waiting for any child
    if (parent->num_children == 0 && parent->num_zombies == 0) {
      set_p_errno(P_ECHILD);  // No child processes
      spthread_enable_interrupts_self();
      return -1;
    }
  } else {
    // Waiting for specific child - verify it's actually our child
    bool found = false;
    for (int i = 0; i < parent->num_children; i++) {
      if (parent->children[i] == pid) {
        found = true;
        break;
      }
    }
    for (int i = 0; i < parent->num_zombies && !found; i++) {
      if (parent->zombie_children[i] == pid) {
        found = true;
        break;
      }
    }
    if (!found) {
      set_p_errno(P_ECHILD);  // No child processes (not our child)
      spthread_enable_interrupts_self();
      return -1;
    }
  }

  // First, check for zombie children
  int exit_status;
  pid_t zombie_pid = pcb_get_zombie(parent, pid, &exit_status);
  if (zombie_pid != -1) {
    // Found a zombie child
    if (wstatus != NULL) {
      *wstatus = exit_status;
    }

    // Remove from children list
    pcb_remove_child(parent, zombie_pid);

    // Log WAITED event before cleanup
    pcb_t* zombie = k_get_process(zombie_pid);
    if (zombie != NULL) {
      log_waited(zombie->pid, zombie->priority, zombie->name);
      k_proc_cleanup(zombie);
    }

    set_p_errno(P_SUCCESS);  // Success
    spthread_enable_interrupts_self();
    return zombie_pid;
  }

  // Check for stopped children
  if (pid == -1 && parent->num_stopped > 0) {
    // Return first stopped child
    pid_t stopped_pid = parent->stopped_children[0];
    if (wstatus != NULL) {
      *wstatus = 0x7F;  // Stopped status
    }

    // Don't remove from stopped list or children list - it's still alive
    set_p_errno(P_SUCCESS);  // Success
    spthread_enable_interrupts_self();
    return stopped_pid;
  } else if (pid != -1) {
    // Check if specific child is stopped
    for (int i = 0; i < parent->num_stopped; i++) {
      if (parent->stopped_children[i] == pid) {
        if (wstatus != NULL) {
          *wstatus = 0x7F;  // Stopped status
        }
        set_p_errno(P_SUCCESS);  // Success
        spthread_enable_interrupts_self();
        return pid;
      }
    }
  }

  // No zombie or stopped children available
  if (nohang) {
    // Don't block, return 0
    set_p_errno(P_SUCCESS);  // Success (no child ready, but not an error)
    spthread_enable_interrupts_self();
    return 0;
  }

  // Block until a child changes state
  parent->is_waiting = true;
  parent->waiting_for_pid = pid;
  parent->state = BLOCKED;
  log_blocked(parent->pid, parent->priority, parent->name);

  spthread_enable_interrupts_self();

  // Yield to scheduler - will be woken up when child changes state
  // The scheduler should not schedule BLOCKED processes
  while (parent->state == BLOCKED) {
    // Wait for notification from child
    spthread_suspend(parent->thread);
  }

  // When we wake up, try again
  spthread_disable_interrupts_self();
  parent->is_waiting = false;
  // Note: UNBLOCKED is logged in k_notify_parent when state changes
  spthread_enable_interrupts_self();

  // Recursively call to get the zombie/stopped child
  return s_waitpid(pid, wstatus, true);
}

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target process.
 * @param signal Signal number to be sent (P_SIGSTOP, P_SIGCONT, P_SIGTERM).
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal) {
  spthread_disable_interrupts_self();

  // Get the target process
  pcb_t* target = k_get_process(pid);
  if (target == NULL) {
    set_p_errno(P_ESRCH);  // No such process
    spthread_enable_interrupts_self();
    return -1;
  }

  // Validate signal
  if (signal != P_SIGSTOP && signal != P_SIGCONT && signal != P_SIGTERM) {
    set_p_errno(P_EINVAL);  // Invalid argument
    spthread_enable_interrupts_self();
    return -1;
  }

  // Set pending signal
  target->pending_signal = signal;

  // Handle the signal immediately
  k_handle_pending_signals(target);

  spthread_enable_interrupts_self();

  // Success - clear errno
  set_p_errno(P_SUCCESS);
  return 0;
}

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void) {
  spthread_disable_interrupts_self();

  pcb_t* proc = k_get_current_process();
  if (proc == NULL) {
    spthread_enable_interrupts_self();
    s_fprintf_stderr("ERROR: s_exit called with no current process\n");
    return;
  }

  // Check if process was already marked as zombie (e.g., by SIGTERM)
  bool already_zombie = (proc->state == ZOMBIE);

  // Get parent process
  pcb_t* parent = k_get_process(proc->ppid);

  if (!already_zombie) {
    // Set exit status (normal exit)
    proc->exit_status = 0;  // Exit code 0, normal termination
    proc->state = ZOMBIE;

    // Log EXITED and ZOMBIE events
    log_exited(proc->pid, proc->priority, proc->name);
    log_zombie(proc->pid, proc->priority, proc->name);

    // Add to parent's zombie queue
    if (parent != NULL) {
      pcb_add_zombie(parent, proc->pid, proc->exit_status);

      // Notify parent if it's waiting
      k_notify_parent(proc->pid, parent);
    }
  }

  // Reparent any children to INIT (if not already done by signal handler)
  // Note: k_reparent_children_to_init clears children lists, so calling twice
  // is safe
  // BUT: Don't reparent if this IS the INIT process (would create duplicates)
  if ((proc->num_children > 0 || proc->num_zombies > 0) &&
      proc->pid != INIT_PID) {
    k_reparent_children_to_init(proc->pid);
  }

  spthread_enable_interrupts_self();

  // Exit the thread
  spthread_exit(NULL);
}

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priority value of the thread (0, 1, or 2).
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority) {
  if (priority < 0 || priority > 2) {
    set_p_errno(P_EINVAL);  // Invalid argument
    return -1;
  }

  spthread_disable_interrupts_self();

  // Get the target process
  pcb_t* target = k_get_process(pid);
  if (target == NULL) {
    set_p_errno(P_ESRCH);  // No such process
    spthread_enable_interrupts_self();
    return -1;
  }

  // Save old priority for logging
  int old_priority = target->priority;

  // Update priority
  target->priority = priority;

  // Log NICE event
  log_nice(target->pid, old_priority, priority, target->name);

  // Note: The scheduler should be notified to move this process
  // to the appropriate priority queue. This will be handled when
  // integrating with the scheduler.

  set_p_errno(P_SUCCESS);  // Success
  spthread_enable_interrupts_self();

  return 0;
}

/**
 * @brief Suspends execution of the calling process for a specified number of
 * clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the
 * system clock continues to tick even if the call is interrupted. The sleep can
 * be interrupted by a P_SIGTERM signal, after which the function will return
 * prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater
 * than 0.
 */
void s_sleep(unsigned int ticks) {
  if (ticks == 0) {
    set_p_errno(P_SUCCESS);  // Success (no-op)
    return;
  }

  spthread_disable_interrupts_self();

  pcb_t* proc = k_get_current_process();
  if (proc == NULL) {
    set_p_errno(P_ESRCH);  // No such process (internal error)
    spthread_enable_interrupts_self();
    return;
  }

  // Set sleep ticks and block the process
  proc->sleep_ticks_remaining = ticks;
  proc->state = BLOCKED;
  log_blocked(proc->pid, proc->priority, proc->name);

  spthread_enable_interrupts_self();

  // Yield to scheduler until sleep is done or interrupted
  while (proc->state == BLOCKED && proc->sleep_ticks_remaining > 0) {
    // The scheduler will decrement sleep_ticks_remaining each tick
    // and set state back to RUNNING when done
    spthread_suspend(proc->thread);
  }

  // If we were terminated, the state will be ZOMBIE
  // Otherwise, we woke up normally
  spthread_disable_interrupts_self();

  if (proc->state == BLOCKED) {
    // Sleep completed normally
    proc->state = RUNNING;
    proc->sleep_ticks_remaining = 0;
    log_unblocked(proc->pid, proc->priority, proc->name);
  }

  set_p_errno(P_SUCCESS);  // Success
  spthread_enable_interrupts_self();
}
