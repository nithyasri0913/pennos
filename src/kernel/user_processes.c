#include "kernel/user_processes.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "FAT/commands.h"
#include "FAT/k_functions.h"
#include "FAT/s_functions.h"
#include "kernel/kernel.h"
#include "kernel/logging.h"
#include "kernel/pcb.h"
#include "kernel/scheduler.h"
#include "kernel/syscall.h"

// Example process functions for testing
void* hello_process(void* arg) {
  char** argv = (char**)arg;
  s_printf("  [Process %s]: Hello from child process!\n", argv[0]);
  spthread_suspend_self();
  return NULL;
}

void* counting_process(void* arg) {
  char** argv = (char**)arg;
  s_printf("  [Process %s]: Counting to 3...\n", argv[0]);
  for (int i = 1; i <= 3; i++) {
    s_printf("  [Process %s]: %d\n", argv[0], i);
    spthread_suspend_self();
  }
  s_printf("  [Process %s]: Done!\n", argv[0]);
  return NULL;
}

void* user_sleep(void* arg) {
  char** argv = (char**)arg;

  // Require the ticks argument
  if (argv[1] == NULL) {
    s_fprintf_stderr("sleep: missing operand\n");
    s_fprintf_stderr("Usage: sleep TICKS\n");
    return NULL;
  }

  unsigned int ticks = (unsigned int)atoi(argv[1]);
  if (ticks == 0) {
    s_fprintf_stderr("sleep: invalid time interval '%s'\n", argv[1]);
    return NULL;
  }

  s_sleep(ticks);
  return NULL;
}

void* zombie_process(void* arg) {
  char** argv = (char**)arg;
  s_printf("  [Process %s]: I will become a zombie...\n", argv[0]);
  spthread_suspend_self();
  return NULL;
}

void* ps_process(void* arg) {
  s_printf("\n%-6s %-6s %-10s %-8s %-12s\n", "PID", "PPID", "NAME", "PRIO",
         "STATE");
  s_printf("--------------------------------------------------------\n");

  for (int i = 0; i < MAX_PROCESSES; i++) {
    pcb_t* proc = k_get_process(i);
    if (proc != NULL) {
      const char* state_str;
      switch (proc->state) {
        case RUNNING:
          state_str = "RUNNING";
          break;
        case BLOCKED:
          state_str = "BLOCKED";
          break;
        case STOPPED:
          state_str = "STOPPED";
          break;
        case ZOMBIE:
          state_str = "ZOMBIE";
          break;
        default:
          state_str = "UNKNOWN";
          break;
      }

      s_printf("%-6d %-6d %-10s %-8d %-12s\n", proc->pid, proc->ppid, proc->name,
             proc->priority, state_str);
    }
  }

  s_printf("\nTotal processes: %d\n\n", k_get_process_count());
  return NULL;
}

/**
 * @brief Helper for orphanify - orphan child that runs forever after being
 * reparented.
 */
void* orphan_child(void* arg) {
  // Get our PCB to access our name (don't use argv - parent may have exited)
  pcb_t* self = k_get_current_process();
  if (self == NULL) {
    return NULL;
  }

  s_printf(
      "  [Process %s]: I'm an orphan child, waiting to be adopted by INIT...\n",
      self->name);

  // Wait for parent to exit and reparenting to happen
  spthread_suspend_self();

  // Check if we've been reparented to INIT
  self = k_get_current_process();
  if (self != NULL && self->ppid == 1) {
    s_printf("  [Process %s]: Successfully reparented to INIT (PID 1)!\n",
           self->name);
    s_printf("  [Process %s]: Orphan child will now run forever...\n",
           self->name);
  } else if (self != NULL) {
    s_printf("  [Process %s]: ERROR - still have parent PID %d\n", self->name,
           self->ppid);
  }

  // Run forever - orphan child stays alive under INIT
  while (1) {
    spthread_suspend_self();
  }

  return NULL;
}

/**
 * @brief Used to test orphanifying functionality of your kernel.
 * Spawns a child process then immediately exits, orphaning the child.
 * The child should be reparented to INIT (PID 1) and will run forever.
 *
 * Example Usage: orphanify
 */
