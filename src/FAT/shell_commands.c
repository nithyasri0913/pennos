#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "fat_structs.h"
#include "fd_table.h"
#include "k_functions.h"
#include "s_functions.h"
#include "../kernel/p_errno.h"

void cmd_ls(char** args) {
  if (args[1] == NULL) {
    k_ls(NULL);
  } else {
    k_ls(args[1]);
  }
}

void cmd_chmod(char** args) {
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

void rm(char** args) {
  // args[0] = "rm"
  // args[1] = filename
  char* fname = args[1];
  int result = k_unlink(fname);
  if (result < 0) {
    fat_perror("rm");
    return;
  }
  s_printf("Removed '%s'\n", fname);
}

void touch(char** args) {
  // args[0] = "touch"
  // args[1..n] = filename

  for (int i = 1; args[i] != NULL; i++) {
    char* fname = args[i];
    int fd = k_open(fname, F_APPEND);
    if (fd < 0) {
      fat_perror("touch");
      return;
    }
    if (k_close(fd) < 0) {
      fat_perror("touch");
    }
  }
}

void mv(char** args) {
  // args[0] = "mv"
  // args[1] = source filename
  // args[2] = destination filename

  // Check if filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("mv: filesystem not mounted\n");
    return;
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
    s_printf("mv: cannot move '%s': as no such file exists\n", src);
    return;
  }

  // If destination already exists, remove it first
  directory_entry_t dest_entry;
  if (find_file_in_directory(dest, &dest_entry) == 0) {
    int result = k_unlink(dest);
    if (result == -1) {
      s_printf("mv: cannot remove '%s': file is currently open\n", dest);
      return;
    } else if (result < 0) {
      s_printf("mv: cannot remove '%s'\n", dest);
      return;
    }
  }

  // Update only the name field
  strncpy(src_entry.name, dest, 31);
  src_entry.name[31] = '\0';

  if (update_directory_entry(src, &src_entry) < 0) {
    s_printf("mv: error renaming '%s' to '%s'\n", src, dest);
  }
}

void cat(char** args) {
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
    return;
  }

  // Mode 1: cat -w OUTPUT_FILE (read from terminal, overwrite)
  // Mode 2: cat -a OUTPUT_FILE (read from terminal, append)
  if ((write_mode == 1 || write_mode == 2) && num_input_files == 0) {
    if (output_file == NULL) {
      s_printf("cat: missing output file operand\n");
      return;
    }

    // Read from stdin until EOF (Ctrl+D)
    s_printf("Reading from terminal (press Ctrl+D to finish):\n");

    int fd = k_open(output_file, write_mode == 1 ? F_WRITE : F_APPEND);
    if (fd < 0) {
      if (fd == FILE_NOT_FOUND && write_mode == 1) {
        // F_WRITE should create file, so this shouldn't happen
        s_printf("cat: cannot create '%s'\n", output_file);
      } else {
        s_printf("cat: cannot open '%s'\n", output_file);
      }
      return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), stdin) != NULL) {
      int len = strlen(line);
      k_write(fd, line, len);
    }

    k_close(fd);
    return;
  }

  // Mode 3 & 4: cat FILE ... [-w/-a OUTPUT_FILE]
  // Concatenate files and output to stdout or file

  int out_fd = -1;
  if (output_file != NULL) {
    out_fd = k_open(output_file, write_mode == 1 ? F_WRITE : F_APPEND);
    if (out_fd < 0) {
      s_printf("cat: cannot open '%s' for writing\n", output_file);
      return;
    }
  }

  // Process each input file
  arg_idx = 1;
  while (args[arg_idx] != NULL) {
    // Stop at flags
    if (strcmp(args[arg_idx], "-w") == 0 || strcmp(args[arg_idx], "-a") == 0) {
      break;
    }

    char* input_file = args[arg_idx];
    int in_fd = k_open(input_file, F_READ);

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
    while ((bytes_read = k_read(in_fd, sizeof(buffer), buffer)) > 0) {
      if (out_fd >= 0) {
        // Write to output file
        k_write(out_fd, buffer, bytes_read);
      } else {
        // Write to stdout
        fwrite(buffer, 1, bytes_read, stdout);
      }
    }

    k_close(in_fd);
    arg_idx++;
  }

  if (out_fd >= 0) {
    k_close(out_fd);
  }
}

void cp(char** args) {
  // args[0] = "cp"
  // Usage: cp [-h] SOURCE DEST  OR  cp SOURCE -h DEST

  if (args[1] == NULL || args[2] == NULL) {
    s_printf("cp: missing file operand\n");
    s_printf("Usage: cp [-h] SOURCE DEST  OR  cp SOURCE -h DEST\n");
    return;
  }

  // Check filesystem is mounted
  if (_current_mount == NULL) {
    s_printf("cp: filesystem not mounted\n");
    return;
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
      return;
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
      return;
    }

    // Open destination file in filesystem
    int fs_fd = k_open(dest, F_WRITE);
    if (fs_fd < 0) {
      s_printf("cp: cannot create '%s' in filesystem\n", dest);
      fclose(host_file);
      return;
    }

    // Copy data
    char buffer[512];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), host_file)) > 0) {
      k_write(fs_fd, buffer, bytes_read);
    }

    fclose(host_file);
    k_close(fs_fd);
  } else if (fs_to_host) {
    // Copy from filesystem to host OS
    int fs_fd = k_open(source, F_READ);
    if (fs_fd == FILE_NOT_FOUND) {
      s_printf("cp: Error cannot copy '%s' as no such file exists\n", source);
      return;
    } else if (fs_fd < 0) {
      s_printf("cp: Error cannot open '%s' in filesystem\n", source);
      return;
    }

    // Open destination file on host
    FILE* host_file = fopen(dest, "wb");
    if (host_file == NULL) {
      s_printf("cp: cannot create host file '%s'\n", dest);
      k_close(fs_fd);
      return;
    }

    // Copy data
    char buffer[512];
    int bytes_read;
    while ((bytes_read = k_read(fs_fd, sizeof(buffer), buffer)) > 0) {
      fwrite(buffer, 1, bytes_read, host_file);
    }

    k_close(fs_fd);
    fclose(host_file);
  } else {
    // Copy within filesystem
    int src_fd = k_open(source, F_READ);
    if (src_fd == FILE_NOT_FOUND) {
      s_printf("cp: Error cannot copy '%s' as no such file\n", source);
      return;
    } else if (src_fd < 0) {
      s_printf("cp: Error cannot open '%s'\n", source);
      return;
    }

     // Check if destination already exists
    directory_entry_t dest_entry;
    if (find_file_in_directory(dest, &dest_entry) == 0) {
      int result = k_unlink(dest);
      if (result == -1) {
        s_printf("cp: cannot copy to '%s': file is currently open\n", dest);
        k_close(src_fd);
        return ;
      } else if (result < 0) {
        s_printf("cp: cannot copy to '%s'\n", dest);
        k_close(src_fd);
        return ;
      }
    }

    int dest_fd = k_open(dest, F_WRITE);
    if (dest_fd < 0) {
      s_printf("cp: cannot create '%s'\n", dest);
      k_close(src_fd);
      return;
    }

    // Copy data
    char buffer[512];
    int bytes_read;
    while ((bytes_read = k_read(src_fd, sizeof(buffer), buffer)) > 0) {
      k_write(dest_fd, buffer, bytes_read);
    }

    k_close(src_fd);
    k_close(dest_fd);
  }
}