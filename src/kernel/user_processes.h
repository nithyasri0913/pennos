#ifndef USER_PROCESSES_H_
#define USER_PROCESSES_H_

#include <stdbool.h>
#include <sys/types.h>

/**
 * @brief Argument structure for busy process
 *
 * Contains the process name and background flag for the busy command.
 */
typedef struct {
  char name[64];        /**< Name of the busy process */
  bool is_background;   /**< Whether process runs in background */
} busy_args_t;

/**
 * @brief Simple test process that prints "Hello World"
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* hello_process(void* arg);

/**
 * @brief Process that counts from 1 to n with delays
 *
 * @param arg Command-line arguments (expects count as argv[1])
 * @return NULL
 */
void* counting_process(void* arg);

/**
 * @brief Process that sleeps for a specified number of ticks
 *
 * Calls s_sleep() with the tick count specified in argv[1].
 *
 * @param arg Command-line arguments (expects tick count as argv[1])
 * @return NULL
 */
void* user_sleep(void* arg);

/**
 * @brief Test process that immediately exits to become a zombie
 *
 * Used for testing zombie process handling and s_waitpid().
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* zombie_process(void* arg);

/**
 * @brief Process that creates a zombie child then exits (orphaning the zombie)
 *
 * Used for testing orphaned zombie handling and INIT reparenting.
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* zombify_parent_process(void* arg);

/**
 * @brief Process that displays all running processes (ps command)
 *
 * Prints a formatted table showing PID, PPID, priority, and state for all
 * processes in the system.
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* ps_process(void* arg);

/**
 * @brief Child process for orphan testing
 *
 * Sleeps for a period to allow parent to exit first, creating an orphan.
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* orphan_child(void* arg);

/**
 * @brief Parent process that creates a child then immediately exits
 *
 * Used for testing orphan process handling and reparenting to INIT.
 *
 * @param arg Command-line arguments (unused)
 * @return NULL
 */
void* orphanify(void* arg);

/**
 * @brief Spawn a process with a specific priority level
 *
 * Wrapper around s_spawn() that sets the process priority after creation
 * using s_nice(). Used by the shell to implement priority specification
 * via the 'nice' prefix.
 *
 * @param func Function to execute in the new process
 * @param argv Null-terminated argument array
 * @param fd0 Input file descriptor
 * @param fd1 Output file descriptor
 * @param priority Priority level (0=high, 1=medium, 2=low)
 * @param bg Whether to run in background
 * @return PID of spawned process, or -1 on error
 */
pid_t nice_process(void* (*func)(void*),
                   char* argv[],
                   int fd0,
                   int fd1,
                   int priority,
                   bool bg);

/**
 * @brief Busy-wait process for CPU stress testing
 *
 * Executes an infinite loop performing meaningless computation to
 * consume CPU time. Used for testing scheduler behavior under load.
 *
 * @param arg busy_args_t* containing process name and background flag
 * @return NULL (never returns normally)
 */
void* user_busy(void* arg);

/**
 * @brief User-space cat command - concatenate and display files
 *
 * @param arg Command-line arguments (filenames)
 * @return NULL
 */
void* user_cat(void* arg);

/**
 * @brief User-space echo command - display text
 *
 * @param arg Command-line arguments (text to echo)
 * @return NULL
 */
void* user_echo(void* arg);

/**
 * @brief User-space ls command - list directory contents
 *
 * @param arg Command-line arguments (directory path, optional)
 * @return NULL
 */
void* user_ls(void* arg);

/**
 * @brief User-space touch command - create empty file
 *
 * @param arg Command-line arguments (filename)
 * @return NULL
 */
void* user_touch(void* arg);

/**
 * @brief User-space mv command - move/rename file
 *
 * @param arg Command-line arguments (source, destination)
 * @return NULL
 */
void* user_mv(void* arg);

/**
 * @brief User-space cp command - copy file
 *
 * @param arg Command-line arguments (source, destination)
 * @return NULL
 */
void* user_cp(void* arg);

/**
 * @brief User-space rm command - remove file
 *
 * @param arg Command-line arguments (filename)
 * @return NULL
 */
void* user_rm(void* arg);

/**
 * @brief User-space man command - display command manual
 *
 * @param arg Command-line arguments (command name)
 * @return NULL
 */
void* user_man(void* arg);

/**
 * @brief User-space chmod command - change file permissions
 *
 * @param arg Command-line arguments (permissions, filename)
 */
void user_chmod(char** arg);

#endif  // USER_PROCESSES_H_