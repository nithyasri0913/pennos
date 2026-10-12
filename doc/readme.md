# PennOS - CIS 5480

## Student Information

**Name:** Kushal Garg
**Penn Key:** kushalx

**Name:** Nithyasri Srikumar
**Penn Key:** nithsri

**Name:** Seungju Lee
**Penn Key:** slee25

**Name:** Xiaoyang Wan
**Penn Key:** wan3

## Submitted Source Files

### Main Programs
- `src/pennfat.c` - Main entry point for PennFAT shell
- `src/pennos.c` - Main entry point for PennOS

### FAT Filesystem (`src/FAT/`)
- `src/FAT/commands.c` - Filesystem commands (mkfs, mount, unmount)
- `src/FAT/commands.h` - Header for filesystem commands
- `src/FAT/k_functions.c` - Core kernel functions for file operations
- `src/FAT/k_functions.h` - Header for kernel functions
- `src/FAT/s_functions.c` - System-level functions
- `src/FAT/s_functions.h` - Header for system functions
- `src/FAT/shell_commands.c` - User-facing shell commands
- `src/FAT/shell_commands.h` - Header for shell commands
- `src/FAT/fd_table.c` - File descriptor table management
- `src/FAT/fd_table.h` - Header for file descriptor table
- `src/FAT/fat_structs.h` - Data structures and definitions for FAT

### Kernel (`src/kernel/`)
- `src/kernel/kernel.c` - Core kernel implementation
- `src/kernel/kernel.h` - Header for kernel
- `src/kernel/shell.c` - Kernel shell implementation
- `src/kernel/shell.h` - Header for kernel shell
- `src/kernel/commands.c` - Kernel command implementations
- `src/kernel/commands.h` - Header for kernel commands
- `src/kernel/scheduler.c` - Process scheduler implementation
- `src/kernel/scheduler.h` - Header for scheduler
- `src/kernel/pcb.c` - Process Control Block management
- `src/kernel/pcb.h` - Header for PCB
- `src/kernel/syscall.c` - System call implementations
- `src/kernel/syscall.h` - Header for system calls
- `src/kernel/init.c` - Kernel initialization
- `src/kernel/init.h` - Header for kernel initialization
- `src/kernel/logging.c` - Logging functionality
- `src/kernel/logging.h` - Header for logging
- `src/kernel/user_processes.c` - User process management
- `src/kernel/user_processes.h` - Header for user processes

### Parser (`src/parser/`)
- `src/parser/parser.c` - Command line parser implementation
- `src/parser/parser.h` - Header for parser

### Utilities (`src/utils/`)
- `src/utils/Vec.c` - Vector data structure implementation
- `src/utils/Vec.h` - Header for vector
- `src/utils/input_processing.c` - Input processing utilities
- `src/utils/input_processing.h` - Header for input processing
- `src/utils/panic.c` - Panic/error handling
- `src/utils/panic.h` - Header for panic handling
- `src/utils/redirection.c` - I/O redirection handling
- `src/utils/redirection.h` - Header for redirection
- `src/utils/spthread.c` - Simple pthread wrapper implementation
- `src/utils/spthread.h` - Header for spthread

### Tests (`tests/`)
- `tests/stress.c` - Stress test implementations for process management
- `tests/stress.h` - Header for stress tests

### Additional Files
- `Makefile` - Build configuration

## Extension Answers
[Provide answers to any extension questions here]

## Compilation Instructions
```bash
# Build PennFAT
make pennfat

# Build PennOS
make pennos

# Build everything
make all

# Clean build artifacts
make clean
```

Run the compiled programs:
```bash
# Run PennFAT
./bin/pennfat

# Run PennOS
./bin/pennos
```

## Overview of Work Accomplished
This project implements:
- **PennFAT**: A FAT (File Allocation Table) based filesystem with support for file operations including create, read, write, delete, copy, and permission management
- **PennOS**: A Unix-like operating system with process scheduling and system calls

Key accomplishments:
- Complete filesystem layer with dynamic block allocation
- File descriptor table for managing open files
- Shell interface for user interaction
- Support for host-to-FAT and FAT-to-host file transfers
- Process scheduling and management (PennOS component)

## Description of Code and Code Layout

### Directory Structure
```
src/
├── FAT/
│   ├── commands.c          # Filesystem management (mkfs, mount, unmount)
│   ├── k_functions.c       # Core kernel functions (open, read, write, close, etc.)
│   ├── shell_commands.c    # User-facing shell commands
│   ├── fd_table.c          # File descriptor table management
│   └── fat_structs.h       # Data structures and definitions
├── kernel/
│   └── shell.c             # Kernel shell implementation
└── pennfat.c              # Main PennFAT shell loop
```

### Key Components

#### FAT Filesystem (`src/FAT/`)
- **commands.c**: Implements `mkfs` (filesystem creation), `mount`, and `unmount` operations
- **k_functions.c**: Core filesystem operations including:
  - `k_open()`: Open files with READ/WRITE/APPEND modes
  - `k_read()`: Read data from file descriptors
  - `k_write()`: Write data with automatic block allocation
  - `k_close()`: Close files and update metadata
  - `k_lseek()`: Seek within files
  - `k_unlink()`: Delete files with safety checks
  - `k_ls()`: List directory contents
- **shell_commands.c**: User-level wrappers (touch, cat, cp, mv, rm, chmod, ls)
- **fd_table.c**: Manages global file descriptor table with reference counting
- **fat_structs.h**: Defines FAT structures, directory entries, and file descriptor metadata

#### Kernel (`src/kernel/`)
- **shell.c**: Implements kernel shell for PennOS

### FAT Structure Details
- **FAT[0]**: Metadata block containing blocks_in_fat (MSB) and block_size_config (LSB)
- **FAT[1]**: Root directory (always marked 0xFFFF)
- **FAT[2+]**: File data blocks with chain pointers
  - 0x0000: Free block
  - 0xFFFF: End of file
  - Other values: Next block number in chain

### File Descriptor Table
- Global table tracking all open files
- Per-FD metadata: filename, mode, offset, size, permissions
- Reference counting for proper resource cleanup

## General Comments


### Testing Notes
Carried out comprehensive testing besides the demo

### Known Issues/Limitations
None 

### Design Decisions
is_background flag in PCB
