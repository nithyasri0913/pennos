#include "p_errno.h"
#include <stdio.h>

/**
 * @brief Get error message string for a given error code
 *
 * This function returns a human-readable string describing the error
 * corresponding to the given error code.
 *
 * @param err Error code (one of the P_E* constants)
 * @return String description of the error
 */
const char* get_error_string(int err) {
  switch (err) {
    case P_SUCCESS:
      return "Success";

    // Process Management Errors (1-9)
    case P_EPERM:
      return "Operation not permitted";
    case P_ESRCH:
      return "No such process";
    case P_EINVAL:
      return "Invalid argument";
    case P_ECHILD:
      return "No child processes";
    case P_EINTR:
      return "Interrupted system call";
    case P_EAGAIN:
      return "Resource temporarily unavailable";

    // File System Errors (10-19)
    case P_ENOTMOUNTED:
      return "Filesystem not mounted";
    case P_ENOENT:
      return "No such file or directory";
    case P_EBUSY:
      return "Device or resource busy";
    case P_EMFILE:
      return "Too many open files";
    case P_EBADF:
      return "Bad file descriptor";
    case P_EINVALMODE:
      return "Invalid file open mode";
    case P_EEXIST:
      return "File exists";
    case P_ENOTDIR:
      return "Not a directory";
    case P_EISDIR:
      return "Is a directory";

    // Memory/Resource Errors (20-29)
    case P_ENOMEM:
      return "Out of memory";
    case P_ENOSPC:
      return "No space left on device";

    // Other Errors (30+)
    case P_ENOSYS:
      return "Function not implemented";
    case P_EUNKNOWN:
      return "Unknown error";

    default:
      return "Unknown error code";
  }
}

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
 * @param msg User message to print before the error description (can be NULL)
 */
void u_perror(const char* msg) {
  int err = get_p_errno();
  const char* err_str = get_error_string(err);

  if (msg != NULL && msg[0] != '\0') {
    fprintf(stderr, "%s: %s\n", msg, err_str);
  } else {
    fprintf(stderr, "%s\n", err_str);
  }
}
