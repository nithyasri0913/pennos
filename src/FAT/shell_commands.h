/**
 * @file shell_commands.h
 * @brief PennFAT shell commands
 *
 * Provides Unix-like file manipulation commands for the PennFAT filesystem.
 * These commands are used by both the interactive shell and as user processes.
 */

#ifndef SHELL_COMMANDS_H_
#define SHELL_COMMANDS_H_

/**
 * @brief List directory contents
 *
 * Displays files in the filesystem with their metadata (size, permissions,
 * modification time). If args[1] is provided, shows detailed info for that
 * specific file. Otherwise, lists all files.
 *
 * Usage: ls [filename]
 *
 * @param args Null-terminated argument array (args[0] = "ls", args[1] = optional filename)
 */
void cmd_ls(char** args);

/**
 * @brief Change file permissions
 *
 * Modifies the permission bits of a file. Permissions are specified as
 * octal values (0-7) corresponding to read(4), write(2), execute(1).
 *
 * Usage: chmod <permissions> <filename>
 *
 * @param args Null-terminated argument array (args[0] = "chmod", args[1] = perms, args[2] = filename)
 */
void cmd_chmod(char** args);

/**
 * @brief Create an empty file
 *
 * Creates a new empty file with default permissions (PERM_READ_WRITE).
 * If file already exists, updates its modification time.
 *
 * Usage: touch <filename>
 *
 * @param args Null-terminated argument array (args[0] = "touch", args[1] = filename)
 */
void touch(char** args);

/**
 * @brief Move/rename a file
 *
 * Renames a file from source to destination. Implemented by reading the
 * entire source file, creating destination, and deleting source.
 *
 * Usage: mv <source> <destination>
 *
 * @param args Null-terminated argument array (args[0] = "mv", args[1] = source, args[2] = dest)
 */
void mv(char** args);

/**
 * @brief Remove (delete) a file
 *
 * Deletes the specified file from the filesystem. File must not be
 * currently open by any process.
 *
 * Usage: rm <filename>
 *
 * @param args Null-terminated argument array (args[0] = "rm", args[1] = filename)
 */
void rm(char** args);

/**
 * @brief Concatenate and display file contents
 *
 * Reads and displays the contents of one or more files to stdout.
 * If multiple files are specified, concatenates them in order.
 *
 * Usage: cat <filename> [filename2 ...]
 *
 * @param args Null-terminated argument array (args[0] = "cat", args[1..n] = filenames)
 */
void cat(char** args);

/**
 * @brief Copy a file
 *
 * Creates a duplicate of a file. Reads the entire source file and
 * writes it to the destination.
 *
 * Usage: cp <source> <destination>
 *
 * @param args Null-terminated argument array (args[0] = "cp", args[1] = source, args[2] = dest)
 */
void cp(char** args);

#endif  // SHELL_COMMANDS_H_