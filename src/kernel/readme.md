1. PennOS Kernel
Your PennOS operating system will run as a single process on the host OS (e.g., Linux on your Docker Container). That is, each of the “processes” in PennOS is really the same process as PennOS according to the host operating system. However, to imitate real processes within PennOS, each “process” will be implemented as threads that are independently scheduled by your implementation of the kernel.

To accomplish this task you will be using a provided wrapper around the pthread library called spthread. If you are issuing calls to fork(2), exec(2), or wait(2), you are doing something very wrong.

Your PennOS should have notion of a process control block (PCB), which is structure that describes all the needed information about a running process (aka thread). In the PCB, you will have to at least store (but certainly not limited to):

a handle to the spthread
PID, parent PID, child PID(s)
open file descriptors
priority level
process state
You must describe your PCB structure in your companion document.

If you would like, you can refer to Steven’s Advanced Programming in the UNIX Environment and Tanenbaum’s Modern Operating Systems for more information about data normally stored in a PCB.


1.1 Process-Related Required Functions
Your operating system must provide the following system call functions for interacting with PennOS process creation:

 Reminder: you do not need to use these functions exactly as they are described. You are free to make modifications on the parameters they take, but a function doing this functionality should be somewhere in your code.

/**
 * @brief Create a child process that executes the function `func`.
 * The child will retain some attributes of the parent.
 *
 * @param func Function to be executed by the child process.
 * @param argv Null-terminated array of args, including the command name as argv[0].
 * @param fd0 Input file descriptor.
 * @param fd1 Output file descriptor.
 * @return pid_t The process ID of the created child process.
 */
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1);

/**
 * @brief Wait on a child of the calling process, until it changes state.
 * If `nohang` is true, this will not block the calling process and return immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent.
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal);

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void);
Your operating system will also provide at least the following system calls for interacting with the scheduler:

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority);

/**
 * @brief Suspends execution of the calling proces for a specified number of clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the system
 * clock continues to tick even if the call is interrupted.
 * The sleep can be interrupted by a P_SIGTERM signal, after which the function will
 * return prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater than 0.
 */
void s_sleep(unsigned int ticks);
Additionally, your operating system will have the following kernel-level functions:

/**
 * @brief Create a new child process, inheriting applicable properties from the parent.
 *
 * @return Reference to the child PCB.
 */
pcb_t* k_proc_create(pcb_t *parent);

/**
 * @brief Clean up a terminated/finished thread's resources.
 * This may include freeing the PCB, handling children, etc.
 */
void k_proc_cleanup(pcb_t *proc);
Note that these functions are a starting point. Again, feel free to add more functions as needed, but be sure to maintain the abstraction between user-level and kernel-level.


1.2 Signals in PennOS
Your PennOS will not have traditional signals and signal handlers (however, the host operating system may deliver signals that you must handle). Instead, signaling a PennOS process indicates to PennOS that it should take some action related to the signaled thread, such as change the state of a thread to stopped or running. Your operating system will minimally define the following signals:

P_SIGSTOP: a thread receiving this signal should be stopped
P_SIGCONT: a thread receiving this signal should be continued
P_SIGTERM: a thread receiving this signal should be terminated
If you would like to add additional signals, be sure to document them and their functionality in your companion document file.

To be clear, you should not need to call the signal function, pthread_kill or other such functions to deliver these signals through the host operating system. Your code should instead handle these “signals” their own way, even if they are not used like how we have used signals in past assignments.


1.3 Process Statuses
Threads can exists in four states: running, stopped, blocked, or zombied. A running thread may be scheduled; however, a stopped, blocked, or zombied thread should not be. A thread should only become stopped if it was appropriately signaled via s_kill. A thread should only be blocked if it made a call to s_waitpid or s_sleep (see below).

PennOS will provide at least the following user level functions/macros that will return booleans based on the status returned from s_waitpid.

P_WIFEXITED(status): return true if the child terminated normally. That is, by a call to s_exit or by returning.
P_WIFSTOPPED(status): return true if the child was stopped by a signal.
P_WIFSIGNALED(status): return true if the child was terminated by a signal. That is, by a call to s_kill with the S_SIGTERM signal.

1.4 Init, Zombies and Waiting
As processes complete, it may not always be the case that their parent processes can wait on them immediately. As a result, you must queue up these processes so that the parent may wait on them in the future. These terminated but unreaped processes are zombies.

If at any time the parent thread exits prior to waiting on zombie children, the zombie children will have INIT become their new parent. Your init should reap the zombies through s_waitpid and will eventually reap the other children once they terminate (either by exiting normally or being signalled).

To do this, we highly recommend having an INIT process similar to how unix does. The INIT process is the ancestor of all processes and is started on boot. That means it is the first process that your OS should start up, and then init will create the shell process. One main thing that the init process does, is that all orphan child processes will have INIT become their new parent and init will clean up those processes (through something like s_waitpid()).

Additionally, as noted above, other child process state changes can cause a s_waitpid() to return. In UNIX, a child process that transitions from running to stopped would issue a SIGCHLD signal. Your operating system should have similar functionality for parent process to learn of similar state changes. In your PennOS kernel-land implementation of s_waitpid(), you may find it useful to maintain other queues of “waitable” children, not just zombie children.