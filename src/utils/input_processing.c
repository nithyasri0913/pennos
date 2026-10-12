#define _GNU_SOURCE
#include "input_processing.h"
#include <stdlib.h>
#include <string.h>
#include "../FAT/s_functions.h"

bool input_check(struct parsed_command** cmd, char* input) {
  int ret = parse_command(input, cmd);
  if (ret < 0) {
    perror("parse_command");
    return false;
  }
  if (ret > 0) {
    if (ret == UNEXPECTED_FILE_INPUT || ret == UNEXPECTED_FILE_OUTPUT) {
      s_printf("invalid\n");
    } else {
      print_parser_errcode(stderr, ret);
    }
    return false;
  }
  if (!*cmd) {
    return false;
  }
  return true;
}