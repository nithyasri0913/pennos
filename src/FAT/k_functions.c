#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "commands.h"
#include "fat_structs.h"
#include "fd_table.h"
#include "FAT/s_functions.h"
#include "../kernel/p_errno.h"

// Global errno for non-process contexts (pennfat shell)
// This is used when there's no PennOS process (no PCB)
static int fat_global_errno = 0;

/**
 * @brief Set errno for both process and global contexts
 *
 * This function sets errno in both:
 * 1. Current process's PCB (for PennOS processes)
 * 2. Global errno (for pennfat shell)
 *
 * This ensures error handling works in both pennos and pennfat.
 */
static void set_fat_errno(int err) {
  fat_global_errno = err;  // Always set global errno
  set_p_errno(err);         // Also set process errno (if process exists)
}

/**
 * @brief Get errno, checking global errno first
 *
 * Returns global errno for non-process contexts (pennfat),
 * or process errno for PennOS processes.
 */
int get_fat_errno(void) {
  return fat_global_errno;
}

// Forward declarations
int update_directory_entry(const char* fname, directory_entry_t* updated_entry);

// find free block in FAT
// starting with 2 as 0 is metadat and 1 is root dir
int allocate_block() {
  for (int i = 2; i < (_fat_size / 2); i++) {
    if (_fat[i] == 0) {
      // Zero out the block to avoid garbage data
      char* zero_block = calloc(1, _block_size);
      if (zero_block != NULL) {
        off_t block_offset = _fat_size + _block_size * (i - 1);
        lseek(_fs_fd, block_offset, SEEK_SET);
        write(_fs_fd, zero_block, _block_size);
        free(zero_block);
      }
      return i;
    }
  }
  return -1;
}

void print_directory_entry(directory_entry_t* entry) {
  char buffer[256];
  int len;

  len = snprintf(buffer, sizeof(buffer), "name: %s\n", entry->name);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "size: %u\n", entry->size);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "firstBlock: %u\n", entry->firstBlock);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "type: %u\n", entry->type);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "perm: %u\n", entry->perm);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "mtime: %ld\n", entry->mtime);
  write(STDOUT_FILENO, buffer, len);
}

void print_fat_table() {
  char buffer[256];
  int len;
  int num_entries = _fat_size / 2;

  len = snprintf(buffer, sizeof(buffer), "=== FAT Table ===\n");
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "Total entries: %d\n", num_entries);
  write(STDOUT_FILENO, buffer, len);

  len = snprintf(buffer, sizeof(buffer), "FAT size: %d bytes\n", _fat_size);
  write(STDOUT_FILENO, buffer, len);

  len =
      snprintf(buffer, sizeof(buffer), "Block size: %d bytes\n\n", _block_size);
  write(STDOUT_FILENO, buffer, len);

  for (int i = 0; i < num_entries; i++) {
    if (_fat[i] != 0) {
      if (_fat[i] == 0xFFFF) {
        len = snprintf(buffer, sizeof(buffer), "FAT[%d] = %u (EOF)\n", i,
                       _fat[i]);
      } else {
        len = snprintf(buffer, sizeof(buffer),
                       "FAT[%d] = %u (next block: %u)\n", i, _fat[i], _fat[i]);
      }
      write(STDOUT_FILENO, buffer, len);
    }
  }

  len = snprintf(buffer, sizeof(buffer), "\n");
  write(STDOUT_FILENO, buffer, len);
}

