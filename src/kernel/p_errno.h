#ifndef P_ERRNO_H
#define P_ERRNO_H

/**
 * @file p_errno.h
 * @brief PennOS error handling mechanism (similar to errno.h in POSIX)
 *
 * This file defines error codes that can be set by PennOS system calls
 * when they fail. Each process has its own p_errno field in its PCB.
 *
 * Usage:
 *   - System calls set P_ERRNO and return -1 on error
 *   - User programs check return value and call u_perror() to print error
 *
 * Example:
 *   if (s_kill(pid, P_SIGTERM) < 0) {
 *       u_perror("s_kill");  // Prints "s_kill: No such process"
 *   }
 */

/* ============================================================================
 * ERROR CODE DEFINITIONS
 * ============================================================================
 * Error codes are organized by category:
 * - 0: Success
 * - 1-9: Process management errors
 * - 10-19: File system errors
 * - 20-29: Memory/resource errors
 * - 30+: Other errors
 */

/** @brief No error occurred (success) */
#define P_SUCCESS 0

/* Process Management Errors (1-9) */

/** @brief Operation not permitted */
#define P_EPERM 1

/** @brief No such process */
#define P_ESRCH 2

/** @brief Invalid argument */
#define P_EINVAL 3

/** @brief No child processes */
#define P_ECHILD 4

/** @brief Interrupted system call */
#define P_EINTR 5

/** @brief Resource temporarily unavailable (try again) */
#define P_EAGAIN 6

/* File System Errors (10-19) */

/** @brief Filesystem not mounted */
#define P_ENOTMOUNTED 10

/** @brief No such file or directory */
#define P_ENOENT 11

/** @brief Device or resource busy (file already open) */
#define P_EBUSY 12

/** @brief Too many open files (no free file descriptors) */
#define P_EMFILE 13

/** @brief Bad file descriptor */
#define P_EBADF 14

/** @brief Invalid file open mode */
#define P_EINVALMODE 15

/** @brief File exists */
#define P_EEXIST 16

/** @brief Not a directory */
#define P_ENOTDIR 17

/** @brief Is a directory */
#define P_EISDIR 18

/* Memory/Resource Errors (20-29) */

/** @brief Out of memory / Cannot allocate memory */
#define P_ENOMEM 20

/** @brief No space left on device */
#define P_ENOSPC 21

/* Other Errors (30+) */

/** @brief Function not implemented */
#define P_ENOSYS 30

/** @brief Unknown error */
#define P_EUNKNOWN 99

/* ============================================================================
 * P_ERRNO ACCESS MACRO
 * ============================================================================
 * P_ERRNO provides access to the current process's error number.
 * This is implemented as a macro that calls get_p_errno().
 */

/**
 * @brief Get the error number for the current process
 *
 * This macro provides a convenient way to access the current process's
 * errno value, similar to the standard errno variable.
 *
 * @return Current process's error number
 */
#define P_ERRNO (get_p_errno())

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * @brief Get the error number for the current process
 *
 * @return Current process's p_errno value, or 0 if no current process
 */
int get_p_errno(void);

/**
 * @brief Set the error number for the current process
 *
 * System calls should use this function to set the error code when
 * they fail and return -1.
 *
 * @param err Error code to set (one of the P_E* constants)
 */
void set_p_errno(int err);

/**
 * @brief Print error message for the current P_ERRNO value
 *
 * Similar to perror(3) in POSIX. Prints the user-provided message
 * followed by ": " and a description of the current error.
 *
 * Format: "<msg>: <error_description>\n"
 *
 * Example:
 *   if (s_kill(pid, P_SIGTERM) < 0) {
 *       u_perror("Failed to kill process");
 *       // Output: "Failed to kill process: No such process"
 *   }
 *
 * @param msg User message to print before the error description
 */
void u_perror(const char* msg);

/**
 * @brief Get error message string for a given error code
 *
 * This is an internal helper function used by u_perror.
 *
 * @param err Error code
 * @return String description of the error
 */
const char* get_error_string(int err);

#endif  // P_ERRNO_H
