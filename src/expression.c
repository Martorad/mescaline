#include "expression.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  OP_NUMBER,
  OP_PX,
  OP_PY,
  OP_X,
  OP_Y,
  OP_FRAME,
  OP_T,
  OP_WIDTH,
  OP_HEIGHT,
  OP_SEED,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_POWER,
  OP_NEGATE,
  OP_SIN,
  OP_COS,
  OP_TAN,
  OP_SQRT,
  OP_LOG,
  OP_ABS,
  OP_MIN,
  OP_MAX,
  OP_RANDOM,
} opcode_t;

typedef struct {
  opcode_t opcode;
  double number;
  uint32_t id;
} instruction_t;

struct expression {
  instruction_t *code;
  size_t count;
};

typedef struct {
  const char *source;
  const char *cursor;
  expression_t *expression;
  mescaline_error_t *error;
  unsigned depth;
  unsigned random_id;
  size_t stack_size;
} parser_t;

static void skip_space(parser_t *parser) {
  while (isspace((unsigned char)*parser->cursor)) parser->cursor++;
}

static bool fail(parser_t *parser, const char *message) {
  mescaline_error_set(parser->error, MESCALINE_USAGE, 0, "Expression error at byte %zu: %s",
                      (size_t)(parser->cursor - parser->source), message);
  return false;
}

static bool emit(parser_t *parser, opcode_t opcode, double number, uint32_t id, int stack_change) {
  if (parser->expression->count == MESCALINE_MAX_EXPRESSION_INSTRUCTIONS) {
    return fail(parser, "instruction limit exceeded");
  }
  if (stack_change < 0 && parser->stack_size < (size_t)-stack_change) {
    return fail(parser, "invalid expression stack");
  }
  if (stack_change < 0) parser->stack_size -= (size_t)-stack_change;
  else parser->stack_size += (size_t)stack_change;
  if (parser->stack_size > MESCALINE_MAX_EXPRESSION_STACK) {
    return fail(parser, "evaluation stack limit exceeded");
  }
  parser->expression->code[parser->expression->count++] =
      (instruction_t){.opcode = opcode, .number = number, .id = id};
  return true;
}

static bool identifier_is(const char *start, size_t length, const char *name) {
  return strlen(name) == length && strncmp(start, name, length) == 0;
}

static bool parse_expression(parser_t *parser);

static bool parse_function(parser_t *parser, const char *name, size_t length) {
  opcode_t opcode;
  unsigned arguments;
  if (identifier_is(name, length, "sin")) opcode = OP_SIN, arguments = 1;
  else if (identifier_is(name, length, "cos")) opcode = OP_COS, arguments = 1;
  else if (identifier_is(name, length, "tan")) opcode = OP_TAN, arguments = 1;
  else if (identifier_is(name, length, "sqrt")) opcode = OP_SQRT, arguments = 1;
  else if (identifier_is(name, length, "log")) opcode = OP_LOG, arguments = 1;
  else if (identifier_is(name, length, "abs")) opcode = OP_ABS, arguments = 1;
  else if (identifier_is(name, length, "min")) opcode = OP_MIN, arguments = 2;
  else if (identifier_is(name, length, "max")) opcode = OP_MAX, arguments = 2;
  else if (identifier_is(name, length, "pow")) opcode = OP_POWER, arguments = 2;
  else if (identifier_is(name, length, "random")) opcode = OP_RANDOM, arguments = 0;
  else return fail(parser, "unknown function");

  parser->cursor++;
  skip_space(parser);
  if (arguments == 0) {
    if (*parser->cursor != ')') return fail(parser, "random takes no arguments");
    parser->cursor++;
    return emit(parser, opcode, 0.0, parser->random_id++, 1);
  }

  for (unsigned argument = 0; argument < arguments; argument++) {
    if (!parse_expression(parser)) return false;
    skip_space(parser);
    if (argument + 1 < arguments) {
      if (*parser->cursor != ',') return fail(parser, "expected ',' between function arguments");
      parser->cursor++;
    }
  }
  skip_space(parser);
  if (*parser->cursor != ')') return fail(parser, "expected ')' after function arguments");
  parser->cursor++;
  return emit(parser, opcode, 0.0, 0, arguments == 2 ? -1 : 0);
}

