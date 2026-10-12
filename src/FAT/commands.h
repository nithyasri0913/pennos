/**
 * @file commands.h
 * @brief PennFAT filesystem commands and global state
 *
 * Provides functions for creating, mounting, and unmounting FAT filesystems.
 * Also exposes global state variables for the currently mounted filesystem.
 */

#ifndef FAT_COMMANDS_H_
#define FAT_COMMANDS_H_

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Global FAT table (in-memory copy)
 *
 * Array of 16-bit FAT entries. Each entry either:
 * - 0x0000: Free block
 * - 0xFFFF: End of chain
 * - Other value: Next block in chain
 */
extern uint16_t* _fat;

/**
 * @brief Block size in bytes (configured at mkfs time)
 */
extern int _block_size;

/**
 * @brief Number of entries in the FAT (number of data blocks)
 */
extern int _fat_size;

/**
 * @brief File handle for the currently mounted filesystem
 */
extern FILE* _current_mount;

/**
 * @brief File descriptor for the currently mounted filesystem
 */
extern int _fs_fd;

/**
 * @brief Create a new FAT filesystem
 *
 * Initializes a new FAT filesystem file with the specified parameters.
 * Creates the FAT table, initializes all blocks to free (0x0000), and
 * sets up an empty root directory.
 *
 * Block size options:
 * - 0: 256 bytes
 * - 1: 512 bytes
 * - 2: 1024 bytes
 * - 3: 2048 bytes
 * - 4: 4096 bytes
 *
 * @param fs_name Filename for the new filesystem
 * @param blocks_in_fat Number of data blocks in the FAT
 * @param block_size_config Block size configuration (0-4)
 */
void mkfs(const char* fs_name, int blocks_in_fat, int block_size_config);

/**
 * @brief Mount an existing FAT filesystem
 *
 * Opens the filesystem file and loads the FAT table into memory.
 * Sets up global state (_fat, _block_size, _current_mount, etc.).
 * Must be called before any file operations.
 *
 * @param fs_name Filename of the filesystem to mount
 * @return 0 on success, -1 on error
 */
int mount(const char* fs_name);

/**
 * @brief Unmount the currently mounted filesystem
 *
 * Writes the FAT table back to disk, closes the filesystem file, and
 * frees allocated memory. Should be called before program exit.
 *
 * @return 0 on success, -1 on error
 */
int unmount();

#endif  // FAT_COMMANDS_H_