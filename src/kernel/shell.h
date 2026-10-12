#ifndef SHELL_H_
#define SHELL_H_

/**
 * @brief Run the interactive shell in the current process context
 *
 * Executes the main shell loop, reading commands from stdin and executing
 * them. This function handles:
 * - Reading user input
 * - Parsing commands
 * - Executing built-in commands (cat, sleep, busy, ls, touch, etc.)
 * - Spawning processes for user programs
 * - Managing job control (foreground/background processes)
 *
 * This function runs until the user exits (Ctrl+D or exit command).
 */
void run_shell(void);

/**
 * @brief Thread wrapper function for running the shell
 *
 * Thin wrapper around run_shell() that provides the proper signature
 * for use as a thread function. Used when spawning the shell as a
 * separate thread in the INIT process.
 *
 * @param arg Unused argument (required for thread function signature)
 * @return NULL when the shell exits
 */
void* shell_main(void* arg);

#endif  // SHELL_H_