void* orphanify(void* arg) {
  char** argv = (char**)arg;
  s_printf("  [Process %s]: Starting orphanify parent\n", argv[0]);

  // Spawn the orphan child
  char child_name[64];
  snprintf(child_name, sizeof(child_name), "orphan-child-of-%s", argv[0]);
  char* child_argv[] = {child_name, NULL};

  pid_t child_pid = s_spawn(orphan_child, child_argv, -1, -1);

  if (child_pid > 0) {
    s_printf("  [Process %s]: Spawned orphan child PID %d\n", argv[0], child_pid);
    s_printf(
        "  [Process %s]: Parent exiting immediately to orphan the child...\n",
        argv[0]);
  } else {
    s_fprintf_stderr("  [Process %s]: Failed to spawn orphan child\n", argv[0]);
  }

  // Exit immediately - this orphans the child
  return NULL;
}

// Zombify parent process - creates a zombie child and waits
void* zombify_parent_process(void* arg) {
  char** argv = (char**)arg;
  s_printf("  [Process %s]: Starting zombify parent\n", argv[0]);

  // Spawn a child that will exit immediately
  char child_name[64];
  snprintf(child_name, sizeof(child_name), "zombie-child-of-%s", argv[0]);
  char* child_argv[] = {child_name, NULL};

  pid_t child_pid = s_spawn(zombie_process, child_argv, -1, -1);

  if (child_pid > 0) {
    s_printf("  [Process %s]: Created zombie child PID %d\n", argv[0], child_pid);

    // Wait for child to become zombie
    spthread_suspend_self();

    s_printf(
        "  [Process %s]: Child is now a zombie. Parent will wait "
        "indefinitely...\n",
        argv[0]);
    s_printf(
        "  [Process %s]: (Press Ctrl-C to kill parent and orphan the zombie)\n",
        argv[0]);

    // Wait indefinitely (until killed)
    while (1) {
      // Check if we've been terminated (e.g., by SIGTERM)
      pcb_t* self = k_get_current_process();
      if (self != NULL && self->state == ZOMBIE) {
        s_printf("  [Process %s]: Received termination signal, exiting...\n",
               argv[0]);
        break;
      }
    }
  } else {
    s_fprintf_stderr("  [Process %s]: Failed to spawn zombie child\n", argv[0]);
  }

  return NULL;
}

pid_t nice_process(void* (*func)(void*),
                   char* argv[],
                   int fd0,
                   int fd1,
                   int priority,
                   bool bg) {
  // Validate priority
  if (priority < 0 || priority > 2) {
    s_fprintf_stderr("nice_process: invalid priority %d (must be 0, 1, or 2)\n",
            priority);
    return -1;
  }

  // Spawn the process
  pid_t pid = s_spawn(func, argv, fd0, fd1);
  if (pid < 0) {
    s_fprintf_stderr("nice_process: failed to spawn process\n");
    return -1;
  }

  // Set the priority immediately after spawning
  if (s_nice(pid, priority) == -1) {
    s_fprintf_stderr("nice_process: failed to set priority %d for process %d\n",
            priority, pid);
    return -1;
  }

  // If foreground, wait for the process
  if (!bg) {
    int status;
    s_waitpid(pid, &status, false);
  }

  return pid;
}

void* user_busy(void* arg) {
  s_printf("  [Busy Process]: Starting busy process...\n");
  while (1)
    ;
  return NULL;
}

