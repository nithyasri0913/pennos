#ifndef LOGGING_H_
#define LOGGING_H_

#include "pcb.h"

/**
 * @brief Initialize the logger and open the log file
 *
 * Opens the specified log file for writing and initializes the tick counter
 * to 0. Must be called once during kernel initialization before any logging
 * functions are used.
 *
 * @param log_filename Path to the log file to create/open
 */
void logger_init(const char* log_filename);

/**
 * @brief Close the log file and clean up logger resources
 *
 * Closes the log file handle and resets the logger state. 
 */
void logger_close(void);

/**
 * @brief Get the current tick count for logging
 *
 * Returns the current system tick counter used for timestamping log entries.
 * The tick is incremented by the scheduler every quantum (100ms).
 *
 * @return Current tick count
 */
int logger_get_ticks(void);

/**
 * @brief Set the current tick count
 *
 * Updates the system tick counter. Called by the scheduler at the start of
 * each quantum to maintain the tick count for timestamping log entries.
 *
 * @param ticks New tick count value
 */
void logger_set_ticks(int ticks);

/**
 * @brief Log a SCHEDULE event when a process is scheduled to run
 *
 * @param pid Process ID
 * @param priority Process priority level (0, 1, or 2)
 * @param process_name Name of the process
 */
void log_schedule(int pid, int priority, const char* process_name);

/**
 * @brief Log a CREATE event when a new process is created
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_create(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a SIGNALED event when a process is terminated by signal
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_signaled(int pid, int nice_value, const char* process_name);

/**
 * @brief Log an EXITED event when a process exits normally
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_exited(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a ZOMBIE event when a process becomes a zombie
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_zombie(int pid, int nice_value, const char* process_name);

/**
 * @brief Log an ORPHAN event when a process become orphan and is reparented to INIT
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_orphan(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a WAITED event when a zombie process is reaped by parent
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_waited(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a NICE event when a process priority is changed
 *
 * @param pid Process ID
 * @param old_nice_value Previous priority value
 * @param new_nice_value New priority value
 * @param process_name Name of the process
 */
void log_nice(int pid,
              int old_nice_value,
              int new_nice_value,
              const char* process_name);

/**
 * @brief Log a BLOCKED event when a process enters blocked state
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_blocked(int pid, int nice_value, const char* process_name);

/**
 * @brief Log an UNBLOCKED event when a process exits blocked state
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_unblocked(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a STOPPED event when a process stops
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_stopped(int pid, int nice_value, const char* process_name);

/**
 * @brief Log a CONTINUED event when a stopped process continues
 *
 * @param pid Process ID
 * @param nice_value Process priority/nice value
 * @param process_name Name of the process
 */
void log_continued(int pid, int nice_value, const char* process_name);

#endif  // LOGGING_H_