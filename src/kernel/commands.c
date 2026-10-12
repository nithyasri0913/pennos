#include "kernel/commands.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "kernel/init.h"
#include "kernel/kernel.h"
#include "kernel/logging.h"
#include "kernel/p_errno.h"
#include "kernel/pcb.h"
#include "kernel/scheduler.h"
#include "kernel/syscall.h"
#include "kernel/user_processes.h"
#include "utils/spthread.h"
#include "../FAT/s_functions.h"

// Process state (non-static so shell can access for Ctrl-C handling)
volatile pid_t foreground_pid = -1;     // Current foreground process
volatile bool sigint_received = false;  // Ctrl-C flag

// SIGINT handler for Ctrl-C
void sigint_handler(int sig) {
  sigint_received = true;

  // If there's a foreground process, send SIGTERM directly to it
  // This allows Ctrl-C to work even when shell is blocked in waitpid
  if (foreground_pid > 0) {
    s_kill(foreground_pid, P_SIGTERM);
    foreground_pid = -1;
  }
}

// Print help
void print_help(void) {
  s_printf("\n=== Kernel Shell Commands ===\n");
  s_printf("  spawn <type>        - Spawn a new process\n");
  s_printf(
      "                        Types: hello, count, sleep, zombie, zombify, "
      "orphanify, ps\n");
  s_printf("  zombify [&]         - Create parent with zombie child\n");
  s_printf("                        With &: runs in background\n");
  s_printf(
      "                        Without &: runs in foreground (Ctrl-C to "
      "kill)\n");
  s_printf("  orphanify           - Create orphan child (reparented to INIT)\n");
  s_printf(
      "  busy [priority] [&] - Run CPU-intensive process for benchmarking\n");
  s_printf(
      "                        priority: 0 (high), 1 (med), 2 (low) - defaults "
      "to 0\n");
  s_printf("                        &: run in background (optional)\n");
  s_printf("  waitpid <pid>       - Wait for a specific child (polls 20x)\n");
  s_printf("  waitany             - Wait for any child (polls 20x)\n");
  s_printf("  waitpid_blocking <pid> - Wait forever (Ctrl-C to interrupt)\n");
  s_printf("  waitany_blocking    - Wait for any child forever (Ctrl-C to interrupt)\n");
  s_printf("  kill <pid> <signal> - Send signal to process\n");
  s_printf("                        Signals: stop (0), cont (1), term (2)\n");
  s_printf(
      "  nice <pid> <prio>   - Set process priority (0=high, 1=med, 2=low)\n");
  s_printf("  fg [pid]            - Bring background job to foreground\n");
  s_printf(
      "                        pid: process ID (optional, defaults to most "
      "recent)\n");
  s_printf("  bg [pid]            - Resume stopped job in background\n");
  s_printf(
      "                        pid: process ID (optional, defaults to most "
      "recent)\n");
  s_printf("  jobs                - List all jobs\n");
  s_printf("  ps                  - List all processes\n");
  s_printf("  info <pid>          - Show detailed process info\n");
  s_printf("  help                - Show this help\n");
  s_printf("  exit / logout       - Exit shell and shutdown PennOS\n");
  s_printf("==============================\n\n");
}

// List all processes
void cmd_ps(void) {
  char name[64];
  snprintf(name, sizeof(name), "ps-%d", rand() % 1000);

  char* argv[] = {name, NULL};
  pid_t pid = s_spawn(ps_process, argv, -1, 1);

  if (pid <= 0) {
    s_printf("Failed to spawn ps process\n");
    return;
  }

  // Set as foreground process - shell loop will block and wait for it
  foreground_pid = pid;
}