// User-level functions
void* user_cat(void* arg) {
  char** args = (char**)arg;
  // args[0] = "cat"
  // Determine operation mode by checking flags and arguments

  int arg_idx = 1;
  int write_mode = 0;  // 0 = none, 1 = -w (overwrite), 2 = -a (append)
  char* output_file = NULL;
  int num_input_files = 0;

  // Parse arguments to find flags
  while (args[arg_idx] != NULL) {
    if (strcmp(args[arg_idx], "-w") == 0) {
      write_mode = 1;
      arg_idx++;
      if (args[arg_idx] != NULL) {
        output_file = args[arg_idx];
        arg_idx++;
      }
      break;
    } else if (strcmp(args[arg_idx], "-a") == 0) {
      write_mode = 2;
      arg_idx++;
      if (args[arg_idx] != NULL) {
        output_file = args[arg_idx];
        arg_idx++;
      }
      break;
    } else {
      num_input_files++;
      arg_idx++;
    }
  }

  // Check filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("cat: filesystem not mounted\n");
    return NULL;
  }

  // Mode 1: cat -w OUTPUT_FILE (read from terminal, overwrite)
  // Mode 2: cat -a OUTPUT_FILE (read from terminal, append)
  if ((write_mode == 1 || write_mode == 2) && num_input_files == 0) {
    if (output_file == NULL) {
      s_printf("cat: missing output file operand\n");
      return NULL;
    }

    // Read from stdin until EOF (Ctrl+D)
    s_printf("Reading from terminal (press Ctrl+D to finish):\n");

    int fd = s_open(output_file, write_mode == 1 ? F_WRITE : F_APPEND);
    if (fd < 0) {
      if (fd == FILE_NOT_FOUND && write_mode == 1) {
        // F_WRITE should create file, so this shouldn't happen
        s_printf("cat: cannot create '%s'\n", output_file);
      } else {
        s_printf("cat: cannot open '%s'\n", output_file);
      }
      return NULL;
    }

    char line[1024];
    while (fgets(line, sizeof(line), stdin) != NULL) {
      int len = strlen(line);
      s_write(fd, line, len);
    }

    // Clear EOF condition on stdin for next use
    if (feof(stdin)) {
      clearerr(stdin);
    }

    s_close(fd);
    return NULL;
  }

  // Mode 3 & 4: cat FILE ... [-w/-a OUTPUT_FILE]
  // Concatenate files and output to stdout or file

  int out_fd = -1;
  if (output_file != NULL) {
    out_fd = s_open(output_file, write_mode == 1 ? F_WRITE : F_APPEND);
    if (out_fd < 0) {
      s_printf("cat: cannot open '%s' for writing\n", output_file);
      return NULL;
    }
  } else {
    // Check if stdout was redirected via shell (e.g., cat file > output)
    // If fd_table[1] was set by s_spawn, use it as the output fd
    pcb_t* self = k_get_current_process();
    if (self != NULL) {
      if (self->fd_table[1] != STDOUT_FILENO && self->fd_table[1] != -1) {
        // Shell redirection is active, use fd 1 from fd_table
        out_fd = 1;
      }
    }
  }

  // If no input files specified, read from stdin (handles "cat" or "cat <file")
  if (num_input_files == 0) {
    // Check if stdin was redirected via shell
    pcb_t* self = k_get_current_process();
    int in_fd = -1;
    int stdin_redirected = 0;
    int stdout_redirected = 0;

    if (self != NULL) {
      if (self->fd_table[0] != STDIN_FILENO && self->fd_table[0] != -1) {
        // Shell redirection is active, use fd 0 from fd_table
        in_fd = 0;
        stdin_redirected = 1;
      }
      if (self->fd_table[1] != STDOUT_FILENO && self->fd_table[1] != -1) {
        stdout_redirected = 1;
      }
    }

    // Read from stdin (redirected file only)
    if (stdin_redirected && in_fd == 0) {
      // Stdin is redirected to a file
      // If both stdin and stdout are redirected to files, we need to read all input
      // first before writing to avoid the case where input and output are the same file
      if (stdout_redirected && out_fd >= 0) {
        // Read entire input into memory first
        char* full_buffer = NULL;
        size_t total_size = 0;
        char buffer[512];
        int bytes_read;

        while ((bytes_read = s_read(in_fd, sizeof(buffer), buffer)) > 0) {
          char* new_buffer = realloc(full_buffer, total_size + bytes_read);
          if (new_buffer == NULL) {
            s_printf("cat: out of memory\n");
            free(full_buffer);
            if (out_fd >= 0) {
              s_close(out_fd);
            }
            return NULL;
          }
          full_buffer = new_buffer;
          memcpy(full_buffer + total_size, buffer, bytes_read);
          total_size += bytes_read;
        }

        // Now write everything to output
        if (total_size > 0 && full_buffer != NULL) {
          s_write(out_fd, full_buffer, total_size);
          free(full_buffer);
        }
      } else {
        // Normal streaming mode - read and write in chunks
        char buffer[512];
        int bytes_read;
        while ((bytes_read = s_read(in_fd, sizeof(buffer), buffer)) > 0) {
          if (out_fd >= 0) {
            // Write to output file or redirected stdout
            s_write(out_fd, buffer, bytes_read);
          } else {
            // Write to stdout
            fwrite(buffer, 1, bytes_read, stdout);
          }
        }
      }
    } else {
      // Stdin is NOT redirected - read from terminal
      char line[1024];
      while (fgets(line, sizeof(line), stdin) != NULL) {
        if (out_fd >= 0) {
          // Write to redirected stdout
          s_write(out_fd, line, strlen(line));
        } else {
          // Echo to stdout
          s_printf("%s", line);
        }
      }
      // Clear EOF condition on stdin for next use
      if (feof(stdin)) {
        clearerr(stdin);
      }
    }
  } else {
    // Process each input file
    arg_idx = 1;
    while (args[arg_idx] != NULL) {
      // Stop at flags
      if (strcmp(args[arg_idx], "-w") == 0 || strcmp(args[arg_idx], "-a") == 0) {
        break;
      }

      char* input_file = args[arg_idx];
      int in_fd = s_open(input_file, F_READ);

      if (in_fd == FILE_NOT_FOUND) {
        s_printf("cat: Error %s: No such file\n", input_file);
        arg_idx++;
        continue;
      } else if (in_fd < 0) {
        s_printf("cat: Error cannot open '%s'\n", input_file);
        arg_idx++;
        continue;
      }

      // Read and output file contents
      char buffer[512];
      int bytes_read;
      while ((bytes_read = s_read(in_fd, sizeof(buffer), buffer)) > 0) {
        if (out_fd >= 0) {
          // Write to output file
          s_write(out_fd, buffer, bytes_read);
        } else {
          // Write to stdout
          fwrite(buffer, 1, bytes_read, stdout);
        }
      }

      s_close(in_fd);
      arg_idx++;
    }
  }

  if (out_fd >= 0) {
    s_close(out_fd);
  }
  return NULL;
}

