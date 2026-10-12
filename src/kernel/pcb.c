#include "pcb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../FAT/s_functions.h"

/**
 * @brief Create a new PCB
 *
 * @param pid Process ID
 * @param ppid Parent process ID
 * @param name Process name
 * @param priority Priority level (0, 1, or 2)
 * @return Pointer to newly created PCB, or NULL on failure
 */
pcb_t* pcb_create(pid_t pid, pid_t ppid, const char* name, int priority) {
  pcb_t* pcb = (pcb_t*)malloc(sizeof(pcb_t));
  if (pcb == NULL) {
    return NULL;
  }

  // Initialize basic fields
  pcb->pid = pid;
  pcb->ppid = ppid;
  strncpy(pcb->name, name, sizeof(pcb->name) - 1);
  pcb->name[sizeof(pcb->name) - 1] = '\0';

  // Initialize state
  pcb->priority = priority;
  pcb->state = RUNNING;
  pcb->exit_status = 0;
  pcb->p_errno = 0;  // Initialize error number to success

  // Initialize children tracking
  pcb->num_children = 0;
  pcb->num_zombies = 0;
  pcb->num_stopped = 0;

  // Initialize signal handling
  pcb->pending_signal = -1;

  // Initialize sleep tracking
  pcb->sleep_ticks_remaining = 0;

  // Initialize waiting state
  pcb->is_waiting = false;
  pcb->waiting_for_pid = -1;

  // Initialize background job flag
  pcb->is_background = false;

  // Initialize file descriptor table
  for (int i = 0; i < MAX_FDS; i++) {
    pcb->fd_table[i] = -1;
  }
  pcb->fd_table[0] = 0;  // stdin
  pcb->fd_table[1] = 1;  // stdout
  pcb->fd_table[2] = 2;  // stderr

  return pcb;
}

/**
 * @brief Destroy a PCB and free its resources
 *
 * @param pcb Pointer to PCB to destroy
 */
void pcb_destroy(pcb_t* pcb) {
  if (pcb != NULL) {
    free(pcb);
  }
}

/**
 * @brief Add a child PID to the parent's children list
 *
 * @param parent Parent PCB
 * @param child_pid Child process ID
 */
void pcb_add_child(pcb_t* parent, pid_t child_pid) {
  if (parent == NULL) {
    return;
  }

  if (parent->num_children >= MAX_CHILDREN) {
    s_fprintf_stderr( "ERROR: Parent process %d has too many children\n",
            parent->pid);
    return;
  }

  parent->children[parent->num_children] = child_pid;
  parent->num_children++;
}

/**
 * @brief Remove a child PID from the parent's children list
 *
 * @param parent Parent PCB
 * @param child_pid Child process ID to remove
 */
void pcb_remove_child(pcb_t* parent, pid_t child_pid) {
  if (parent == NULL) {
    return;
  }

  for (int i = 0; i < parent->num_children; i++) {
    if (parent->children[i] == child_pid) {
      // Shift remaining children
      for (int j = i; j < parent->num_children - 1; j++) {
        parent->children[j] = parent->children[j + 1];
      }
      parent->num_children--;
      return;
    }
  }
}

/**
 * @brief Add a zombie child to the parent's zombie queue
 *
 * @param parent Parent PCB
 * @param child_pid Child process ID
 * @param exit_status Exit status of the child
 */
void pcb_add_zombie(pcb_t* parent, pid_t child_pid, int exit_status) {
  if (parent == NULL) {
    return;
  }

  if (parent->num_zombies >= MAX_CHILDREN) {
    s_fprintf_stderr( "ERROR: Parent process %d has too many zombie children\n",
            parent->pid);
    return;
  }

  parent->zombie_children[parent->num_zombies] = child_pid;
  parent->exit_statuses[parent->num_zombies] = exit_status;
  parent->num_zombies++;
}

/**
 * @brief Get and remove a zombie child from the parent's zombie queue
 *
 * @param parent Parent PCB
 * @param pid Specific child PID to get, or -1 for any zombie child
 * @param exit_status Pointer to store the exit status
 * @return PID of the zombie child, or -1 if none found
 */
pid_t pcb_get_zombie(pcb_t* parent, pid_t pid, int* exit_status) {
  if (parent == NULL || parent->num_zombies == 0) {
    return -1;
  }

  int index = -1;

  if (pid == -1) {
    // Get any zombie child (randomly selected for non-deterministic reaping)
    static bool seeded = false;
    if (!seeded) {
      srand(time(NULL));
      seeded = true;
    }
    index = rand() % parent->num_zombies;
  } else {
    // Find specific zombie child
    for (int i = 0; i < parent->num_zombies; i++) {
      if (parent->zombie_children[i] == pid) {
        index = i;
        break;
      }
    }
  }

  if (index == -1) {
    return -1;
  }

  // Get the zombie's info
  pid_t zombie_pid = parent->zombie_children[index];
  if (exit_status != NULL) {
    *exit_status = parent->exit_statuses[index];
  }

  // Remove from zombie queue
  for (int i = index; i < parent->num_zombies - 1; i++) {
    parent->zombie_children[i] = parent->zombie_children[i + 1];
    parent->exit_statuses[i] = parent->exit_statuses[i + 1];
  }
  parent->num_zombies--;

  return zombie_pid;
}

/**
 * @brief Add a stopped child to the parent's stopped children list
 *
 * @param parent Parent PCB
 * @param child_pid Child process ID
 */
void pcb_add_stopped_child(pcb_t* parent, pid_t child_pid) {
  if (parent == NULL) {
    return;
  }

  if (parent->num_stopped >= MAX_CHILDREN) {
    s_fprintf_stderr( "ERROR: Parent process %d has too many stopped children\n",
            parent->pid);
    return;
  }

  // Check if already in the list
  for (int i = 0; i < parent->num_stopped; i++) {
    if (parent->stopped_children[i] == child_pid) {
      return;  // Already in list
    }
  }

  parent->stopped_children[parent->num_stopped] = child_pid;
  parent->num_stopped++;
}

/**
 * @brief Remove a stopped child from the parent's stopped children list
 *
 * @param parent Parent PCB
 * @param child_pid Child process ID to remove
 */
void pcb_remove_stopped_child(pcb_t* parent, pid_t child_pid) {
  if (parent == NULL) {
    return;
  }

  for (int i = 0; i < parent->num_stopped; i++) {
    if (parent->stopped_children[i] == child_pid) {
      // Shift remaining children
      for (int j = i; j < parent->num_stopped - 1; j++) {
        parent->stopped_children[j] = parent->stopped_children[j + 1];
      }
      parent->num_stopped--;
      return;
    }
  }
}