// Show detailed process info
void cmd_info(pid_t pid) {
  pcb_t* proc = k_get_process(pid);
  if (proc == NULL) {
    s_printf("Error: Process %d not found\n", pid);
    return;
  }

  s_printf("\n=== Process %d Info ===\n", pid);
  s_printf("Name:          %s\n", proc->name);
  s_printf("Parent PID:    %d\n", proc->ppid);
  s_printf("Priority:      %d\n", proc->priority);
  s_printf("State:         ");
  switch (proc->state) {
    case RUNNING:
      s_printf("RUNNING\n");
      break;
    case BLOCKED:
      s_printf("BLOCKED\n");
      break;
    case STOPPED:
      s_printf("STOPPED\n");
      break;
    case ZOMBIE:
      s_printf("ZOMBIE\n");
      break;
  }
  s_printf("Exit Status:   %d (0x%x)\n", proc->exit_status, proc->exit_status);
  s_printf("Pending Signal: %d\n", proc->pending_signal);
  s_printf("Sleep Ticks:   %u\n", proc->sleep_ticks_remaining);

  s_printf("\nChildren (%d):\n", proc->num_children);
  for (int i = 0; i < proc->num_children; i++) {
    s_printf("  - PID %d\n", proc->children[i]);
  }

  s_printf("\nZombies (%d):\n", proc->num_zombies);
  for (int i = 0; i < proc->num_zombies; i++) {
    s_printf("  - PID %d (exit status: 0x%x)\n", proc->zombie_children[i],
           proc->exit_statuses[i]);
  }

  s_printf("\nStopped Children (%d):\n", proc->num_stopped);
  for (int i = 0; i < proc->num_stopped; i++) {
    s_printf("  - PID %d\n", proc->stopped_children[i]);
  }

  s_printf("Waiting (Ctrl-C to kill): %s\n", proc->is_waiting ? "Yes" : "No");
  if (proc->is_waiting) {
    s_printf("  Waiting for PID: %d\n", proc->waiting_for_pid);
  }

  s_printf("\n");
}

// Spawn a process
void cmd_spawn(const char* type) {
  void* (*func)(void*) = NULL;
  char name[64];

  if (strcmp(type, "hello") == 0) {
    func = hello_process;
    snprintf(name, sizeof(name), "hello-%d", rand() % 1000);
  } else if (strcmp(type, "count") == 0) {
    func = counting_process;
    snprintf(name, sizeof(name), "counter-%d", rand() % 1000);
  } else if (strcmp(type, "sleep") == 0) {
    func = user_sleep;
    snprintf(name, sizeof(name), "sleeper-%d", rand() % 1000);
  } else if (strcmp(type, "zombie") == 0) {
    func = zombie_process;
    snprintf(name, sizeof(name), "zombie-%d", rand() % 1000);
  } else if (strcmp(type, "zombify") == 0) {
    func = zombify_parent_process;
    snprintf(name, sizeof(name), "zombify-%d", rand() % 1000);
  } else if (strcmp(type, "orphanify") == 0) {
    func = orphanify;
    snprintf(name, sizeof(name), "orphanify-%d", rand() % 1000);
  } else if (strcmp(type, "ps") == 0) {
    func = ps_process;
    snprintf(name, sizeof(name), "ps-%d", rand() % 1000);
  } else {
    s_printf("Error: Unknown process type '%s'\n", type);
    s_printf(
        "Available types: hello, count, sleep, zombie, zombify, orphanify, "
        "ps\n");
    return;
  }

  char* argv[] = {name, NULL};
  pid_t pid = s_spawn(func, argv, -1, 1);
  s_nice(pid, 1);  // Medium priority
  if (pid > 0) {
    s_printf("Spawned process '%s' with PID %d\n", name, pid);
  } else {
    s_printf("Failed to spawn process\n");
  }
}

// Wait for a specific child
void cmd_waitpid(pid_t pid) {
  s_printf("Waiting for process %d...\n", pid);

  int status;
  pid_t result = -1;

  // Poll with timeout
  for (int i = 0; i < 20; i++) {
    result = s_waitpid(pid, &status, true);
    if (result != 0) {
      break;
    }
    spthread_suspend_self();
  }

  if (result == pid) {
    s_printf("Process %d exited", pid);
    if (P_WIFEXITED(status)) {
      s_printf(" normally with status %d\n", P_EXITSTATUS(status));
    } else if (P_WIFSTOPPED(status)) {
      s_printf(" (stopped)\n");
    } else if (P_WIFSIGNALED(status)) {
      s_printf(" (terminated by signal)\n");
    } else {
      s_printf(" (status: 0x%x)\n", status);
    }
  } else if (result == 0) {
    s_printf("Process %d has not exited yet (still waiting)\n", pid);
  } else {
    s_printf("Error waiting for process %d (result: %d)\n", pid, result);
  }
}

