#include "logging.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../FAT/s_functions.h"

static int log_fd = -1;
static int current_ticks;

void logger_init(const char* filename) {
  log_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (log_fd == -1) {
    s_fprintf_stderr("ERROR: Failed to open log file: %s\n", filename);
    exit(1);
  }
  current_ticks = 0;
}

void logger_close(void) {
  if (log_fd != -1) {
    close(log_fd);
    log_fd = -1;
  }
}

int logger_get_ticks(void) {
  return current_ticks;
}

void logger_set_ticks(int ticks) {
  current_ticks = ticks;
}

// Schedule event
void log_schedule(int pid, int priority_queue, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tSCHEDULE\t%d\t%d\t%s\n",
                      current_ticks, pid, priority_queue, process_name);
    write(log_fd, buffer, len);
  }
}

// Process life-cycle events
void log_create(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tCREATE\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_signaled(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tSIGNALED\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_exited(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tEXITED\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_zombie(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tZOMBIE\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_orphan(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tORPHAN\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_waited(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tWAITED\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

// Nice event
void log_nice(int pid,
              int old_nice_value,
              int new_nice_value,
              const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tNICE\t\t%d\t%d\t%d\t%s\n",
                      current_ticks, pid, old_nice_value, new_nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

// Process blocked/unblocked
void log_blocked(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tBLOCKED\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_unblocked(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tUNBLOCKED\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

// Process stopped/continued
void log_stopped(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tSTOPPED\t\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}

void log_continued(int pid, int nice_value, const char* process_name) {
  if (log_fd != -1) {
    char buffer[512];
    int len = sprintf(buffer, "[%d]\tCONTINUED\t%d\t%d\t%s\n",
                      current_ticks, pid, nice_value, process_name);
    write(log_fd, buffer, len);
  }
}