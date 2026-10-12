#include "./redirection.h"
#include "../FAT/s_functions.h"
#include "../FAT/fat_structs.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Get redirected file descriptors without modifying current process
void get_redirected_fds(struct parsed_command* cmd, int* fd_in, int* fd_out) {
  *fd_in = -1;   // default to no redirection
  *fd_out = -1;  // default to no redirection

  // Check if stdin and stdout refer to the same file
  int same_file = 0;
  if (cmd->stdin_file != NULL && cmd->stdout_file != NULL) {
    if (strcmp(cmd->stdin_file, cmd->stdout_file) == 0) {
      same_file = 1;
    }
  }

  // If same file, we need special handling:
  // - For append mode (>>), this is an error - input and output cannot be the same when appending
  // - For write mode (>), open for output first (which truncates), making the file empty,
  //   then open for input (which reads the now-empty file)
  // This matches standard Unix behavior

  if (same_file) {
    if (cmd->is_file_append) {
      // Error: cannot append to the same file we're reading from
      s_fprintf_stderr( "cat: input and output cannot be the same when appending\n");
      // Set both to -1 to signal error
      *fd_in = -1;
      *fd_out = -1;
      return;
    }

    // Open output first to truncate
    *fd_out = s_open(cmd->stdout_file, F_WRITE);
    if (*fd_out < 0) {
      s_fprintf_stderr( "redirection: cannot open '%s' for output\n", cmd->stdout_file);
      *fd_out = STDOUT_FILENO;
    }

    // Now open for input (file is already truncated)
    *fd_in = s_open(cmd->stdin_file, F_READ);
    if (*fd_in < 0) {
      s_fprintf_stderr( "redirection: cannot open '%s' for input\n", cmd->stdin_file);
      *fd_in = STDIN_FILENO;
    }
  } else {
    // Normal case: different files or only one specified

    // Open stdin file if specified
    if (cmd->stdin_file != NULL) {
      *fd_in = s_open(cmd->stdin_file, F_READ);
      if (*fd_in < 0) {
        s_fprintf_stderr( "redirection: cannot open '%s' for input\n", cmd->stdin_file);
        *fd_in = STDIN_FILENO;
      }
    }

    // Open stdout file if specified
    if (cmd->stdout_file != NULL) {
      int mode = cmd->is_file_append ? F_APPEND : F_WRITE;
      *fd_out = s_open(cmd->stdout_file, mode);
      if (*fd_out < 0) {
        s_fprintf_stderr( "redirection: cannot open '%s' for output\n", cmd->stdout_file);
        *fd_out = STDOUT_FILENO;
      }
    }
  }
}