// BLOCKING wait for specific child (simulated with interruptible polling)
// NOTE: True blocking (nohang=false) cannot be interrupted by Ctrl-C because
// the thread is suspended. So we simulate blocking with long polling intervals
// while checking for SIGINT.
void cmd_waitpid_blocking(pid_t pid) {
  s_printf("BLOCKING wait for process %d (will wait until it exits)...\n", pid);
  s_printf("Press Ctrl-C to interrupt\n");

  int status;
  pid_t result = 0;

  // Reset the sigint flag
  sigint_received = false;

  // Poll indefinitely until child exits or Ctrl-C
  while (!sigint_received) {
    result = s_waitpid(pid, &status, true);  // nohang=true to poll

    if (result == pid) {
      // Child exited/stopped!
      s_printf("\nProcess %d exited", pid);
      if (P_WIFEXITED(status)) {
        s_printf(" normally with status %d\n", P_EXITSTATUS(status));
      } else if (P_WIFSTOPPED(status)) {
        s_printf(" (stopped)\n");
      } else if (P_WIFSIGNALED(status)) {
        s_printf(" (terminated by signal)\n");
      } else {
        s_printf(" (status: 0x%x)\n", status);
      }
      return;
    } else if (result == -1) {
      s_printf("Error waiting for process %d\n", pid);
      return;
    }

    // result == 0: child hasn't changed state yet, keep waiting
    spthread_suspend_self();  // Yield to scheduler
  }

  // Interrupted by Ctrl-C
  s_printf("\n^C (interrupted)\n");
}

// BLOCKING wait for any child (simulated with interruptible polling)
// NOTE: True blocking (nohang=false) cannot be interrupted by Ctrl-C because
// the thread is suspended. So we simulate blocking with long polling intervals
// while checking for SIGINT.
void cmd_waitany_blocking(void) {
  s_printf("BLOCKING wait for any child (will wait until one changes state)...\n");
  s_printf("Press Ctrl-C to interrupt\n");

  int status;
  pid_t result = 0;

  // Reset the sigint flag
  sigint_received = false;

  // Poll indefinitely until a child exits or Ctrl-C
  while (!sigint_received) {
    result = s_waitpid(-1, &status, true);  // nohang=true to poll

    if (result > 0) {
      // Child exited/stopped!
      s_printf("\nProcess %d changed state", result);
      if (P_WIFEXITED(status)) {
        s_printf(" (exited normally)\n");
      } else if (P_WIFSTOPPED(status)) {
        s_printf(" (stopped)\n");
      } else if (P_WIFSIGNALED(status)) {
        s_printf(" (terminated by signal)\n");
      } else {
        s_printf(" (status: 0x%x)\n", status);
      }
      return;
    } else if (result == -1) {
      s_printf("Error: No children to wait for\n");
      return;
    }

    // result == 0: no child has changed state yet, keep waiting
    spthread_suspend_self();  // Yield to scheduler
  }

  // Interrupted by Ctrl-C
  s_printf("\n^C (interrupted)\n");
}

// Wait for any child
void cmd_waitany(void) {
  s_printf("Waiting for any child process...\n");

  int status;
  pid_t result = -1;

  // Poll with timeout
  for (int i = 0; i < 20; i++) {
    result = s_waitpid(-1, &status, true);
    if (result != 0) {
      break;
    }
    spthread_suspend_self();
  }

  if (result > 0) {
    s_printf("Process %d changed state", result);
    if (P_WIFEXITED(status)) {
      s_printf(" (exited normally)\n");
    } else if (P_WIFSTOPPED(status)) {
      s_printf(" (stopped)\n");
    } else if (P_WIFSIGNALED(status)) {
      s_printf(" (terminated by signal)\n");
    } else {
      s_printf(" (status: 0x%x)\n", status);
    }
  } else if (result == 0) {
    s_printf("No child has changed state yet\n");
  } else {
    s_printf("Error: No children to wait for\n");
  }
}

// Send signal to process
void cmd_kill(pid_t pid, int signal) {
  const char* sig_name;
  switch (signal) {
    case P_SIGSTOP:
      sig_name = "SIGSTOP";
      break;
    case P_SIGCONT:
      sig_name = "SIGCONT";
      break;
    case P_SIGTERM:
      sig_name = "SIGTERM";
      break;
    default:
      s_printf("Error: Invalid signal %d (use 0=STOP, 1=CONT, 2=TERM)\n", signal);
      return;
  }

  s_printf("Sending %s to process %d...\n", sig_name, pid);

  int result = s_kill(pid, signal);
  if (result == 0) {
    s_printf("Signal sent successfully\n");
  } else {
    u_perror("kill");
  }
}

