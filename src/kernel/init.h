#ifndef INIT_H_
#define INIT_H_

/**
 * @brief Signal the INIT process to begin shutdown
 *
 * Sets the shutdown flag that causes the INIT process main loop to exit.
 */
void init_shutdown(void);

/**
 * @brief Main function for the INIT process (PID 1)
 *
 * The INIT process is the first user-space process in PennOS. It is
 * responsible for:
 * - Running the interactive shell
 * - Reaping orphaned zombie processes
 * - Managing system shutdown
 *
 * This function runs in an infinite loop until init_shutdown() is called.
 * It serves as the ancestor of all user processes and adopts orphaned
 * children when their parent processes exit.
 *
 * @param arg Unused argument (required for thread function signature)
 * @return NULL when the INIT process exits
 */
void* init_main(void* arg);

#endif  // INIT_H_
