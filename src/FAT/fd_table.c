#include "fd_table.h"
#include <stdio.h>
#include <string.h>

// Global file descriptor table
global_fd_entry_t global_fd_table[MAX_OPEN_FILES];

void init_global_fd_table() {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    global_fd_table[i].refcount = 0;
  }

  global_fd_table[0].refcount = 1;
  global_fd_table[1].refcount = 1;
  global_fd_table[2].refcount = 1;
  strcpy(global_fd_table[0].filename, "stdin");
  strcpy(global_fd_table[1].filename, "stdout");
  strcpy(global_fd_table[2].filename, "stderr");
}

void increment_refcount(int global_index) {
  if (global_index >= 0 && global_index < MAX_OPEN_FILES) {
    global_fd_table[global_index].refcount++;
  }
}

void decrement_refcount(int global_index) {
  if (global_index >= 0 && global_index < MAX_OPEN_FILES) {
    global_fd_table[global_index].refcount--;
    // re-setting
    if (global_fd_table[global_index].refcount <= 0) {
      global_fd_table[global_index].refcount = 0;
    }
  }
}