static bool parse_primary(parser_t *parser) {
  skip_space(parser);
  if (++parser->depth > MESCALINE_MAX_EXPRESSION_DEPTH) {
    return fail(parser, "nesting limit exceeded");
  }

  bool success = false;
  if (*parser->cursor == '(') {
    parser->cursor++;
    success = parse_expression(parser);
    skip_space(parser);
    if (success && *parser->cursor != ')') success = fail(parser, "expected ')'");
    if (success) parser->cursor++;
  } else if (isdigit((unsigned char)*parser->cursor) ||
             (*parser->cursor == '.' && isdigit((unsigned char)parser->cursor[1]))) {
    errno = 0;
    char *end;
    double number = strtod(parser->cursor, &end);
    if (errno == ERANGE || !isfinite(number)) success = fail(parser, "invalid number");
    else {
      parser->cursor = end;
      success = emit(parser, OP_NUMBER, number, 0, 1);
    }
  } else if (isalpha((unsigned char)*parser->cursor) || *parser->cursor == '_') {
    const char *name = parser->cursor++;
    while (isalnum((unsigned char)*parser->cursor) || *parser->cursor == '_') parser->cursor++;
    size_t length = (size_t)(parser->cursor - name);
    skip_space(parser);
    if (*parser->cursor == '(') success = parse_function(parser, name, length);
    else if (identifier_is(name, length, "px")) success = emit(parser, OP_PX, 0.0, 0, 1);
    else if (identifier_is(name, length, "py")) success = emit(parser, OP_PY, 0.0, 0, 1);
    else if (identifier_is(name, length, "x")) success = emit(parser, OP_X, 0.0, 0, 1);
    else if (identifier_is(name, length, "y")) success = emit(parser, OP_Y, 0.0, 0, 1);
    else if (identifier_is(name, length, "frame")) success = emit(parser, OP_FRAME, 0.0, 0, 1);
    else if (identifier_is(name, length, "t")) success = emit(parser, OP_T, 0.0, 0, 1);
    else if (identifier_is(name, length, "width")) success = emit(parser, OP_WIDTH, 0.0, 0, 1);
    else if (identifier_is(name, length, "height")) success = emit(parser, OP_HEIGHT, 0.0, 0, 1);
    else if (identifier_is(name, length, "seed")) success = emit(parser, OP_SEED, 0.0, 0, 1);
    else if (identifier_is(name, length, "pi"))
      success = emit(parser, OP_NUMBER, 3.14159265358979323846, 0, 1);
    else if (identifier_is(name, length, "e"))
      success = emit(parser, OP_NUMBER, 2.71828182845904523536, 0, 1);
    else success = fail(parser, "unknown identifier");
  } else {
    success = fail(parser, "expected a number, variable, function, or '('");
  }

  parser->depth--;
  return success;
}

static bool parse_unary(parser_t *parser);

static bool parse_power(parser_t *parser) {
  if (!parse_primary(parser)) return false;
  skip_space(parser);
  if (*parser->cursor == '^') {
    if (++parser->depth > MESCALINE_MAX_EXPRESSION_DEPTH) {
      return fail(parser, "nesting limit exceeded");
    }
    parser->cursor++;
    bool success = parse_unary(parser) && emit(parser, OP_POWER, 0.0, 0, -1);
    parser->depth--;
    return success;
  }
  return true;
}

static bool parse_unary(parser_t *parser) {
  skip_space(parser);
  if (*parser->cursor == '+') {
    if (++parser->depth > MESCALINE_MAX_EXPRESSION_DEPTH) {
      return fail(parser, "nesting limit exceeded");
    }
    parser->cursor++;
    bool success = parse_unary(parser);
    parser->depth--;
    return success;
  }
  if (*parser->cursor == '-') {
    if (++parser->depth > MESCALINE_MAX_EXPRESSION_DEPTH) {
      return fail(parser, "nesting limit exceeded");
    }
    parser->cursor++;
    bool success = parse_unary(parser) && emit(parser, OP_NEGATE, 0.0, 0, 0);
    parser->depth--;
    return success;
  }
  return parse_power(parser);
}

static bool parse_product(parser_t *parser) {
  if (!parse_unary(parser)) return false;
  while (true) {
    skip_space(parser);
    opcode_t opcode;
    if (*parser->cursor == '*') opcode = OP_MULTIPLY;
    else if (*parser->cursor == '/') opcode = OP_DIVIDE;
    else if (*parser->cursor == '%') opcode = OP_MODULO;
    else return true;
    parser->cursor++;
    if (!parse_unary(parser) || !emit(parser, opcode, 0.0, 0, -1)) return false;
  }
}

static bool parse_expression(parser_t *parser) {
  if (!parse_product(parser)) return false;
  while (true) {
    skip_space(parser);
    opcode_t opcode;
    if (*parser->cursor == '+') opcode = OP_ADD;
    else if (*parser->cursor == '-') opcode = OP_SUBTRACT;
    else return true;
    parser->cursor++;
    if (!parse_product(parser) || !emit(parser, opcode, 0.0, 0, -1)) return false;
  }
}