int find_file_in_directory(const char* fname, directory_entry_t* entry) {
  int entries_per_block = _block_size / sizeof(directory_entry_t);
  int current_block = 1;

  while (current_block != 0xFFFF) {
    directory_entry_t* entries = malloc(_block_size);
    if (entries == NULL) {
      return -1;
    }

    // block position
    off_t block_offset = _fat_size + _block_size * (current_block - 1);

    // read block from disk
    lseek(_fs_fd, block_offset, SEEK_SET);
    ssize_t bytes_read = read(_fs_fd, entries, _block_size);
    if (bytes_read != _block_size) {
      free(entries);
      return -1;
    }

    for (int i = 0; i < entries_per_block; i++) {
      // end of directory
      if (entries[i].name[0] == 0) {
        free(entries);
        return -1;
      }

      // deleted entry
      else if (entries[i].name[0] == 1 || entries[i].name[0] == 2) {
        continue;
      }

      // valid entry
      else if (strcmp(entries[i].name, fname) == 0) {
        *entry = entries[i];
        free(entries);
        return 0;
      }
    }
    free(entries);
    current_block = _fat[current_block];
  }
  return -1;
}

// returns fd number  or -1 if no free fds
int allocate_fd() {
  // have to start at 3 (skip stdin/stdout/stderr)
  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    if (!IS_FD_IN_USE(i)) {
      return i;
    }
  }
  return -1;
}

int create_directory_entry_at_block(int block,
                                    const char* fname,
                                    directory_entry_t* entry) {
  int entries_per_block = _block_size / sizeof(directory_entry_t);
  directory_entry_t* entries = malloc(_block_size);

  if (entries == NULL) {
    return -1;
  }

  off_t block_offset = _fat_size + _block_size * (block - 1);

  // read block from disk
  lseek(_fs_fd, block_offset, SEEK_SET);
  ssize_t bytes_read = read(_fs_fd, entries, _block_size);
  if (bytes_read != _block_size) {
    free(entries);
    return -1;
  }

  for (int i = 0; i < entries_per_block; i++) {
    // found free slot
    if (entries[i].name[0] == 0 || entries[i].name[0] == 1) {
      // fill in the new entry
      strncpy(entries[i].name, fname, 31);
      entries[i].name[31] = '\0';
      entries[i].size = 0;
      entries[i].firstBlock = 0;
      entries[i].type = TYPE_REGULAR;
      entries[i].perm = PERM_READ_WRITE;
      entries[i].mtime = time(NULL);

      // write block back to disk
      lseek(_fs_fd, block_offset, SEEK_SET);
      ssize_t written = write(_fs_fd, entries, _block_size);

      if (written != _block_size) {
        free(entries);
        return -1;
      }

      *entry = entries[i];
      free(entries);
      return 0;
    }
  }
  free(entries);
  return -1;
}
// creates a new directory entry for a file
// Returns=> 0 on success, -1 on error
int create_directory_entry(const char* fname, directory_entry_t* entry) {
  int current_block = 1;
  int last_block = 1;

  while (current_block != 0xFFFF) {
    last_block = current_block;
    if (create_directory_entry_at_block(current_block, fname, entry) == 0) {
      return 0;
    }
    current_block = _fat[current_block];
  }
  // No free entry was found in currently allocated blocks
  int new_block = allocate_block();

  char buffer[64];
  int len = snprintf(buffer, sizeof(buffer),
                     "Allocated new block for directory: %d\n", new_block);
  write(STDOUT_FILENO, buffer, len);

  if (new_block < 0) {
    return -1;
  }
  if (create_directory_entry_at_block(new_block, fname, entry) == 0) {
    _fat[last_block] = new_block;
    _fat[new_block] = 0xFFFF;
    return 0;
  }
  return -1;
}

