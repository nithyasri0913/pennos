#include <stdio.h>
#include <stdarg.h>
#include "k_functions.h"
#include "kernel/kernel.h"
#include "../kernel/p_errno.h"

int s_open(const char* fname, int mode) {
  pcb_t* current_process = k_get_current_process();
  int fd = k_open(fname, mode);

  if (fd < 0) {
    return fd;
  }

  // Start from FD 3 to reserve 0, 1, 2 for stdin, stdout, stderr
  int pcb_fd = 3;

  for (; pcb_fd < MAX_FDS; pcb_fd++) {
    if (current_process->fd_table[pcb_fd] == -1) {
      current_process->fd_table[pcb_fd] = fd;
      break;
    }
  }

  if (pcb_fd == MAX_FDS) {
    k_close(fd);
    return -1;
  }

  return pcb_fd;
}

int s_close(int fd) {
  pcb_t* current_process = k_get_current_process();
  if (fd < 0 || fd >= MAX_FDS || current_process->fd_table[fd] == -1) {
    return -1;
  }

  int original_fd = current_process->fd_table[fd];
  current_process->fd_table[fd] = -1;
  return k_close(original_fd);
}

int s_read(int fd, int n, char* buf) {
  pcb_t* current_process = k_get_current_process();
  if (fd < 0 || fd >= MAX_FDS || current_process->fd_table[fd] == -1) {
    return -1;
  }

  int original_fd = current_process->fd_table[fd];
  return k_read(original_fd, n, buf);
}

int s_write(int fd, const char* str, int n) {
  pcb_t* current_process = k_get_current_process();

  // Handle early boot case where no process exists yet
  // For stdout/stderr, pass through directly to k_write
  if (current_process == NULL) {
    if (fd == 1 || fd == 2) {
      return k_write(fd, str, n);
    }
    return -1;
  }

  if (fd < 0 || fd >= MAX_FDS || current_process->fd_table[fd] == -1) {
    return -1;
  }

  int original_fd = current_process->fd_table[fd];
  return k_write(original_fd, str, n);
}

int s_unlink(const char* fname) {
  pcb_t* current_process = k_get_current_process();
  int fd = k_unlink(fname);
  if (fd < 0) {
    return fd;
  }
  current_process->fd_table[fd] = -1;
  return fd;
}

int s_lseek(int fd, int offset, int whence) {
  pcb_t* current_process = k_get_current_process();
  if (fd < 0 || fd >= MAX_FDS || current_process->fd_table[fd] == -1) {
    return -1;
  }
  return k_lseek(current_process->fd_table[fd], offset, whence);
}

int s_ls(const char* filename) {
  return k_ls(filename);
}

int s_printf(const char* format, ...) {
  char buffer[4096];
  va_list args;

  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len < 0) {
    return -1;  // Encoding error
  }

  if (len >= sizeof(buffer)) {
    // Output was truncated
    len = sizeof(buffer) - 1;
  }

  // Use k_write directly for stdout to bypass fd_table checks
  return k_write(1, buffer, len);
}

int s_fprintf_stderr(const char* format, ...) {
  char buffer[4096];
  va_list args;

  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len < 0) {
    return -1;  // Encoding error
  }

  if (len >= sizeof(buffer)) {
    // Output was truncated
    len = sizeof(buffer) - 1;
  }

  // Use k_write directly for stderr to bypass fd_table checks
  return k_write(2, buffer, len);
}

void fat_perror(const char* msg) {
  int err = get_fat_errno();
  const char* err_str = get_error_string(err);

  if (msg != NULL && msg[0] != '\0') {
    fprintf(stderr, "%s: %s\n", msg, err_str);
  } else {
    fprintf(stderr, "%s\n", err_str);
  }
}