// Change process priority
void cmd_nice(pid_t pid, int priority) {
  if (priority < 0 || priority > 2) {
    s_printf("Error: Priority must be 0 (high), 1 (medium), or 2 (low)\n");
    return;
  }

  s_printf("Setting process %d priority to %d...\n", pid, priority);

  int result = s_nice(pid, priority);
  if (result == 0) {
    s_printf("Priority changed successfully\n");
  } else {
    u_perror("nice");
  }
}

// Zombify command - creates parent with zombie child
void cmd_zombify(bool background) {
  static char name[64];
  snprintf(name, sizeof(name), "zombify-parent-%d", rand() % 10);

  static char* argv_storage[2];
  argv_storage[0] = name;
  argv_storage[1] = NULL;

  pid_t pid = s_spawn(zombify_parent_process, argv_storage, -1, 1);

  if (pid <= 0) {
    s_printf("Failed to spawn zombify parent\n");
    return;
  }

  s_printf("Spawned zombify parent '%s' with PID %d\n", name, pid);

  if (background) {
    s_printf("Running in background (use 'ps' to see status)\n");
  } else {
    s_printf("Running in FOREGROUND (Press Ctrl-C to kill parent)\n");

    // Run in foreground - wait for process and handle Ctrl-C
    foreground_pid = pid;
    sigint_received = false;

    // Poll until process dies or Ctrl-C
    while (1) {
      // Check if Ctrl-C was pressed
      if (sigint_received) {
        s_printf("\n[Shell] Ctrl-C received! Sending SIGTERM to PID %d...\n",
               pid);
        s_kill(pid, P_SIGTERM);
        sigint_received = false;

        // Wait a bit for process to die
        spthread_suspend_self();

        // Reap the zombie
        int status;
        s_waitpid(pid, &status, true);
        break;
      }

      // Check if process is still alive
      pcb_t* proc = k_get_process(pid);
      if (proc == NULL || proc->state == ZOMBIE) {
        s_printf("\n[Shell] Process %d exited\n", pid);

        // Reap it
        int status;
        s_waitpid(pid, &status, true);
        break;
      }
      spthread_suspend_self();
    }

    foreground_pid = -1;
    s_printf("[Shell] Foreground process finished\n");
  }
}

// Orphanify command - creates a parent that spawns a child then exits
// immediately
void cmd_orphanify(void) {
  // Use static to ensure name persists for the spawned process
  // (s_spawn only stores pointer, doesn't copy argv strings)
  static char name[64];
  snprintf(name, sizeof(name), "orphanify-parent-%d", rand() % 1000);

  char* argv[] = {name, NULL};
  pid_t pid = s_spawn(orphanify, argv, -1, 1);

  if (pid <= 0) {
    s_printf("Failed to spawn orphanify parent\n");
    return;
  }

  s_printf("Spawned orphanify parent '%s' with PID %d\n", name, pid);
  s_printf("Parent will exit immediately, orphaning its child...\n");
  s_printf("INIT will automatically reap the orphan child when it exits.\n");

  // Wait for orphanify parent to finish (blocking wait sets is_waiting flag
  // so k_notify_parent will wake us up when child exits)
  int status;
  s_waitpid(pid, &status, false);  // false = blocking wait

  s_printf("Orphanify parent exited. Orphan child is now owned by INIT.\n");
}

