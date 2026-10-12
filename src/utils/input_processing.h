#ifndef INPUT_PROCESSING_H_
#define INPUT_PROCESSING_H_

#include "parser/parser.h"

/**
 * @brief Helper function for parsing command.
 * It calls parse_command() in parser.c with given input.
 * It stores the parsed command to given pointer to cmd.
 * Then it error checks if parse_command() is done successfully.
 *
 * @param cmd pointer to struct parsed_command*
 * @param input input string that needs to be parsed
 * @return true if command parsed successfully without error
 * @return false if there was error
 */
bool input_check(struct parsed_command** cmd, char* input);

#endif  // INPUT_PROCESSING_H_