void* user_echo(void* arg) {
  char** argv = (char**)arg;

  // Check if stdout is redirected
  pcb_t* current = k_get_current_process();


  if (current != NULL && current->fd_table[1] != -1) {
    // Output is redirected, use s_write to write to the redirected file
    for (int i = 1; argv[i] != NULL; i++) {
      s_write(1, argv[i], strlen(argv[i]));
      if (argv[i + 1] != NULL) {
        s_write(1, " ", 1);
      }
    }
    s_write(1, "\n", 1);
  } else {
    // No redirection, normal stdout
    for (int i = 1; argv[i] != NULL; i++) {
      s_printf("%s", argv[i]);
      if (argv[i + 1] != NULL) {
        s_printf(" ");
      }
    }
    s_printf("\n");
    fflush(stdout);
  }

  return NULL;
}

void* user_ls(void* arg) {
  char** args = (char**)arg;
  if (args[1] == NULL) {
    s_ls(NULL);
  } else {
    s_ls(args[1]);
  }
  return NULL;
}

void* user_touch(void* arg) {
  char** args = (char**)arg;
  // args[0] = "touch"
  // args[1..n] = filename

  // Check filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("touch: filesystem not mounted\n");
    return NULL;
  }

  for (int i = 1; args[i] != NULL; i++) {
    char* fname = args[i];
    int fd = k_open(fname, F_APPEND);
    if (fd < 0) {
      s_printf("touch: cannot create file '%s'\n", fname);
      return NULL;
    }
    if (k_close(fd) < 0) {
      s_printf("touch: error closing file '%s'\n", fname);
    }
  }
  return NULL;
}

void* user_mv(void* arg) {
  char** args = (char**)arg;
  // args[0] = "mv"
  // args[1] = source filename
  // args[2] = destination filename

  // Check if filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("mv: filesystem not mounted\n");
    return NULL;
  }

  char src[32];   // 31 characters + null terminator
  char dest[32];  // 31 characters + null terminator

  // Copy and ensure null termination for src
  strncpy(src, args[1], 31);
  src[31] = '\0';

  // Copy and ensure null termination for dest
  strncpy(dest, args[2], 31);
  dest[31] = '\0';

  // Check if source file exists
  directory_entry_t src_entry;
  if (find_file_in_directory(src, &src_entry) < 0) {
    s_printf("mv: cannot stat '%s': No such file\n", src);
    return NULL;
  }

  // If destination already exists, remove it first
  directory_entry_t dest_entry;
  if (find_file_in_directory(dest, &dest_entry) == 0) {
    int result = k_unlink(dest);
    if (result == -1) {
      s_printf("mv: cannot remove '%s': file is currently open\n", dest);
      return NULL;
    } else if (result < 0) {
      s_printf("mv: cannot remove '%s'\n", dest);
      return NULL;
    }
  }
  // Update only the name field
  strncpy(src_entry.name, dest, 31);
  src_entry.name[31] = '\0';

  if (update_directory_entry(src, &src_entry) < 0) {
    s_printf("mv: error renaming '%s' to '%s'\n", src, dest);
  }
  return NULL;
}

