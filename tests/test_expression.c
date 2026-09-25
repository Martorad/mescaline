#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>

#include "expression.h"
#include "palette.h"

static double evaluate(const char *source, const expression_context_t *context) {
  mescaline_error_t error = {0};
  expression_t *expression = NULL;
  assert(expression_compile(source, &expression, &error));
  bool nonfinite;
  double value = expression_evaluate(expression, context, &nonfinite);
  assert(!nonfinite);
  expression_destroy(expression);
  return value;
}

int main(void) {
  expression_context_t context = {
      .px = 2,
      .py = 3,
      .width = 5,
      .height = 7,
      .frame = 1,
      .frames = 4,
      .seed = 42,
  };

  assert(evaluate("1 + 2 * 3", &context) == 7.0);
  assert(evaluate("2^3^2", &context) == 512.0);
  assert(evaluate("-2^2", &context) == -4.0);
  assert(evaluate("10 % 4", &context) == 2.0);
  assert(evaluate("min(8, 3) + max(2, 5) + pow(2, 3)", &context) == 16.0);
  assert(evaluate("min(0.5; 1.5) + max(2; 5)", &context) == 5.5);
  assert(fabs(evaluate("sin(pi / 2) + cos(0) + abs(-2)", &context) - 4.0) < 1e-12);
  assert(fabs(evaluate("sqrt(9) + log(e) + tan(0)", &context) - 4.0) < 1e-12);
  assert(fabs(evaluate("x + y + t", &context) - 1.25) < 1e-12);
  assert(evaluate("px + py + width + height + frame + seed", &context) == 60.0);

  double random = evaluate("random() + random()", &context);
  assert(random == evaluate("random() + random()", &context));
  context.px++;
  double next_pixel = evaluate("random() + random()", &context);
  assert(random != next_pixel);
  context.stream++;
  assert(next_pixel != evaluate("random() + random()", &context));

  mescaline_error_t nonfinite_error = {0};
  expression_t *nonfinite_expression = NULL;
  assert(expression_compile("1 / 0", &nonfinite_expression, &nonfinite_error));
  bool nonfinite;
  assert(expression_evaluate(nonfinite_expression, &context, &nonfinite) == 0.0);
  assert(nonfinite);
  expression_destroy(nonfinite_expression);

  const char *invalid[] = {"", "1 +", "unknown", "sin()", "min(1)", "random(1)",
                           "(1", "1,2", "1 2", "1e999"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
    mescaline_error_t error = {0};
    expression_t *expression = NULL;
    assert(!expression_compile(invalid[i], &expression, &error));
    assert(expression == NULL);
  }

  char nested[80];
  memset(nested, '-', sizeof(nested) - 2);
  nested[sizeof(nested) - 2] = '1';
  nested[sizeof(nested) - 1] = '\0';
  mescaline_error_t error = {0};
  expression_t *expression = NULL;
  assert(!expression_compile(nested, &expression, &error));

  char many_operations[2200];
  size_t offset = 0;
  for (unsigned i = 0; i < 1100; i++) {
    many_operations[offset++] = '1';
    if (i != 1099) many_operations[offset++] = '+';
  }
  many_operations[offset] = '\0';
  assert(!expression_compile(many_operations, &expression, &error));

  char many_powers[160];
  offset = 0;
  for (unsigned i = 0; i < 70; i++) {
    many_powers[offset++] = '1';
    if (i != 69) many_powers[offset++] = '^';
  }
  many_powers[offset] = '\0';
  assert(!expression_compile(many_powers, &expression, &error));

  char too_long[MESCALINE_MAX_EXPRESSION_LENGTH + 2];
  memset(too_long, '1', sizeof(too_long) - 1);
  too_long[sizeof(too_long) - 1] = '\0';
  assert(!expression_compile(too_long, &expression, &error));

  uint8_t pixel[3];
  const uint8_t white[3] = {255, 255, 255};
  palette_apply(PALETTE_VIRIDIS, 0, white, pixel);
  assert(pixel[0] == 68 && pixel[1] == 1 && pixel[2] == 84);
  palette_apply(PALETTE_VIRIDIS, 255, white, pixel);
  assert(pixel[0] == 253 && pixel[1] == 231 && pixel[2] == 37);
  return 0;
}
