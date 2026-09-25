#ifndef MESCALINE_CLI_H
#define MESCALINE_CLI_H

#include "shader.h"

typedef enum {
  CLI_RUN,
  CLI_HELP,
  CLI_VERSION,
  CLI_ERROR,
} cli_result_t;

cli_result_t cli_parse(int argc, char *const argv[], mescaline_options_t *options,
                       mescaline_error_t *error);
void cli_print_usage(FILE *stream, const char *program);

#endif