void* user_cp(void* arg) {
  char** args = (char**)arg;
  // args[0] = "cp"
  // Usage: cp [-h] SOURCE DEST  OR  cp SOURCE -h DEST

  if (args[1] == NULL || args[2] == NULL) {
    s_printf("cp: missing file operand\n");
    s_printf("Usage: cp [-h] SOURCE DEST  OR  cp SOURCE -h DEST\n");
    return NULL;
  }

  // Check filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("cp: filesystem not mounted\n");
    return NULL;
  }

  int host_to_fs = 0;  // cp -h SOURCE DEST (host to filesystem)
  int fs_to_host = 0;  // cp SOURCE -h DEST (filesystem to host)
  char* source = NULL;
  char* dest = NULL;

  // Parse arguments
  if (strcmp(args[1], "-h") == 0) {
    // cp -h SOURCE DEST (copy from host to filesystem)
    if (args[3] == NULL) {
      s_printf("cp: missing destination file operand\n");
      return NULL;
    }
    host_to_fs = 1;
    source = args[2];
    dest = args[3];
  } else if (args[3] != NULL && strcmp(args[2], "-h") == 0) {
    // cp SOURCE -h DEST (copy from filesystem to host)
    fs_to_host = 1;
    source = args[1];
    dest = args[3];
  } else {
    // cp SOURCE DEST (copy within filesystem)
    source = args[1];
    dest = args[2];
  }

  if (host_to_fs) {
    // Copy from host OS to filesystem
    FILE* host_file = fopen(source, "rb");
    if (host_file == NULL) {
      s_printf("cp: cannot open host file '%s'\n", source);
      return NULL;
    }

    // Open destination file in filesystem
    int fs_fd = s_open(dest, F_WRITE);
    if (fs_fd < 0) {
      s_printf("cp: cannot create '%s' in filesystem\n", dest);
      fclose(host_file);
      return NULL;
    }

    // Copy data
    char buffer[512];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), host_file)) > 0) {
      s_write(fs_fd, buffer, bytes_read);
    }

    fclose(host_file);
    s_close(fs_fd);
  } else if (fs_to_host) {
    // Copy from filesystem to host OS
    int fs_fd = s_open(source, F_READ);
    if (fs_fd == FILE_NOT_FOUND) {
      s_printf("cp: Error cannot stat '%s': No such file\n", source);
      return NULL;
    } else if (fs_fd < 0) {
      s_printf("cp: Error cannot open '%s' in filesystem\n", source);
      return NULL;
    }

    // Open destination file on host
    FILE* host_file = fopen(dest, "wb");
    if (host_file == NULL) {
      s_printf("cp: cannot create host file '%s'\n", dest);
      s_close(fs_fd);
      return NULL;
    }

    // Copy data
    char buffer[512];
    int bytes_read;
    while ((bytes_read = s_read(fs_fd, sizeof(buffer), buffer)) > 0) {
      fwrite(buffer, 1, bytes_read, host_file);
    }

    s_close(fs_fd);
    fclose(host_file);
  } else {
    // Copy within filesystem
    int src_fd = s_open(source, F_READ);
    if (src_fd == FILE_NOT_FOUND) {
      s_printf("cp: Error cannot stat '%s': No such file\n", source);
      return NULL;
    } else if (src_fd < 0) {
      s_printf("cp: Error cannot open '%s'\n", source);
      return NULL;
    }

    // Check if destination already exists
    directory_entry_t dest_entry;
    if (find_file_in_directory(dest, &dest_entry) == 0) {
      int result = s_unlink(dest);
      if (result == -1) {
        s_printf("cp: cannot copy to '%s': file is currently open\n", dest);
        s_close(src_fd);
        return NULL;
      } else if (result < 0) {
        s_printf("cp: cannot copy to '%s'\n", dest);
        s_close(src_fd);
        return NULL;
      }
    }

    int dest_fd = s_open(dest, F_WRITE);
    if (dest_fd < 0) {
      s_printf("cp: cannot create '%s'\n", dest);
      s_close(src_fd);
      return NULL;
    }

    // Copy data
    char buffer[512];
    int bytes_read;
    while ((bytes_read = s_read(src_fd, sizeof(buffer), buffer)) > 0) {
      s_write(dest_fd, buffer, bytes_read);
    }

    s_close(src_fd);
    s_close(dest_fd);
  }
  return NULL;
}

void* user_rm(void* arg) {
  char** args = (char**)arg;
  // args[0] = "rm"
  // args[1] = filename

  // Check filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("rm: filesystem not mounted\n");
    return NULL;
  }

  char* fname = args[1];
  int result = s_unlink(fname);
  if (result == FILE_NOT_FOUND) {
    s_printf("rm: Error cannot remove '%s': No such file\n", fname);
    return NULL;
  }
  if (result == -1) {
    s_printf("rm: cannot remove '%s': file is currently open\n", fname);
  }
  return NULL;
}