int k_open(const char* fname, int mode) {
  if (_current_mount == NULL) {
    set_fat_errno(P_ENOTMOUNTED);  // Filesystem not mounted
    return -1;
  }

  if (mode != F_WRITE && mode != F_READ && mode != F_APPEND) {
    set_fat_errno(P_EINVALMODE);  // Invalid file open mode
    return -1;
  }

  directory_entry_t dir_entry;
  int found = find_file_in_directory(fname, &dir_entry);

  // diff modes
  if (mode == F_READ) {
    if (found < 0) {
      set_fat_errno(P_ENOENT);  // No such file or directory
      return -1;
    }
  } else if (mode == F_WRITE) {
    if (found == 0) {
      // truncate existing file
      int block = dir_entry.firstBlock;
      while (block != 0 && block != 0xFFFF) {
        int next_block = _fat[block];
        _fat[block] = 0;
        block = next_block;
      }
      dir_entry.size = 0;
      dir_entry.firstBlock = 0;
      dir_entry.mtime = time(NULL);

      // Write updated directory entry to disk
      if (update_directory_entry(fname, &dir_entry) < 0) {
        return -1;
      }
    } else {
      // create new file
      if (create_directory_entry(fname, &dir_entry) < 0) {
        return -1;
      }
    }
  } else if (mode == F_APPEND) {
    if (found < 0) {
      // create new file
      if (create_directory_entry(fname, &dir_entry) < 0) {
        return -1;
      }
    }
  }

  // check if already open
  if (mode == F_WRITE || mode == F_APPEND) {
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
      if (IS_FD_IN_USE(i) && strcmp(global_fd_table[i].filename, fname) == 0 &&
          (global_fd_table[i].mode == F_WRITE ||
           global_fd_table[i].mode == F_APPEND)) {
        set_fat_errno(P_EBUSY);  // Device or resource busy
        return -1;
      }
    }
  }

  // allocate fd
  int fd = allocate_fd();
  if (fd < 0) {
    set_fat_errno(P_EMFILE);  // Too many open files
    return -1;
  }

  // fill global_fd_table
  strncpy(global_fd_table[fd].filename, fname, 31);
  global_fd_table[fd].filename[31] = '\0';
  global_fd_table[fd].mode = mode;
  global_fd_table[fd].first_block = dir_entry.firstBlock;
  global_fd_table[fd].size = dir_entry.size;
  global_fd_table[fd].perm = dir_entry.perm;
  global_fd_table[fd].refcount = 1;

  if (mode == F_APPEND) {
    global_fd_table[fd].offset = dir_entry.size;
  } else {
    global_fd_table[fd].offset = 0;
  }

  return fd;
}

// update directory entry on disk
int update_directory_entry(const char* fname,
                           directory_entry_t* updated_entry) {
  int entries_per_block = _block_size / sizeof(directory_entry_t);
  int current_block = 1;

  while (current_block != 0xFFFF) {
    directory_entry_t* entries = malloc(_block_size);
    off_t block_offset = _fat_size + _block_size * (current_block - 1);

    lseek(_fs_fd, block_offset, SEEK_SET);
    read(_fs_fd, entries, _block_size);

    for (int i = 0; i < entries_per_block; i++) {
      if (entries[i].name[0] == 0) {
        free(entries);
        return -1;
      }

      if (entries[i].name[0] != 1 && entries[i].name[0] != 2 &&
          strcmp(entries[i].name, fname) == 0) {
        // if found update back
        entries[i] = *updated_entry;

        lseek(_fs_fd, block_offset, SEEK_SET);
        write(_fs_fd, entries, _block_size);
        free(entries);
        return 0;
      }
    }

    free(entries);
    current_block = _fat[current_block];
  }

  return -1;
}

