#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FAT/commands.h"
#include "FAT/s_functions.h"
#include "kernel/init.h"
#include "kernel/kernel.h"
#include "kernel/logging.h"
#include "kernel/pcb.h"
#include "kernel/scheduler.h"

static pcb_t* init_process = NULL;

int main(int argc, char* argv[]) {
  // Check for filesystem argument
  const char* fs_name = NULL;
  if (argc > 1) {
    fs_name = argv[1];
  }
  s_printf("╔════════════════════════════════════════╗\n");
  s_printf("║      PennOS Kernel Booting...         ║\n");
  s_printf("╚════════════════════════════════════════╝\n\n");

  s_printf("[BOOT] Initializing logger...\n");
  logger_init("log/log");

  // Initialize kernel
  s_printf("[BOOT] Initializing kernel...\n");
  k_init();

  // Mount filesystem if provided
  if (fs_name != NULL) {
    s_printf("[BOOT] Mounting filesystem: %s\n", fs_name);
    if (mount(fs_name) != 0) {
      s_fprintf_stderr("[BOOT] Warning: Failed to mount filesystem '%s'\n",
              fs_name);
    }
  } else {
    s_printf("[BOOT] No filesystem specified (use: pennos <filesystem>)\n");
  }

  s_printf("[BOOT] Initializing scheduler...\n");
  scheduler_init();

  // Create INIT process (PID 1)
  s_printf("[BOOT] Creating INIT process (PID 1)...\n");
  init_process = pcb_create(1, 0, "init", 0);
  if (init_process == NULL) {
    s_fprintf_stderr("[BOOT] Failed to create INIT process\n");
    logger_close();
    return 1;
  }

  k_add_process(init_process);

  // Log INIT creation
  logger_set_ticks(0);
  log_create(init_process->pid, init_process->priority, init_process->name);
  // Create thread for INIT
  s_printf("[BOOT] Creating thread for INIT...\n");
  init_process->state = RUNNING;
  spthread_create(&init_process->thread, NULL, init_main, init_process);
  init_process->thread_created = true;

  s_printf("[BOOT] Adding INIT to scheduler...\n");
  scheduler_insert(init_process);

  s_printf("[BOOT] INIT process created successfully\n");

  // Start INIT process in main thread
  s_printf("[BOOT] Starting scheduler...\n\n");
  scheduler();
  // When INIT exits, system shuts down
  s_printf("\n[BOOT] System shutting down...\n");
  s_printf("[BOOT] Cleaning up scheduler...\n");
  scheduler_cleanup();
  s_printf("[BOOT] Cleaning up kernel...\n");
  k_shutdown();

  // Unmount filesystem if it was mounted
  if (fs_name != NULL) {
    s_printf("[BOOT] Unmounting filesystem...\n");
    unmount();
  }

  s_printf("[BOOT] Closing logger...\n");
  logger_close();

  s_printf("\n╔════════════════════════════════════════╗\n");
  s_printf("║         System Halted                  ║\n");
  s_printf("╚════════════════════════════════════════╝\n");
  return 0;
}