void* user_man(void* arg) {
  s_printf("\n=== PennOS Available Commands ===\n\n");

  s_printf("Process Management:\n");
  s_printf("  ps                    - List all processes\n");
  s_printf("  spawn <type>          - Spawn a new process\n");
  s_printf(
      "  kill <pid> <signal>   - Send signal to process (0=STOP, 1=CONT, "
      "2=TERM)\n");
  s_printf(
      "  nice <priority> <cmd> - Run command with specified priority (0-2)\n");
  s_printf("  nice_pid <priority> <pid> - Change priority of existing process\n");
  s_printf("  waitpid <pid>         - Wait for a specific child process\n");
  s_printf("  waitany               - Wait for any child process\n");
  s_printf("  info <pid>            - Show detailed process information\n");
  s_printf("  busy [priority] [&]   - Run CPU-intensive process\n");
  s_printf("  zombify [&]           - Create parent with zombie child\n");
  s_printf("  orphanify             - Create orphan child process\n");
  s_printf("  sleep <seconds> [&]   - Sleep for specified seconds\n\n");

  s_printf("File System:\n");
  s_printf("  cat <file>            - Display file contents\n");
  s_printf("  ls [directory]        - List directory contents\n");
  s_printf("  touch <file>          - Create a new file\n");
  s_printf("  mv <src> <dest>       - Move/rename file\n");
  s_printf("  cp <src> <dest>       - Copy file\n");
  s_printf("  rm <file>             - Remove file\n");
  s_printf(
      "  chmod <mode> <file>   - Change file permissions (e.g., rw, r, rwx)\n");
  s_printf("  echo <text>           - Print text to stdout\n\n");

  s_printf("Shell:\n");
  s_printf("  man                   - Show this help message\n");
  s_printf("  help                  - Show kernel shell help\n");
  s_printf("  exit / quit           - Exit the shell\n\n");

  s_printf("Notes:\n");
  s_printf("  - Add '&' at end of command to run in background\n");
  s_printf("  - Priority: 0 (high), 1 (medium), 2 (low)\n");
  s_printf("  - Use Ctrl-C to terminate foreground processes\n\n");

  return NULL;
}

void user_chmod(char** args) {
  // args[0] = "chmod"
   // args[1] = permissions (e.g., "rw", "r", "rwx", "+x", "-w", "+rw")
  // args[2] = filename

  if (args[1] == NULL || args[2] == NULL) {
    s_fprintf_stderr("chmod: missing operand\n");
    s_fprintf_stderr("Usage: chmod MODE FILE\n");
    return;
  }

  const char* mode_str = args[1];
  const char* filename = args[2];

   // find file in directory first (needed for symbolic modes)
  directory_entry_t entry;
  if (find_file_in_directory(filename, &entry) < 0) {
    s_fprintf_stderr("chmod: cannot access '%s': No such file\n", filename);
    return;
  }

  // Determine if this is symbolic (+/-) or absolute mode
  int symbolic_mode = 0;  // 0 = absolute, 1 = add (+), 2 = remove (-)
  int start_idx = 0;

  if (mode_str[0] == '+') {
    symbolic_mode = 1;
    start_idx = 1;
  } else if (mode_str[0] == '-') {
    symbolic_mode = 2;
    start_idx = 1;
  }

  // parse permission string to permission bits
  uint8_t perm = 0;

  for (int i = start_idx; mode_str[i] != '\0'; i++) {
    switch (mode_str[i]) {
      case 'r':
        perm |= 4;  // Read bit
        break;
      case 'w':
        perm |= 2;  // Write bit
        break;
      case 'x':
        perm |= 1;  // Execute bit
        break;
      default:
        s_fprintf_stderr("chmod: invalid mode: '%c'\n", mode_str[i]);
        return;
    }
  }


  // Apply permissions based on mode
  if (symbolic_mode == 1) {
    // Add permissions (+)
    entry.perm |= perm;
  } else if (symbolic_mode == 2) {
    // Remove permissions (-)
    entry.perm &= ~perm;
  } else {
    // Absolute mode - set permissions directly
    entry.perm = perm;
  }

  // Write back to directory
  if (update_directory_entry(filename, &entry) < 0) {
    s_fprintf_stderr("chmod: failed to update '%s'\n", filename);
    return;
  }
}