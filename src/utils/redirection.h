#ifndef REDIRECTION_H_
#define REDIRECTION_H_

#include "parser/parser.h"

/**
 * @brief Get redirected file descriptors for stdin/stdout
 * Opens files specified in redirection operators and returns their file descriptors.
 *
 * @param cmd parsed command with redirection info
 * @param fd_in pointer to store input file descriptor
 * @param fd_out pointer to store output file descriptor
 */
void get_redirected_fds(struct parsed_command* cmd, int* fd_in, int* fd_out);

#endif  // REDIRECTION_H_