int k_write(int fd, const char* str, int n) {
  if (fd == 1 || fd == 2) {
    
    write(fd, str, n);
    return n;
  };
  if (fd < 0 || fd >= MAX_OPEN_FILES || !IS_FD_IN_USE(fd)) {
    return -1;
  }

  if (global_fd_table[fd].mode == F_READ) {
    return -1;
  }

  off_t offset = global_fd_table[fd].offset;
  int bytes_written = 0;

  while (bytes_written < n) {
    // calculate which block and offset within block
    int block_index = offset / _block_size;
    int block_offset = offset % _block_size;
    int bytes_to_write = _block_size - block_offset;
    if (bytes_to_write > n - bytes_written) {
      bytes_to_write = n - bytes_written;
    }

    // find or allocate block
    int current_block;
    if (block_index == 0 && global_fd_table[fd].first_block == 0) {
      // allocate first block
      current_block = allocate_block();
      if (current_block < 0)
        return bytes_written;

      global_fd_table[fd].first_block = current_block;
      _fat[current_block] = 0xFFFF;
    } else {
      // navigate to block
      current_block = global_fd_table[fd].first_block;
      for (int i = 0; i < block_index; i++) {
        if (_fat[current_block] == 0xFFFF) {
          // new block
          int new_block = allocate_block();
          if (new_block < 0)
            return bytes_written;
          _fat[current_block] = new_block;
          _fat[new_block] = 0xFFFF;
          current_block = new_block;
        } else {
          current_block = _fat[current_block];
        }
      }
    }

    char* block_buffer = malloc(_block_size);
    off_t disk_offset = _fat_size + _block_size * (current_block - 1);
    lseek(_fs_fd, disk_offset, SEEK_SET);
    read(_fs_fd, block_buffer, _block_size);

    memcpy(block_buffer + block_offset, str + bytes_written, bytes_to_write);

    lseek(_fs_fd, disk_offset, SEEK_SET);
    write(_fs_fd, block_buffer, _block_size);
    free(block_buffer);

    bytes_written += bytes_to_write;
    offset += bytes_to_write;
  }

  // update offset after write
  global_fd_table[fd].offset = offset;
  if (offset > global_fd_table[fd].size) {
    global_fd_table[fd].size = offset;
  }

  directory_entry_t updated_entry;
  if (find_file_in_directory(global_fd_table[fd].filename, &updated_entry) ==
      0) {
    updated_entry.size = global_fd_table[fd].size;
    updated_entry.firstBlock = global_fd_table[fd].first_block;
    updated_entry.mtime = time(NULL);
    update_directory_entry(global_fd_table[fd].filename, &updated_entry);
  }

  return bytes_written;
}

int k_read(int fd, int n, char* buf) {
  if (fd < 0 || fd >= MAX_OPEN_FILES || !IS_FD_IN_USE(fd)) {
    return -1;
  }

  off_t offset = global_fd_table[fd].offset;
  uint32_t file_size = global_fd_table[fd].size;

  // check EOF
  if (offset >= file_size) {
    return 0;
  }

  // don't read past EOF
  if (offset + n > file_size) {
    n = file_size - offset;
  }

  int bytes_read = 0;
  int current_block = global_fd_table[fd].first_block;

  if (current_block == 0) {
    return 0;
  }

  // skip to starting block
  int block_index = offset / _block_size;
  for (int i = 0; i < block_index; i++) {
    current_block = _fat[current_block];
    if (current_block == 0xFFFF) {
      return bytes_read;
    }
  }

  int block_offset = offset % _block_size;

  while (bytes_read < n && current_block != 0xFFFF) {
    // read block
    char* block_buffer = malloc(_block_size);
    off_t disk_offset = _fat_size + _block_size * (current_block - 1);
    lseek(_fs_fd, disk_offset, SEEK_SET);
    read(_fs_fd, block_buffer, _block_size);

    int bytes_to_read = _block_size - block_offset;
    if (bytes_to_read > n - bytes_read) {
      bytes_to_read = n - bytes_read;
    }

    memcpy(buf + bytes_read, block_buffer + block_offset, bytes_to_read);
    free(block_buffer);

    bytes_read += bytes_to_read;
    block_offset = 0;
    current_block = _fat[current_block];
  }

  global_fd_table[fd].offset += bytes_read;
  return bytes_read;
}

int k_close(int fd) {
  if (fd < 0 || fd >= MAX_OPEN_FILES || !IS_FD_IN_USE(fd)) {
    return -1;
  }

  if (global_fd_table[fd].mode == F_WRITE ||
      global_fd_table[fd].mode == F_APPEND) {
    directory_entry_t updated_entry;
    if (find_file_in_directory(global_fd_table[fd].filename, &updated_entry) ==
        0) {
      updated_entry.size = global_fd_table[fd].size;
      updated_entry.firstBlock = global_fd_table[fd].first_block;
      updated_entry.mtime = time(NULL);
      update_directory_entry(global_fd_table[fd].filename, &updated_entry);
    }
  }

  global_fd_table[fd].refcount--;
  if (global_fd_table[fd].refcount <= 0) {
    global_fd_table[fd].refcount = 0;
  }

  return 0;
}

