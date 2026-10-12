#include "shell.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../tests/stress.h"
// Include kernel/commands.h BEFORE FAT headers to avoid conflict with FAT/commands.h
#include "kernel/commands.h"
#include "kernel/init.h"
#include "kernel/kernel.h"
#include "kernel/pcb.h"
#include "kernel/scheduler.h"
#include "kernel/syscall.h"
#include "kernel/user_processes.h"
#include "parser/parser.h"
#include "utils/redirection.h"
// FAT headers must come after kernel headers due to commands.h naming conflict
#include "FAT/fat_structs.h"
#include "FAT/k_functions.h"
#include "FAT/s_functions.h"

// Helper function to execute a parsed command
// Returns true if shell should continue, false if it should exit
static bool execute_command(struct parsed_command *pcmd)
{
  // Get command name (first argument of first command)
  char *cmd = pcmd->commands[0][0];
  if (cmd == NULL)
  {
    return true;
  }
  char **argv = pcmd->commands[0];
  bool is_background = pcmd->is_background;

  // Get redirected file descriptors (don't modify shell's stdin/stdout)
  int fd_in, fd_out;
  get_redirected_fds(pcmd, &fd_in, &fd_out);

  // Handle commands
  if (strcmp(cmd, "help") == 0)
  {
    print_help();
  }
  else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0 || strcmp(cmd, "logout") == 0)
  {
    cmd_logout();
    scheduler_stop();
    return false; // Signal to exit shell
  }
  else if (strcmp(cmd, "ps") == 0)
  {
    cmd_ps();
  }
  else if (strcmp(cmd, "spawn") == 0)
  {
    char *type = pcmd->commands[0][1];
    if (type != NULL)
    {
      cmd_spawn(type);
    }
    else
    {
      s_printf("Usage: spawn <type>\n");
      s_printf("Types: hello, count, sleep, zombie, zombify\n");
    }
  }
  else if (strcmp(cmd, "zombify") == 0)
  {
    cmd_zombify(pcmd->is_background);
  }
  else if (strcmp(cmd, "waitpid") == 0)
  {
    char *pid_str = pcmd->commands[0][1];
    if (pid_str != NULL)
    {
      int pid = atoi(pid_str);
      cmd_waitpid(pid);
    }
    else
    {
      s_printf("Usage: waitpid <pid>\n");
    }
  }
  else if (strcmp(cmd, "waitany") == 0)
  {
    cmd_waitany();
  }
  else if (strcmp(cmd, "waitpid_blocking") == 0)
  {
    char *pid_str = pcmd->commands[0][1];
    if (pid_str != NULL)
    {
      int pid = atoi(pid_str);
      cmd_waitpid_blocking(pid);
    }
    else
    {
      s_printf("Usage: waitpid_blocking <pid>\n");
    }
  }
  else if (strcmp(cmd, "waitany_blocking") == 0)
  {
    cmd_waitany_blocking();
  }
  else if (strcmp(cmd, "kill") == 0)
  {
    // Parse signal (optional, defaults to "term")
    int signal = P_SIGTERM; // Default signal
    int pid_start_idx = 1;  // Index where PIDs start in argv

    // Check if first argument is a signal flag
    if (argv[1] != NULL && argv[1][0] == '-')
    {
      if (strcmp(argv[1], "-term") == 0)
      {
        signal = P_SIGTERM;
      }
      else if (strcmp(argv[1], "-stop") == 0)
      {
        signal = P_SIGSTOP;
      }
      else if (strcmp(argv[1], "-cont") == 0)
      {
        signal = P_SIGCONT;
      }
      else
      {
        s_printf("kill: invalid signal '%s'\n", argv[1]);
        s_printf("Valid signals: -term, -stop, -cont\n");
        goto cleanup;
      }
      pid_start_idx = 2; // PIDs start at index 2
    }

    // Check if at least one PID is provided
    if (argv[pid_start_idx] == NULL)
    {
      s_printf("Usage: kill [-term|-stop|-cont] PID...\n");
      s_printf("  -term: Terminate process (default)\n");
      s_printf("  -stop: Stop process\n");
      s_printf("  -cont: Continue process\n");
      goto cleanup;
    }

    // Send signal to all specified PIDs
    for (int i = pid_start_idx; argv[i] != NULL; i++)
    {
      int pid = atoi(argv[i]);
      if (pid <= 0)
      {
        s_printf("kill: invalid PID '%s'\n", argv[i]);
        continue;
      }
      cmd_kill(pid, signal);
    }
  }
  else if (strcmp(cmd, "nice") == 0)
  {
    if (argv[1] == NULL || argv[2] == NULL)
    {
      s_fprintf_stderr("Usage: nice <priority> <program> [&]\n");
    }
    else
    {
      int priority = atoi(argv[1]);
      char *program = argv[2];

      // Prepare argv for the spawned process (skip "nice" and priority)
      char **spawn_argv = &argv[2];

      pid_t pid = -1;
      void *(*func)(void *) = NULL;

      // Map program names to functions
      struct
      {
        const char *name;
        void *(*func)(void *);
      } map[] = {{"cat", user_cat}, {"ls", user_ls}, {"echo", user_echo}, {"sleep", user_sleep}, {"man", user_man}, {NULL, NULL}};

      // Special handling for busy process
      if (strcmp(program, "busy") == 0)
      {
        char name[64];
        snprintf(name, sizeof(name), "busy-%d", rand() % 1000);
        char *busy_argv[] = {name, NULL};
        pid = nice_process(user_busy, busy_argv, fd_in, fd_out, priority,
                           is_background);
      }
      else
      {
        // Look up program in map
        for (int i = 0; map[i].name != NULL; i++)
        {
          if (strcmp(program, map[i].name) == 0)
          {
            func = map[i].func;
            break;
          }
        }

        if (func != NULL)
        {
          pid = nice_process(func, spawn_argv, fd_in, fd_out, priority,
                             is_background);
        }
        else
        {
          s_fprintf_stderr("Unknown program: %s\n", program);
          s_fprintf_stderr(
                  "Supported programs: cat, sleep, busy, echo, ls, man\n");
        }
      }

      if (pid > 0)
      {
        s_printf("Spawned '%s' with PID %d (priority %d)%s\n", program, pid,
               priority, is_background ? " in background" : "");
      }
      else if (pid == 0)
      {
        s_fprintf_stderr("Failed to spawn program: %s\n", program);
      }
    }
  }
  else if (strcmp(cmd, "nice_pid") == 0)
  {
    if (argv[1] == NULL || argv[2] == NULL)
    {
      s_fprintf_stderr("Usage: nice_pid <priority> <pid>\n");
    }
    else
    {
      int priority = atoi(argv[1]);
      int pid = atoi(argv[2]);

      int result = s_nice(pid, priority);

      if (result == 0)
      {
        s_printf("Set priority of process %d to %d\n", pid, priority);
      }
      else
      {
        s_fprintf_stderr("Failed to set priority for PID %d\n", pid);
      }
    }
  }
  else if (strcmp(cmd, "busy") == 0)
  {
    // Parse priority argument (optional, defaults to 0)
    int priority = 0;
    if (argv[1] != NULL)
    {
      priority = atoi(argv[1]);
      if (priority < 0 || priority > 2)
      {
        s_fprintf_stderr("Error: priority must be 0, 1, or 2\n");
        goto cleanup;
      }
    }

    // Generate unique name for the busy process
    char name[64];
    snprintf(name, sizeof(name), "busy-%d", rand() % 1000);
    char *spawn_argv[] = {name, NULL};

    // Spawn the process
    pid_t pid = s_spawn(user_busy, spawn_argv, fd_in, fd_out);

    if (pid > 0)
    {
      // Set priority
      s_nice(pid, priority);

      // Track for foreground handling (will be handled at end of loop)
      foreground_pid = is_background ? -1 : pid;

      s_printf("Spawned '%s' with PID %d (priority %d)%s\n", name, pid,
             priority, is_background ? " in background" : " in foreground");
    }
    else
    {
      s_fprintf_stderr("Failed to spawn busy process\n");
    }
  }
  else if (strcmp(cmd, "info") == 0)
  {
    char *pid_str = pcmd->commands[0][1];
    if (pid_str != NULL)
    {
      int pid = atoi(pid_str);
      cmd_info(pid);
    }
    else
    {
      s_printf("Usage: info <pid>\n");
    }
  }
  else if (strcmp(cmd, "orphanify") == 0)
  {
    cmd_orphanify();
  }
  else if (strcmp(cmd, "fg") == 0)
  {
    char *pid_str = pcmd->commands[0][1];
    pid_t fg_pid = -1; // Default to most recent background job
    if (pid_str != NULL)
    {
      fg_pid = atoi(pid_str);
    }
    cmd_fg(fg_pid);
  }
  else if (strcmp(cmd, "bg") == 0)
  {
    char *pid_str = pcmd->commands[0][1];
    pid_t bg_pid = -1; // Default to most recent stopped job
    if (pid_str != NULL)
    {
      bg_pid = atoi(pid_str);
    }
    cmd_bg(bg_pid);
  }
  else if (strcmp(cmd, "jobs") == 0)
  {
    cmd_jobs();
  }
  else if (strcmp(cmd, "cat") == 0)
  {
    pid_t pid = s_spawn(user_cat, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn cat\n");
    }
  }
  else if (strcmp(cmd, "sleep") == 0)
  {
    pid_t pid = s_spawn(user_sleep, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn sleep\n");
    }
  }
  else if (strcmp(cmd, "ls") == 0)
  {
    pid_t pid = s_spawn(user_ls, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn ls\n");
    }
  }
  else if (strcmp(cmd, "touch") == 0)
  {
    pid_t pid = s_spawn(user_touch, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn touch\n");
    }
  }
  else if (strcmp(cmd, "mv") == 0)
  {
    pid_t pid = s_spawn(user_mv, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn mv\n");
    }
  }
  else if (strcmp(cmd, "cp") == 0)
  {
    pid_t pid = s_spawn(user_cp, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn cp\n");
    }
  }
  else if (strcmp(cmd, "rm") == 0)
  {
    pid_t pid = s_spawn(user_rm, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn rm\n");
    }
  }
  else if (strcmp(cmd, "echo") == 0)
  {
    pid_t pid = s_spawn(user_echo, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn echo\n");
    }
  }
  else if (strcmp(cmd, "man") == 0)
  {
    pid_t pid = s_spawn(user_man, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn man\n");
    }
  }
  else if (strcmp(cmd, "chmod") == 0)
  {
    // chmod is handled directly (not as a spawned process)
    user_chmod(argv);
  }
  else if (strcmp(cmd, "hang") == 0)
  {
    pid_t pid = s_spawn(hang, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn hang\n");
    }
  }
  else if (strcmp(cmd, "nohang") == 0)
  {
    pid_t pid = s_spawn(nohang, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn nohang\n");
    }
  }
  else if (strcmp(cmd, "recur") == 0)
  {
    pid_t pid = s_spawn(recur, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn recur\n");
    }
  }
  else if (strcmp(cmd, "crash") == 0)
  {
    pid_t pid = s_spawn(crash, argv, fd_in, fd_out);
    if (pid > 0)
    {
      pcb_t *proc = k_get_process(pid);
      if (proc != NULL)
      {
        proc->is_background = is_background;
      }
      foreground_pid = is_background ? -1 : pid;
      if (is_background)
      {
        s_printf("[%d]\n", pid);
      }
    }
    else
    {
      s_fprintf_stderr("Failed to spawn crash\n");
    }
  }
  else
  {
    // Check if an executable file exists with this name
    directory_entry_t entry;
    if (find_file_in_directory(cmd, &entry) == 0)
    {
      // File exists, check if it has execute permission
      if ((entry.perm & 1) || entry.perm == PERM_ALL ||
          entry.perm == PERM_READ_EXEC)
      {
        // File is executable - read and execute it line by line
        int script_fd = s_open(cmd, F_READ);
        if (script_fd >= 0)
        {
          // Read file content
          char *script_buf = malloc(entry.size + 1);
          if (script_buf != NULL)
          {
            int bytes_read = s_read(script_fd, entry.size, script_buf);
            s_close(script_fd);

            if (bytes_read > 0)
            {
              script_buf[bytes_read] = '\0';

              // Execute each line as a command
              // If parent has output redirection, ensure the file exists
              if (fd_out >= 0 && pcmd->stdout_file != NULL) {
                // File is already opened by get_redirected_fds,
                // but we need to ensure it exists for child commands to open independently
                // Close and let children reopen (not ideal but simple)
                s_close(fd_out);
                fd_out = -1;
              }

              // Inherit output redirection from parent if present
              const char* inherited_stdout = pcmd->stdout_file;
              // Force append mode for script commands to avoid truncation
              // between successive commands in the script
              bool inherited_append = true;  // Always append within script

              char* line_ptr = script_buf;
              char* newline;

              while ((newline = strchr(line_ptr, '\n')) != NULL)
              {
                *newline = '\0';

                // Skip empty lines and comments
                if (line_ptr[0] != '\0' && line_ptr[0] != '#')
                {
                  // Parse and execute this line
                  struct parsed_command* script_pcmd;
                  if (parse_command(line_ptr, &script_pcmd) == 0) {
                    // Inherit output redirection if script has redirection but command doesn't
                    if (inherited_stdout != NULL && script_pcmd->stdout_file == NULL) {
                      script_pcmd->stdout_file = inherited_stdout;
                      script_pcmd->is_file_append = inherited_append;
                    }

                    // Recursively execute the command
                    if (!execute_command(script_pcmd))
                    {
                      free(script_pcmd);
                      free(script_buf);
                      return false; // exit command in script
                    }
                    free(script_pcmd);
                  }
                }

                line_ptr = newline + 1;
              }

              // Handle last line if file doesn't end with newline
              if (*line_ptr != '\0' && line_ptr[0] != '#') {
                struct parsed_command* script_pcmd;
                if (parse_command(line_ptr, &script_pcmd) == 0) {
                  // Inherit output redirection if script has redirection but command doesn't
                  if (inherited_stdout != NULL && script_pcmd->stdout_file == NULL) {
                    script_pcmd->stdout_file = inherited_stdout;
                    script_pcmd->is_file_append = inherited_append;
                  }

                  if (!execute_command(script_pcmd)) {
                    free(script_pcmd);
                    free(script_buf);
                    return false; // exit command in script
                  }
                  free(script_pcmd);
                }
              }
            }
            free(script_buf);
          }
          else
          {
            s_fprintf_stderr("%s: out of memory\n", cmd);
          }
        }
        else
        {
          s_fprintf_stderr("%s: cannot open file\n", cmd);
        }
      }
      else
      {
        s_fprintf_stderr("%s: permission denied\n", cmd);
      }
    }
    else
    {
      s_printf("Unknown command: %s\n", cmd);
      s_printf("Type 'man' for available commands\n");
    }
  }

