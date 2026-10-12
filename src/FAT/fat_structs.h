/**
 * @file fat_structs.h
 * @brief PennFAT filesystem data structures and constants
 *
 * Defines the core data structures, file modes, permissions, and error
 * codes for the PennFAT filesystem implementation.
 */

#ifndef FAT_STRUCTS_H_
#define FAT_STRUCTS_H_

#include <stdint.h>
#include <time.h>

/**
 * @brief File open modes
 */
#define F_WRITE 0   /**< Write mode - truncate file if exists, create if not */
#define F_READ 1    /**< Read mode - file must exist */
#define F_APPEND 2  /**< Append mode - create if not exists, seek to end */

/**
 * @brief File seek reference points (whence parameter for lseek)
 */
#define F_SEEK_SET 0  /**< Seek from beginning of file */
#define F_SEEK_CUR 1  /**< Seek from current position */
#define F_SEEK_END 2  /**< Seek from end of file */

/**
 * @brief File permission bits (Unix-style)
 */
#define PERM_NONE 0         /**< No permissions */
#define PERM_WRITE 2        /**< Write permission */
#define PERM_READ 4         /**< Read permission */
#define PERM_READ_EXEC 5    /**< Read and execute permissions */
#define PERM_READ_WRITE 6   /**< Read and write permissions */
#define PERM_ALL 7          /**< All permissions (read, write, execute) */

/**
 * @brief File type identifiers
 */
#define TYPE_UNKNOWN 0    /**< Unknown file type */
#define TYPE_REGULAR 1    /**< Regular file */
#define TYPE_DIRECTORY 2  /**< Directory (not fully supported) */
#define TYPE_SYMLINK 4    /**< Symbolic link (not fully supported) */

/**
 * @brief Maximum number of simultaneously open files in the system
 */
#define MAX_OPEN_FILES 256

/**
 * @brief PennFAT error codes
 */
#define FS_NOT_MOUNTED -1      /**< No filesystem is currently mounted */
#define FILE_NOT_FOUND -2      /**< Requested file does not exist */
#define FILE_ALREADY_OPEN -3   /**< File is already open */
#define NO_FREE_FD -4          /**< No free file descriptors available */
#define INVALID_MODE -5        /**< Invalid file open mode specified */

/**
 * @brief Directory entry structure (64 bytes)
 *
 * Represents a file's metadata in the root directory. Each directory
 * occupies exactly one block and contains fixed-size entries.
 */
typedef struct {
  char name[32];        /**< Filename (null-terminated) */
  uint32_t size;        /**< File size in bytes */
  uint16_t firstBlock;  /**< First block in FAT chain (0xFFFF if empty) */
  uint8_t type;         /**< File type (TYPE_REGULAR, TYPE_DIRECTORY, etc.) */
  uint8_t perm;         /**< Permission bits */
  time_t mtime;         /**< Last modification time (Unix timestamp) */
  char reserved[16];    /**< Reserved for future use (padding to 64 bytes) */
} directory_entry_t;

#endif  // FAT_STRUCTS_H_