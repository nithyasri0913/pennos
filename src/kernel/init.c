#include "init.h"
#include <stdio.h>
#include <stdlib.h>
#include "kernel/kernel.h"
#include "kernel/logging.h"
#include "kernel/pcb.h"
#include "kernel/scheduler.h"
#include "kernel/shell.h"
#include "kernel/syscall.h"
#include "../FAT/s_functions.h"

static volatile bool system_running = true;

void init_shutdown(void) {
  system_running = false;
}
// INIT process - spawns shell and cleans up when shell exits
void* init_main(void* arg) {
  pcb_t* init_process = (pcb_t*)arg;

  s_printf("[INIT] Starting INIT process (PID 1)...\n");
  // Set INIT as current process for this thread
  k_set_current_process(init_process);

  // Spawn the shell process
  s_printf("[INIT] Spawning shell process...\n");
  char* shell_argv[] = {"shell", NULL};
  pid_t shell_pid = s_spawn(shell_main, shell_argv, -1, 0);

  if (shell_pid <= 0) {
    s_fprintf_stderr("[INIT] Failed to spawn shell\n");
    system_running = false;
    scheduler_stop();
    return NULL;
  }

  // Set shell priority to 0 (high priority, same as user processes by default)
  s_nice(shell_pid, 0);

  s_printf("[INIT] Shell spawned with PID %d (priority 0)\n", shell_pid);
  s_printf("[INIT] Waiting for children (blocking)...\n\n");

  // INIT waits for ANY child (-1), but only exits when shell dies
  // This allows INIT to reap orphaned zombies while still blocking
  while (system_running) {
    int status;
    pid_t reaped =
        s_waitpid(-1, &status, false);  // Blocking wait for any child

    if (reaped > 0) {
      if (reaped == shell_pid) {
        // Shell exited - time to shut down
        s_printf("\n[INIT] Shell (PID %d) exited, shutting down...\n", reaped);
        system_running = false;
        break;
      } else {
        // Orphaned zombie reaped
        s_printf("[INIT] Reaped orphan zombie PID %d", reaped);
        if (P_WIFEXITED(status)) {
          s_printf(" (exited with status %d)\n", P_EXITSTATUS(status));
        } else if (P_WIFSIGNALED(status)) {
          s_printf(" (terminated by signal)\n");
        } else {
          s_printf(" (status: 0x%x)\n", status);
        }
        // Continue waiting for more children
      }
    } else if (reaped == -1) {
      // No children - shouldn't happen unless shell died unexpectedly
      s_printf("[INIT] No children to wait for, exiting...\n");
      system_running = false;
      break;
    }
  }

  // Final cleanup - reap any remaining zombies (non-blocking)
  s_printf("[INIT] Final cleanup...\n");
  int zombies_reaped = 0;
  while (1) {
    int child_status;
    pid_t reaped = s_waitpid(-1, &child_status, true);  // Non-blocking

    if (reaped > 0) {
      zombies_reaped++;
      s_printf("[INIT] Reaped remaining zombie PID %d\n", reaped);
    } else {
      break;
    }
  }

  if (zombies_reaped > 0) {
    s_printf("[INIT] Cleaned up %d remaining zombie(s)\n", zombies_reaped);
  }

  s_printf("[INIT] INIT process exiting, shutting down system...\n");
  scheduler_stop();
  return NULL;
}