cleanup:
  // Handle foreground process blocking
  // If a command spawned a foreground process, wait for it here
  if (foreground_pid > 0)
  {
    // Shell blocks waiting for foreground process to finish
    int status;
    s_waitpid(foreground_pid, &status, false); // Blocking wait

    // Clear foreground process
    foreground_pid = -1;
  }

  // Close redirected file descriptors if they were opened
  if (fd_in != STDIN_FILENO && fd_in != -1)
  {
    s_close(fd_in);
  }
  if (fd_out != STDOUT_FILENO && fd_out != -1)
  {
    s_close(fd_out);
  }

  return true;
}

// Main shell loop
void run_shell(void)
{
  char line[256];

  // Set up Ctrl-C handler
  signal(SIGINT, sigint_handler);

  s_printf("\n╔════════════════════════════════════════╗\n");
  s_printf("║   PennOS Kernel Interactive Shell     ║\n");
  s_printf("║   Type 'help' for available commands  ║\n");
  s_printf("╚════════════════════════════════════════╝\n");

  while (1)
  {


    s_printf("\npennos> ");
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL)
    {
      break;
    }

    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Skip empty lines
    if (strlen(line) == 0)
    {
      continue;
    }

    // Parse command using parser
    struct parsed_command *pcmd;
    int parse_result = parse_command(line, &pcmd);

    if (parse_result != 0)
    {
      if (parse_result > 0)
      {
        print_parser_errcode(stderr, parse_result);
      }
      else
      {
        perror("Parse error");
      }
      continue;
    }
    // Auto-reap any zombie children before printing prompt
    // We need to be careful here: s_waitpid will return stopped children
    // but we don't want to reap them (they're still alive). We also don't
    // want to loop forever returning the same stopped child.
    // So we'll track if we saw a stopped child and break after one iteration.
    int saw_stopped = false;
    while (1)
    {
      int status;
      pid_t reaped = s_waitpid(-1, &status, true); // Non-blocking
      if (reaped <= 0)
      {
        break; // No more children to wait for
      }
      // If we got a stopped child, note it and break after this iteration
      // to avoid infinite loop (stopped children remain in the list)
      if (P_WIFSTOPPED(status))
      {
        if (saw_stopped)
        {
          break; // Already saw a stopped child, don't loop forever
        }
        saw_stopped = true;
      }
      // Continue to reap other zombies even if we saw a stopped child
    }

    // Execute the command
    if (!execute_command(pcmd))
    {
      // execute_command returned false (exit command)
      free(pcmd);
      break;
    }

    free(pcmd);
  }
}

// Shell main function (runs as a child of INIT)
void *shell_main(void *arg)
{
  char **argv = (char **)arg;
  s_printf("[SHELL] Starting shell process: %s\n", argv[0]);
  // Get our own PCB from the process table
  // We need to find ourselves - we're a child of INIT (PID 1)
  pcb_t *init = k_get_process(1);
  if (init == NULL || init->num_children == 0)
  {
    s_fprintf_stderr("[SHELL] Cannot find shell process!\n");
    return NULL;
  }

  // We should be INIT's first and only child initially
  pid_t shell_pid = init->children[0];
  pcb_t *shell_process = k_get_process(shell_pid);

  if (shell_process == NULL)
  {
    s_fprintf_stderr("[SHELL] Cannot find shell PCB!\n");
    return NULL;
  }

  // Set shell as current process for this thread
  k_set_current_process(shell_process);

  s_printf("[SHELL] Shell process started (PID %d)\n", shell_pid);

  run_shell();

  s_printf("[SHELL] Shell process exiting...\n");
  return NULL;
}
