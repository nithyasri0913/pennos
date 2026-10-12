#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <signal.h>
#include <stdbool.h>
#include "kernel/pcb.h"

/**
 * @brief PID of the current foreground process
 *
 * Shared state used by signal handler to send SIGTERM to foreground
 * process on Ctrl-C. Set to -1 when no foreground process is running.
 */
extern volatile pid_t foreground_pid;

/**
 * @brief Flag indicating whether SIGINT (Ctrl-C) was received
 *
 * Set by signal handler and checked by shell to determine if user
 * interrupted command.
 */
extern volatile bool sigint_received;

/**
 * @brief Print help information for shell commands
 *
 * Displays usage information for all built-in shell commands.
 */
void print_help(void);

/**
 * @brief Display all running processes (ps command)
 *
 * Prints a table showing PID, PPID, priority, and state for all processes.
 */
void cmd_ps(void);

/**
 * @brief Display detailed information about a specific process
 *
 * Shows comprehensive information including PID, name, parent, priority,
 * state, children, zombies, and stopped children.
 *
 * @param pid Process ID to query
 */
void cmd_info(pid_t pid);

/**
 * @brief Spawn a test process
 *
 * Creates various test processes for demonstration purposes (hello,
 * counting, orphan, etc.).
 *
 * @param type Type of process to spawn (e.g., "hello", "counting")
 */
void cmd_spawn(const char* type);

/**
 * @brief Non-blocking wait for a specific child process
 *
 * Equivalent to waitpid(pid, &status, WNOHANG) in UNIX.
 * Returns immediately if child has not changed state.
 *
 * @param pid Process ID to wait for
 */
void cmd_waitpid(pid_t pid);

/**
 * @brief Non-blocking wait for any child process
 *
 * Equivalent to waitpid(-1, &status, WNOHANG) in UNIX.
 * Returns immediately if no child has changed state.
 */
void cmd_waitany(void);

/**
 * @brief Blocking wait for a specific child process
 *
 * Equivalent to waitpid(pid, &status, 0) in UNIX.
 * Blocks until the specified child changes state.
 *
 * @param pid Process ID to wait for
 */
void cmd_waitpid_blocking(pid_t pid);

/**
 * @brief Blocking wait for any child process
 *
 * Equivalent to waitpid(-1, &status, 0) in UNIX.
 * Blocks until any child changes state.
 */
void cmd_waitany_blocking(void);

/**
 * @brief Send a signal to a process
 *
 * @param pid Target process ID
 * @param signal Signal to send (P_SIGSTOP, P_SIGCONT, or P_SIGTERM)
 */
void cmd_kill(pid_t pid, int signal);

/**
 * @brief Change process priority
 *
 * @param pid Target process ID
 * @param priority New priority level (0=high, 1=medium, 2=low)
 */
void cmd_nice(pid_t pid, int priority);

/**
 * @brief Create a zombie process for testing
 *
 * Spawns a child that immediately exits to become a zombie.
 *
 * @param background Whether to run the zombie process in background
 */
void cmd_zombify(bool background);

/**
 * @brief Create an orphan process for testing
 *
 * Spawns a parent that creates a child then exits, orphaning the child
 * which gets reparented to INIT.
 */
void cmd_orphanify(void);

/**
 * @brief Move a stopped background job to foreground
 *
 * Sends P_SIGCONT to the process and waits for it in foreground.
 *
 * @param pid Process ID to bring to foreground
 */
void cmd_fg(pid_t pid);

/**
 * @brief Resume a stopped background job
 *
 * Sends P_SIGCONT to the process and allows it to run in background.
 *
 * @param pid Process ID to resume in background
 */
void cmd_bg(pid_t pid);

/**
 * @brief Display all background jobs
 *
 * Lists stopped and running background processes.
 */
void cmd_jobs(void);

/**
 * @brief Exit the shell and shutdown the system
 *
 * Triggers INIT shutdown sequence, stopping scheduler and cleaning up
 * all processes.
 */
void cmd_logout(void);

/**
 * @brief Run the interactive shell in the current process context
 *
 * Main shell loop - reads commands, parses them, and executes them.
 */
void run_shell(void);

/**
 * @brief Thread wrapper function for running the shell
 *
 * @param arg Unused argument (required for thread function signature)
 * @return NULL when shell exits
 */
void* shell_main(void* arg);

/**
 * @brief Signal handler for SIGINT (Ctrl-C)
 *
 * Sends P_SIGTERM to the current foreground process and sets the
 * sigint_received flag.
 *
 * @param sig Signal number (SIGINT)
 */
void sigint_handler(int sig);

#endif  // COMMANDS_H_