int k_lseek(int fd, int offset, int whence) {
  if (fd < 0 || fd >= MAX_OPEN_FILES || !IS_FD_IN_USE(fd)) {
    return -1;
  }

  off_t new_offset;

  if (whence == F_SEEK_SET) {
    new_offset = offset;
  } else if (whence == F_SEEK_CUR) {
    new_offset = global_fd_table[fd].offset + offset;
  } else if (whence == F_SEEK_END) {
    new_offset = global_fd_table[fd].size + offset;
  } else {
    return -1;
  }

  if (new_offset < 0) {
    return -1;
  }

  global_fd_table[fd].offset = new_offset;
  return new_offset;
}

int k_ls(const char* filename) {
  if (_current_mount == NULL) {
    set_fat_errno(P_ENOTMOUNTED);  // Filesystem not mounted
    return -1;
  }

  int entries_per_block = _block_size / sizeof(directory_entry_t);
  // root dir
  int current_block = 1;
  int found = 0;

  while (current_block != 0xFFFF) {
    directory_entry_t* entries = malloc(_block_size);
    if (entries == NULL) {
      return -1;
    }

    off_t block_offset = _fat_size + _block_size * (current_block - 1);
    lseek(_fs_fd, block_offset, SEEK_SET);
    read(_fs_fd, entries, _block_size);

    for (int i = 0; i < entries_per_block; i++) {
      // end of directory
      if (entries[i].name[0] == 0) {
        free(entries);
        return found ? 0 : -1;
      }

      // skip deleted entries
      if (entries[i].name[0] == 1 || entries[i].name[0] == 2) {
        continue;
      }

      // if filename specified check if it matches
      if (filename != NULL && strcmp(entries[i].name, filename) != 0) {
        continue;
      }

      found = 1;

      // format permissions
      char perm_str[4] = "---";
      if (entries[i].perm & 4)
        perm_str[0] = 'r';
      if (entries[i].perm & 2)
        perm_str[1] = 'w';
      if (entries[i].perm & 1)
        perm_str[2] = 'x';

      // format time
      struct tm* timeinfo = localtime(&entries[i].mtime);
      char time_str[20];
      strftime(time_str, 20, "%b %d %H:%M", timeinfo);

      // first_block perms size time name
      char line_buffer[256];
      int line_len = sprintf(line_buffer, "%4u -%s %8u %s %s\n",
                             entries[i].firstBlock, perm_str,
                             entries[i].size, time_str, entries[i].name);
      write(STDOUT_FILENO, line_buffer, line_len);

      // if specific file requestedne
      if (filename != NULL) {
        free(entries);
        return 0;
      }
    }

    free(entries);
    current_block = _fat[current_block];
  }

  return found ? 0 : -1;
}

int k_unlink(const char* fname) {
  if (_current_mount == NULL) {
    set_fat_errno(P_ENOTMOUNTED);  // Filesystem not mounted
    return -1;
  }

  // check if file is open (refcount > 0)
  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    if (IS_FD_IN_USE(i) && strcmp(global_fd_table[i].filename, fname) == 0) {
      // file is open so cant delete
      return -1;
    }
  }

  // find file in dir
  directory_entry_t entry;
  if (find_file_in_directory(fname, &entry) < 0) {
    set_fat_errno(P_ENOENT);  // No such file or directory
    return -1;
  }

  // free blocks in fat
  int block = entry.firstBlock;
  while (block != 0 && block != 0xFFFF) {
    int next_block = _fat[block];
    _fat[block] = 0;
    block = next_block;
  }

  // mark directory entry as deleted
  entry.name[0] = 1;
  update_directory_entry(fname, &entry);

  return 0;
}