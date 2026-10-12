# PennOS

PennOS was developed as the final project for CIS 5480 (Fall 2025). It is a small Unix-like operating system that runs as a single process on the host. Each PennOS process is implemented as an spthread and is scheduled by a custom kernel scheduler. Files are stored in PennFAT, a FAT-style filesystem contained within a single host file.

## Building

`clang-15` is required (it is provided in the course Docker container).

```bash
make            # builds bin/pennfat and bin/pennos
make pennfat    # builds pennfat only and creates a ./pennfat symlink
make tests      # builds bin/test_p_errno
make clean
make format     # runs clang-format (Chromium style)
```

All warnings are treated as errors (`-Werror`).

## Running

### PennFAT (standalone)

PennFAT can be run on its own to create and inspect a filesystem without the kernel.

```
$ ./bin/pennfat
pennfat> mkfs myfs 1 0      # name, # of FAT blocks, block size config (0-4 => 256..4096 bytes)
pennfat> mount myfs
pennfat> touch a.txt
pennfat> cp -h notes.txt b.txt   # host -> PennFAT
pennfat> cp b.txt -h out.txt     # PennFAT -> host
pennfat> ls
pennfat> unmount
```

`cat`, `mv`, `rm`, `chmod` and `print_fat` are also supported. Several `test_*` commands used during development of the k-level functions are kept in [src/pennfat.c](src/pennfat.c).

### PennOS

```bash
./bin/pennos myfs
```

A filesystem created with `pennfat` should be passed as the argument. If none is given, the system still boots, but file commands are unavailable. Scheduler events are logged to `log/log`.

A full list of shell commands is printed by `help`. The main commands are:

- **Processes:** `ps`, `kill [-term|-stop|-cont] pid`, `nice <prio> <cmd>`, `nice_pid <prio> <pid>`, `sleep`, `busy`
- **Job control:** `&` (background), `jobs`, `fg`, `bg`, Ctrl-C, Ctrl-Z
- **Files:** `ls`, `cat`, `echo`, `touch`, `mv`, `cp`, `rm`, `chmod`
- **Redirection:** `<`, `>` and `>>`
- **Scripts:** executable files in PennFAT (`chmod +x`) can be run by name, and each line is executed as a command
- **Testing:** `zombify`, `orphanify`, `hang`, `nohang`, `recur`, `crash`
- **Shutdown:** `logout`

## Project Structure

```
src/
  pennos.c        kernel boot, INIT creation, scheduler startup
  pennfat.c       standalone filesystem shell
  kernel/         PCBs, scheduler, syscalls (s_*), shell, init, logging, p_errno
  FAT/            mkfs/mount, k_* file functions, s_* wrappers, fd table, shell commands
  parser/         command line parser (provided)
  utils/          spthread, Vec, redirection, panic helpers
tests/            stress tests and p_errno tests
doc/              companion document and course readme
```

User programs call only `s_*` functions, which in turn call the `k_*` kernel functions. Kernel state is never accessed directly by user code.

### Design Notes

- **Scheduler:** Three priority levels (0, 1, 2) are used, driven by a `SIGALRM` timer with a 100 ms quantum. Higher-priority queues are selected more often, but lower-priority queues are still guaranteed time, so starvation is prevented.
- **INIT:** INIT is created as PID 1. The shell is spawned by INIT, and orphaned processes are reparented to INIT and reaped by it.
- **FAT layout:** The FAT size and block size configuration are stored in FAT[0], and block 1 is reserved for the root directory. Free blocks are marked `0x0000` and the end of a chain is marked `0xFFFF`.
- **Errors:** A `p_errno` value is stored in each PCB. On failure, it is set by the syscall and -1 is returned; the message is printed by `u_perror()`, similar to `errno`/`perror`.

Further detail (PCB fields, signals, design decisions) is provided in the [companion document](doc/PennOS_Companion_Document.pdf).
