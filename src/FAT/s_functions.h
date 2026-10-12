#ifndef S_FUNCTIONS_H_
#define S_FUNCTIONS_H_

/**
 * @brief User-level file open system call
 *
 * Opens a file in the FAT filesystem for the current process. Uses the
 * process's file descriptor table. Inherits file descriptors across fork.
 *
 * @param fname Filename to open
 * @param mode Open mode: F_WRITE (0), F_READ (1), or F_APPEND (2)
 * @return File descriptor number, or -1 on error
 */
int s_open(const char* fname, int mode);

/**
 * @brief User-level file read system call
 *
 * Reads up to n bytes from the file into buf. Uses the current process's
 * file descriptor table.
 *
 * @param fd File descriptor
 * @param n Maximum number of bytes to read
 * @param buf Buffer to store read data
 * @return Number of bytes read, or -1 on error
 */
int s_read(int fd, int n, char* buf);

/**
 * @brief User-level file write system call
 *
 * Writes up to n bytes from str to the file. Uses the current process's
 * file descriptor table. Special handling for stdout (fd=1) and stderr (fd=2).
 *
 * @param fd File descriptor
 * @param str Buffer containing data to write
 * @param n Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
int s_write(int fd, const char* str, int n);

/**
 * @brief User-level file close system call
 *
 * Closes the file descriptor in the current process's FD table. Decrements
 * the reference count in the global FD table.
 *
 * @param fd File descriptor to close
 * @return 0 on success, -1 on error
 */
int s_close(int fd);

/**
 * @brief User-level file deletion system call
 *
 * Deletes the specified file from the FAT filesystem. File must not be
 * currently open by any process.
 *
 * @param fname Filename to delete
 * @return 0 on success, -1 on error
 */
int s_unlink(const char* fname);

/**
 * @brief User-level file seek system call
 *
 * Repositions the file offset for subsequent read/write operations.
 * Uses the current process's file descriptor table.
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence Reference point: F_SEEK_SET (0), F_SEEK_CUR (1), F_SEEK_END (2)
 * @return New absolute offset, or -1 on error
 */
int s_lseek(int fd, int offset, int whence);

/**
 * @brief User-level directory listing system call
 *
 * Lists directory contents or shows information about a specific file.
 * If filename is NULL or empty, lists all files.
 *
 * @param filename Specific file to query, or NULL to list all
 * @return 0 on success, -1 on error
 */
int s_ls(const char* filename);

/**
 * @brief User-level printf to stdout
 *
 * Formatted output function that writes to the current process's stdout
 * (fd=1) using k_write. Supports standard printf format specifiers.
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return Number of characters written, or -1 on error
 */
int s_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief User-level fprintf to stderr
 *
 * Formatted output function that writes to the current process's stderr
 * (fd=2) using k_write. Supports standard printf format specifiers.
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return Number of characters written, or -1 on error
 */
int s_fprintf_stderr(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Print error message using global FAT errno
 *
 * Similar to u_perror(), but uses the global FAT errno instead of per-process errno.
 * This is useful in contexts where there is no PennOS process (e.g., pennfat shell).
 * Prints to stderr in the format: "msg: error_description\n"
 *
 * @param msg User message to print before the error description
 */
void fat_perror(const char* msg);

#endif  // S_FUNCTIONS_H_
