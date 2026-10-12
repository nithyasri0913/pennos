/**
 * @file fd_table.h
 * @brief Global file descriptor table for PennFAT
 *
 * Manages open file descriptors across all processes. Each process has
 * a local FD table (fd_table[MAX_FDS]) that indexes into this global
 * table. Multiple processes can share file descriptors via fork/inheritance.
 */

#ifndef FD_TABLE_H_
#define FD_TABLE_H_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "fat_structs.h"

/**
 * @brief Maximum number of file descriptors per process
 */
#define MAX_FDS 16

/**
 * @brief File open modes (duplicated from fat_structs.h for convenience)
 */
#define F_WRITE 0   /**< Write mode */
#define F_READ 1    /**< Read mode */
#define F_APPEND 2  /**< Append mode */

/**
 * @brief Global file descriptor entry
 *
 * Represents an open file in the system. Shared across all processes
 * via reference counting. When a process forks, child inherits parent's
 * FD table, incrementing refcounts.
 */
typedef struct {
  char filename[32];    /**< Name of the open file */
  int mode;             /**< Open mode (F_WRITE, F_READ, F_APPEND) */
  off_t offset;         /**< Current file position for read/write */
  int refcount;         /**< Number of processes with this FD open */
  uint16_t first_block; /**< First block in FAT chain */
  uint32_t size;        /**< Current file size in bytes */
  uint8_t perm;         /**< File permissions */
} global_fd_entry_t;

/**
 * @brief Check if a file descriptor is in use
 *
 * Returns true if the global FD entry has a non-zero reference count.
 */
#define IS_FD_IN_USE(fd) (global_fd_table[fd].refcount > 0)

/**
 * @brief Global file descriptor table (MAX_OPEN_FILES entries)
 *
 * Shared across all processes in the system. Indexed by global FD number.
 */
extern global_fd_entry_t global_fd_table[MAX_OPEN_FILES];

/**
 * @brief Initialize the global file descriptor table
 *
 * Sets all entries to unused (refcount=0). Must be called once during
 * system initialization before any file operations.
 */
void init_global_fd_table();

/**
 * @brief Increment the reference count for a global FD
 *
 * Called when a process inherits or duplicates a file descriptor
 * (e.g., via fork).
 *
 * @param global_index Index into global_fd_table
 */
void increment_refcount(int global_index);

/**
 * @brief Decrement the reference count for a global FD
 *
 * Called when a process closes a file descriptor. If refcount reaches
 * zero, the entry becomes available for reuse.
 *
 * @param global_index Index into global_fd_table
 */
void decrement_refcount(int global_index);

#endif  // FD_TABLE_H_