#ifndef PCB_H_
#define PCB_H_

#include <stdbool.h>
#include <sys/types.h>
#include "FAT/fd_table.h"
#include "utils/spthread.h"

/**
 * @brief Process states in PennOS
 *
 * - RUNNING: Process is active and schedulable
 * - BLOCKED: Process is waiting for I/O or child state change
 * - STOPPED: Process is paused by P_SIGSTOP signal
 * - ZOMBIE: Process has terminated but not yet been reaped by parent
 */
typedef enum { RUNNING, BLOCKED, STOPPED, ZOMBIE } process_state_t;

/**
 * @brief Signal definitions for PennOS
 */
#define P_SIGSTOP 0  /**< Stop (pause) a process */
#define P_SIGCONT 1  /**< Continue a stopped process */
#define P_SIGTERM 2  /**< Terminate a process */

/**
 * @brief Status macros for s_waitpid - check how a process terminated
 */
#define P_WIFEXITED(status) ((status & 0x7F) == 0)              /**< True if process exited normally */
#define P_WIFSTOPPED(status) ((status & 0xFF) == 0x7F)          /**< True if process was stopped */
#define P_WIFSIGNALED(status) (((status & 0x7F) + 1) >> 1 > 0)  /**< True if process was terminated by signal */

/**
 * @brief Extract exit code from status returned by s_waitpid
 */
#define P_EXITSTATUS(status) ((status >> 8) & 0xFF)

/**
 * @brief Maximum number of children a process can have
 */
#define MAX_CHILDREN 128

/**
 * @brief Process Control Block (PCB)
 *
 * Contains all state information for a single process in PennOS.
 * Each process has a unique PID, parent PID, priority level, and
 * tracks its children, zombies, and stopped children for proper
 * process lifecycle management.
 */
typedef struct pcb {
  spthread_t thread;              /**< Thread handle for this process */

  pid_t pid;                      /**< Process ID (unique identifier) */
  pid_t ppid;                     /**< Parent process ID */
  char name[256];                 /**< Process name (from argv[0]) */

  int priority;                   /**< Priority level: 0 (high), 1 (medium), 2 (low) */
  process_state_t state;          /**< Current process state */
  int exit_status;                /**< Exit status (encoded for s_waitpid) */
  int p_errno;                    /**< Per-process error number (similar to errno) */

  pid_t children[MAX_CHILDREN];   /**< Array of living child PIDs */
  int num_children;               /**< Number of living children */

  pid_t zombie_children[MAX_CHILDREN];  /**< Array of zombie child PIDs */
  int exit_statuses[MAX_CHILDREN];      /**< Exit statuses of zombie children */
  int num_zombies;                      /**< Number of zombie children */

  pid_t stopped_children[MAX_CHILDREN];  /**< Array of stopped child PIDs */
  int num_stopped;                       /**< Number of stopped children */

  int pending_signal;             /**< Pending signal (-1 if none, else P_SIG*) */
  unsigned int sleep_ticks_remaining;  /**< Remaining sleep ticks (0 if not sleeping) */

  int fd_table[MAX_FDS];          /**< File descriptor table */

  bool is_waiting;                /**< True if blocked in s_waitpid */
  pid_t waiting_for_pid;          /**< PID being waited for (-1 for any child) */

  bool thread_created;            /**< True if thread has been created */
  bool is_background;             /**< True if started as background job */
} pcb_t;

/**
 * @brief Create a new process control block
 *
 * Allocates and initializes a new PCB with the specified parameters.
 *
 * @param pid Process ID for the new process
 * @param ppid Parent process ID
 * @param name Process name
 * @param priority Priority level (0, 1, or 2)
 * @return Pointer to newly allocated PCB, or NULL on failure
 */
pcb_t* pcb_create(pid_t pid, pid_t ppid, const char* name, int priority);

/**
 * @brief Destroy a process control block and free its memory
 *
 * Frees all resources associated with the PCB. 
 *
 * @param pcb PCB to destroy
 */
void pcb_destroy(pcb_t* pcb);

/**
 * @brief Add a child to the parent's children list
 *
 * Adds the child PID to the parent's living children array.
 *
 * @param parent Parent PCB
 * @param child_pid PID of child to add
 */
void pcb_add_child(pcb_t* parent, pid_t child_pid);

/**
 * @brief Remove a child from the parent's children list
 *
 * Removes the child PID from the parent's living children array.
 *
 * @param parent Parent PCB
 * @param child_pid PID of child to remove
 */
void pcb_remove_child(pcb_t* parent, pid_t child_pid);

/**
 * @brief Add a zombie child to the parent's zombie queue
 *
 * Moves a child from the living children list to the zombie queue
 * with its exit status. Used when a child process exits.
 *
 * @param parent Parent PCB
 * @param child_pid PID of child that became a zombie
 * @param exit_status Exit status of the zombie child
 */
void pcb_add_zombie(pcb_t* parent, pid_t child_pid, int exit_status);

/**
 * @brief Get and remove a zombie child from the parent's zombie queue
 *
 * Searches for a zombie child matching the given PID (or any zombie if
 * pid == -1) and removes it from the queue. Returns the zombie's PID
 * and exit status.
 *
 * @param parent Parent PCB
 * @param pid PID to search for, or -1 for any zombie
 * @param exit_status Output parameter for zombie's exit status
 * @return PID of zombie found, or -1 if no matching zombie
 */
pid_t pcb_get_zombie(pcb_t* parent, pid_t pid, int* exit_status);

/**
 * @brief Add a child to the parent's stopped children list
 *
 * Tracks that a child has been stopped (P_SIGSTOP) 
 *
 * @param parent Parent PCB
 * @param child_pid PID of child that was stopped
 */
void pcb_add_stopped_child(pcb_t* parent, pid_t child_pid);

/**
 * @brief Remove a child from the parent's stopped children list
 *
 * Used when a stopped child is continued (P_SIGCONT) or terminated.
 *
 * @param parent Parent PCB
 * @param child_pid PID of child to remove from stopped list
 */
void pcb_remove_stopped_child(pcb_t* parent, pid_t child_pid);

#endif  // PCB_H_