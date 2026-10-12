#ifndef KERNEL_H_
#define KERNEL_H_

#include <stdbool.h>
#include <sys/types.h>
#include "logging.h"
#include "pcb.h"
// Maximum number of processes
#define MAX_PROCESSES 256

// Special PID for INIT process
#define INIT_PID 1

/**
 * @brief Initialize the kernel and process table
 *
 * This should be called once at boot time before any other kernel functions.
 */
void k_init(void);

/**
 * @brief Shutdown the kernel and clean up all resources
 */
void k_shutdown(void);

/**
 * @brief Create a new child process, inheriting applicable properties from the
 * parent
 *
 * @param parent Parent PCB (can be NULL for INIT process)
 * @return Pointer to the child PCB, or NULL on failure
 */
pcb_t* k_proc_create(pcb_t* parent);

/**
 * @brief Clean up a terminated/finished process's resources
 *
 * This handles:
 * - Removing the process from the process table
 * - Handling zombie children (reparenting to INIT)
 * - Notifying parent of termination
 * - Freeing the PCB
 *
 * @param proc Process to clean up
 */
void k_proc_cleanup(pcb_t* proc);

/**
 * @brief Get a process by PID
 *
 * @param pid Process ID to look up
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* k_get_process(pid_t pid);

/**
 * @brief Get the currently running process
 *
 * @return Pointer to the current process's PCB
 */
pcb_t* k_get_current_process(void);

/**
 * @brief Set the currently running process
 *
 * @param proc Process to set as current
 */
void k_set_current_process(pcb_t* proc);

/**
 * @brief Get the INIT process
 *
 * @return Pointer to INIT process's PCB
 */
pcb_t* k_get_init_process(void);

/**
 * @brief Allocate a new unique PID
 *
 * @return New PID, or -1 if no PIDs available
 */
pid_t k_alloc_pid(void);

/**
 * @brief Add a process to the process table
 *
 * @param proc Process to add
 * @return 0 on success, -1 on failure
 */
int k_add_process(pcb_t* proc);

/**
 * @brief Remove a process from the process table
 *
 * @param pid Process ID to remove
 */
void k_remove_process(pid_t pid);

/**
 * @brief Notify parent that a child has changed state
 *
 * This is called when a child process:
 * - Exits (becomes zombie)
 * - Stops (P_SIGSTOP)
 * - Continues (P_SIGCONT)
 *
 * @param child_pid Child process ID
 * @param parent Parent PCB
 */
void k_notify_parent(pid_t child_pid, pcb_t* parent);

/**
 * @brief Reparent children to INIT process
 *
 * Called when a parent process exits with living children.
 *
 * @param parent_pid PID of the parent that is exiting
 */
void k_reparent_children_to_init(pid_t parent_pid);

/**
 * @brief Handle pending signals for a process
 *
 * Should be called by the scheduler before context switching to check
 * if the process has any pending signals that need to be handled.
 *
 * @param proc Process to check
 */
void k_handle_pending_signals(pcb_t* proc);

/**
 * @brief Get total number of active processes
 *
 * @return Number of processes in the system (excluding zombies)
 */
int k_get_process_count(void);

/**
 * @brief To give scheduler access to process table
 *
 * @return pcb_t** ptr to process table
 */
pcb_t** k_get_process_table(void);

#endif  // KERNEL_H_
