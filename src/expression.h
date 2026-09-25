#ifndef MESCALINE_EXPRESSION_H
#define MESCALINE_EXPRESSION_H

#include "shader.h"

#define MESCALINE_MAX_EXPRESSION_LENGTH 4096U
#define MESCALINE_MAX_EXPRESSION_INSTRUCTIONS 1024U
#define MESCALINE_MAX_EXPRESSION_DEPTH 64U
#define MESCALINE_MAX_EXPRESSION_STACK 64U

typedef struct {
  uint32_t px;
  uint32_t py;
  uint32_t width;
  uint32_t height;
  uint32_t frame;
  uint32_t frames;
  uint32_t stream;
  uint64_t seed;
} expression_context_t;

bool expression_compile(const char *source, expression_t **expression, mescaline_error_t *error);
double expression_evaluate(const expression_t *expression, const expression_context_t *context,
                           bool *nonfinite);
void expression_destroy(expression_t *expression);

#endif
