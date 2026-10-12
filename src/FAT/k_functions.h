#ifndef K_FUNCTIONS_H_
#define K_FUNCTIONS_H_

#include "commands.h"
#include "fat_structs.h"

/**
 * @brief Find a file in the directory and retrieve its entry
 *
 * Searches the root directory for a file with the specified name and
 * populates the entry structure with its metadata if found.
 *
 * @param fname Filename to search for
 * @param entry Output parameter for directory entry
 * @return 0 on success, -1 if file not found
 */
int find_file_in_directory(const char* fname, directory_entry_t* entry);

/**
 * @brief Allocate a new file descriptor
 *
 * Finds an available slot in the global file descriptor table and
 * returns its index.
 *
 * @return File descriptor number, or -1 if no slots available
 */
int allocate_fd();

/**
 * @brief Create a new directory entry for a file
 *
 * Allocates space in the root directory and initializes a new entry
 * with the specified filename. Sets initial metadata (size=0, type,
 * permissions, modification time).
 *
 * @param fname Filename for the new entry
 * @param entry Output parameter for the created entry
 * @return 0 on success, -1 on failure
 */
int create_directory_entry(const char* fname, directory_entry_t* entry);

/**
 * @brief Update an existing directory entry
 *
 * Writes the updated entry back to the directory on disk. Used to
 * persist changes to file metadata (size, permissions, mtime, etc.).
 *
 * @param fname Filename of entry to update
 * @param updated_entry New entry data to write
 * @return 0 on success, -1 on failure
 */
int update_directory_entry(const char* fname, directory_entry_t* updated_entry);

/**
 * @brief Open a file in the FAT filesystem
 *
 * Opens the specified file with the given mode (F_WRITE, F_READ, F_APPEND).
 * Creates the file if it doesn't exist and mode is F_WRITE or F_APPEND.
 * Returns a file descriptor for subsequent operations.
 *
 * @param fname Filename to open
 * @param mode Open mode: F_WRITE (0), F_READ (1), or F_APPEND (2)
 * @return File descriptor number, or -1 on failure
 */
int k_open(const char* fname, int mode);

/**
 * @brief Allocate a free block from the FAT
 *
 * Scans the FAT for an available block (value 0) and marks it as
 * end-of-chain (0xFFFF).
 *
 * @return Block number, or -1 if no free blocks available
 */
int allocate_block();

/**
 * @brief Write data to an open file
 *
 * Writes up to n bytes from str to the file associated with fd.
 * Allocates new blocks as needed. Updates file size in directory entry.
 *
 * @param fd File descriptor
 * @param str Buffer containing data to write
 * @param n Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
int k_write(int fd, const char* str, int n);

/**
 * @brief Read data from an open file
 *
 * Reads up to n bytes from the file associated with fd into buf.
 * Respects file size and current offset.
 *
 * @param fd File descriptor
 * @param n Maximum number of bytes to read
 * @param buf Buffer to store read data
 * @return Number of bytes read, or -1 on error
 */
int k_read(int fd, int n, char* buf);

/**
 * @brief Close an open file descriptor
 *
 * Releases the file descriptor and updates the reference count in the
 * global FD table.
 *
 * @param fd File descriptor to close
 * @return 0 on success, -1 on error
 */
int k_close(int fd);

/**
 * @brief Reposition the file offset
 *
 * Changes the current position in the file for subsequent read/write
 * operations. Supports SEEK_SET, SEEK_CUR, and SEEK_END.
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence Reference point: F_SEEK_SET (0), F_SEEK_CUR (1), F_SEEK_END (2)
 * @return New absolute offset, or -1 on error
 */
int k_lseek(int fd, int offset, int whence);

/**
 * @brief List directory contents or show file info
 *
 * If filename is NULL or empty, lists all files in the directory.
 * Otherwise, displays detailed information about the specific file.
 *
 * @param filename Specific file to query, or NULL to list all
 * @return 0 on success, -1 on error
 */
int k_ls(const char* filename);

/**
 * @brief Delete a file from the filesystem
 *
 * Removes the file's directory entry and frees all blocks in the FAT
 * chain. File must not be currently open.
 *
 * @param fname Filename to delete
 * @return 0 on success, -1 on error
 */
int k_unlink(const char* fname);

/**
 * @brief Print a directory entry in human-readable format
 *
 * Displays filename, size, permissions, and modification time.
 *
 * @param entry Directory entry to print
 */
void print_directory_entry(directory_entry_t* entry);

/**
 * @brief Print the entire FAT table for debugging
 *
 * Displays all FAT entries showing block allocation chains.
 */
void print_fat_table();

/**
 * @brief Get the global FAT errno value
 *
 * Returns the global error number set by FAT filesystem operations.
 * This is used in contexts where there is no PennOS process (e.g., pennfat shell).
 *
 * @return Global FAT error number
 */
int get_fat_errno(void);

#endif  // K_FUNCTIONS_H_
