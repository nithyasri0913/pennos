# PennOS

Our CIS 5480 (Fall '25) final project. PennOS is a small Unix-like OS that runs as a single process on the host. "Processes" are really spthreads that our own scheduler swaps in and out, and files live in PennFAT, a FAT-style filesystem stored inside one regular host file.

Team 39: Kushal Garg, Nithyasri Srikumar, Seungju Lee, Xiaoyang Wan

## Building

You'll need `clang-15`, which the course Docker container already has.

```bash
make            # builds bin/pennfat and bin/pennos
make pennfat    # just pennfat (also drops a ./pennfat symlink)
make tests      # builds bin/test_p_errno
make clean
make format     # clang-format, Chromium style
```

Warnings are errors (`-Werror`), so the build fails if anything new throws a warning.

## Running

### PennFAT (standalone)

You can use this to create and poke at a filesystem without the kernel running.

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

It also supports `cat`, `mv`, `rm`, `chmod` and `print_fat`. There are a bunch of `test_*` commands in [src/pennfat.c](src/pennfat.c) that we used while building the k-level functions. They're still in there in case they're useful.

### PennOS

```bash
./bin/pennos myfs
```

Pass it a filesystem you made with `pennfat`. If you leave it off, it still boots, but the file commands won't have anything to work with. Scheduler events get logged to `log/log`.

Once you're in the shell, type `help` to see everything. The main ones:

- **Processes:** `ps`, `kill [-term|-stop|-cont] pid`, `nice <prio> <cmd>`, `nice_pid <prio> <pid>`, `sleep`, `busy`
- **Job control:** `&` for background, `jobs`, `fg`, `bg`, Ctrl-C / Ctrl-Z
- **Files:** `ls`, `cat`, `echo`, `touch`, `mv`, `cp`, `rm`, `chmod`
- **Redirection:** `<`, `>` and `>>` all work
- **Scripts:** if a file in PennFAT is executable (`chmod +x`), you can run it by name and it'll run each line as a command
- **Test stuff:** `zombify`, `orphanify`, `hang`, `nohang`, `recur`, `crash`
- `logout` shuts the OS down cleanly

## How it's laid out

```
src/
  pennos.c        boots the kernel, creates INIT, starts the scheduler
  pennfat.c       standalone filesystem shell
  kernel/         PCBs, scheduler, syscalls (s_*), shell, init, logging, p_errno
  FAT/            mkfs/mount, k_* file functions, s_* wrappers, fd table, shell commands
  parser/         command line parser (provided)
  utils/          spthread, Vec, redirection, panic helpers
tests/            stress tests + p_errno tests
doc/              companion document + course readme
```

We split things up the way the spec asks: user programs call `s_*` functions, and those call the `k_*` ones. User code never reaches into the kernel directly.

### A few notes

- **Scheduler:** three priority levels (0, 1, 2), driven by a `SIGALRM` timer with a 100ms quantum. Higher-priority queues get picked more often, but lower ones still get time, so nothing starves.
- **INIT** is PID 1. It spawns the shell and reaps any orphans that get handed to it.
- **FAT layout:** FAT[0] stores the FAT size and block size config, and block 1 is always the root directory. `0x0000` means a free block and `0xFFFF` marks the end of a chain.
- **Errors:** each PCB has its own `p_errno`. Syscalls set it and return -1, and `u_perror()` prints the message, basically like `errno`/`perror`.

The [companion doc](doc/PennOS_Companion_Document.pdf) goes into much more detail (PCB fields, signals, design decisions, etc.).