bool expression_compile(const char *source, expression_t **result, mescaline_error_t *error) {
  *result = NULL;
  size_t length = strlen(source);
  if (length == 0 || length > MESCALINE_MAX_EXPRESSION_LENGTH) {
    mescaline_error_set(error, MESCALINE_USAGE, 0, "Expression length must be from 1 to %u bytes",
                        MESCALINE_MAX_EXPRESSION_LENGTH);
    return false;
  }

  expression_t *expression = calloc(1, sizeof(*expression));
  if (expression == NULL) {
    mescaline_error_set(error, MESCALINE_INTERNAL, errno, "Could not allocate expression");
    return false;
  }
  expression->code =
      malloc(sizeof(*expression->code) * MESCALINE_MAX_EXPRESSION_INSTRUCTIONS);
  if (expression->code == NULL) {
    expression_destroy(expression);
    mescaline_error_set(error, MESCALINE_INTERNAL, errno, "Could not allocate expression code");
    return false;
  }

  parser_t parser = {.source = source, .cursor = source, .expression = expression, .error = error};
  bool success = parse_expression(&parser);
  skip_space(&parser);
  if (success && *parser.cursor != '\0') success = fail(&parser, "unexpected character");
  if (success && parser.stack_size != 1) success = fail(&parser, "invalid expression");
  if (!success) {
    expression_destroy(expression);
    return false;
  }
  *result = expression;
  return true;
}

static uint64_t mix(uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

static double deterministic_random(const expression_context_t *context, uint32_t id) {
  uint64_t value = context->seed;
  value ^= mix(context->px);
  value ^= mix((uint64_t)context->py << 1);
  value ^= mix((uint64_t)context->frame << 2);
  value ^= mix((uint64_t)context->stream << 3);
  value ^= mix((uint64_t)id << 4);
  return (mix(value) >> 11) * 0x1.0p-53;
}

double expression_evaluate(const expression_t *expression, const expression_context_t *context,
                           bool *nonfinite) {
  double stack[MESCALINE_MAX_EXPRESSION_STACK];
  size_t size = 0;
  for (size_t i = 0; i < expression->count; i++) {
    instruction_t instruction = expression->code[i];
    double right;
    switch (instruction.opcode) {
      case OP_NUMBER: stack[size++] = instruction.number; break;
      case OP_PX: stack[size++] = context->px; break;
      case OP_PY: stack[size++] = context->py; break;
      case OP_X:
        stack[size++] = context->width == 1 ? 0.0 : (double)context->px / (context->width - 1);
        break;
      case OP_Y:
        stack[size++] = context->height == 1 ? 0.0 : (double)context->py / (context->height - 1);
        break;
      case OP_FRAME: stack[size++] = context->frame; break;
      case OP_T: stack[size++] = (double)context->frame / context->frames; break;
      case OP_WIDTH: stack[size++] = context->width; break;
      case OP_HEIGHT: stack[size++] = context->height; break;
      case OP_SEED: stack[size++] = (double)context->seed; break;
      case OP_ADD: right = stack[--size]; stack[size - 1] += right; break;
      case OP_SUBTRACT: right = stack[--size]; stack[size - 1] -= right; break;
      case OP_MULTIPLY: right = stack[--size]; stack[size - 1] *= right; break;
      case OP_DIVIDE: right = stack[--size]; stack[size - 1] /= right; break;
      case OP_MODULO: right = stack[--size]; stack[size - 1] = fmod(stack[size - 1], right); break;
      case OP_POWER: right = stack[--size]; stack[size - 1] = pow(stack[size - 1], right); break;
      case OP_NEGATE: stack[size - 1] = -stack[size - 1]; break;
      case OP_SIN: stack[size - 1] = sin(stack[size - 1]); break;
      case OP_COS: stack[size - 1] = cos(stack[size - 1]); break;
      case OP_TAN: stack[size - 1] = tan(stack[size - 1]); break;
      case OP_SQRT: stack[size - 1] = sqrt(stack[size - 1]); break;
      case OP_LOG: stack[size - 1] = log(stack[size - 1]); break;
      case OP_ABS: stack[size - 1] = fabs(stack[size - 1]); break;
      case OP_MIN: right = stack[--size]; stack[size - 1] = fmin(stack[size - 1], right); break;
      case OP_MAX: right = stack[--size]; stack[size - 1] = fmax(stack[size - 1], right); break;
      case OP_RANDOM: stack[size++] = deterministic_random(context, instruction.id); break;
    }
  }
  *nonfinite = !isfinite(stack[0]);
  return *nonfinite ? 0.0 : stack[0];
}

void expression_destroy(expression_t *expression) {
  if (expression == NULL) return;
  free(expression->code);
  free(expression);
}