// Foreground command - bring a background process to foreground
void cmd_fg(pid_t pid) {
  pcb_t* shell = k_get_current_process();
  pcb_t* proc = NULL;

  // If no PID specified, find the most recent background job
  if (pid <= 0) {
    // Look through shell's children for background processes
    // Start from the end to get the most recent one
    for (int i = shell->num_children - 1; i >= 0; i--) {
      pcb_t* child = k_get_process(shell->children[i]);
      if (child != NULL && child->is_background && child->state != ZOMBIE) {
        proc = child;
        pid = child->pid;
        break;
      }
    }

    if (proc == NULL) {
      s_printf("fg: current: no such job\n");
      return;
    }
  } else {
    // PID was specified, validate it
    proc = k_get_process(pid);

    if (proc == NULL) {
      s_printf("fg: %d: no such job\n", pid);
      return;
    }

    // Check if the process is a child of the shell
    bool is_child = false;
    for (int i = 0; i < shell->num_children; i++) {
      if (shell->children[i] == pid) {
        is_child = true;
        break;
      }
    }

    if (!is_child) {
      s_printf("fg: %d: not a child of this shell\n", pid);
      return;
    }

    // Check if process is already terminated (zombie)
    if (proc->state == ZOMBIE) {
      s_printf("fg: %d: process has already terminated\n", pid);
      return;
    }
  }

  s_printf("[%d] %s\n", pid, proc->name);

  // If the process is stopped, send SIGCONT to resume it
  if (proc->state == STOPPED) {
    s_kill(pid, P_SIGCONT);
  }

  // Mark process as no longer background
  proc->is_background = false;

  // Set as foreground process and wait for it
  foreground_pid = pid;
  int status;
  s_waitpid(pid, &status, false);  // Blocking wait

  // Clear foreground process
  foreground_pid = -1;

  // Print status if process exited
  if (P_WIFEXITED(status)) {
    s_printf("\n[%d] Done\n", pid);
  } else if (P_WIFSIGNALED(status)) {
    s_printf("\n[%d] Terminated\n", pid);
  }
}

// Background command - resume a stopped job in the background
void cmd_bg(pid_t pid) {
  pcb_t* shell = k_get_current_process();
  pcb_t* proc = NULL;

  // If no PID specified, find the most recent stopped job
  if (pid <= 0) {
    // Look through shell's stopped children
    // Start from the end to get the most recent one
    for (int i = shell->num_stopped - 1; i >= 0; i--) {
      pcb_t* child = k_get_process(shell->stopped_children[i]);
      if (child != NULL && child->state == STOPPED) {
        proc = child;
        pid = child->pid;
        break;
      }
    }

    if (proc == NULL) {
      s_printf("bg: current: no stopped jobs\n");
      return;
    }
  } else {
    // PID was specified, validate it
    proc = k_get_process(pid);

    if (proc == NULL) {
      s_printf("bg: %d: no such job\n", pid);
      return;
    }

    // Check if the process is a child of the shell
    bool is_child = false;
    for (int i = 0; i < shell->num_children; i++) {
      if (shell->children[i] == pid) {
        is_child = true;
        break;
      }
    }

    if (!is_child) {
      s_printf("bg: %d: not a child of this shell\n", pid);
      return;
    }

    // Check if process is actually stopped
    if (proc->state != STOPPED) {
      s_printf("bg: %d: job is not stopped\n", pid);
      return;
    }
  }

  s_printf("[%d] %s &\n", pid, proc->name);

  // Mark process as background
  proc->is_background = true;

  // Send SIGCONT to resume the stopped process
  s_kill(pid, P_SIGCONT);
}

/**
 * @brief Exits the shell and shuts down PennOS.
 *
 * Example Usage: logout
 */
void cmd_logout(void) {
  s_printf("Logging out...\n");
  init_shutdown();
}

/**
 * @brief Lists all jobs.
 *
 * Example Usage: jobs
 */
void cmd_jobs(void) {
  pcb_t* shell = k_get_current_process();
  if (shell == NULL) {
    s_fprintf_stderr("jobs: cannot get current process\n");
    return;
  }

  if (shell->num_children == 0) {
    // No jobs to display
    return;
  }

  int job_num = 1;
  for (int i = 0; i < shell->num_children; i++) {
    pid_t child_pid = shell->children[i];
    pcb_t* child = k_get_process(child_pid);

    if (child == NULL) {
      continue;  // Child was reaped
    }

    const char* state_str;
    switch (child->state) {
      case RUNNING:
        state_str = "Running";
        break;
      case BLOCKED:
        state_str = "Blocked";  
        break;
      case STOPPED:
        state_str = "Stopped";
        break;
      case ZOMBIE:
        state_str = "Zombie";
        break;
      default:
        state_str = " ";
        break;
    }

    
    s_printf("[%d] %s %s", job_num, state_str, child->name);

    // For background jobs, append &
    if (child->is_background) {
      s_printf(" &");
    }

    s_printf("\n");
    job_num++;
  }
}
