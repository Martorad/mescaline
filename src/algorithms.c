#include "algorithms.h"

#include <math.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

bool algorithm_from_name(const char *name, algorithm_t *algorithm) {
  if (strcmp(name, "checkerboard") == 0) *algorithm = ALGORITHM_CHECKERBOARD;
  else if (strcmp(name, "lasagna") == 0) *algorithm = ALGORITHM_LASAGNA;
  else if (strcmp(name, "carreaux") == 0) *algorithm = ALGORITHM_CARREAUX;
  else return false;
  return true;
}

const char *algorithm_name(algorithm_t algorithm) {
  switch (algorithm) {
    case ALGORITHM_CHECKERBOARD: return "checkerboard";
    case ALGORITHM_LASAGNA: return "lasagna";
    case ALGORITHM_CARREAUX: return "carreaux";
  }
  return "unknown";
}

bool range_mode_from_name(const char *name, range_mode_t *range_mode) {
  if (strcmp(name, "wrap") == 0) *range_mode = RANGE_WRAP;
  else if (strcmp(name, "clamp") == 0) *range_mode = RANGE_CLAMP;
  else return false;
  return true;
}

const char *range_mode_name(range_mode_t range_mode) {
  switch (range_mode) {
    case RANGE_WRAP: return "wrap";
    case RANGE_CLAMP: return "clamp";
  }
  return "unknown";
}

uint8_t range_map_channel(double value, range_mode_t range_mode) {
  if (!isfinite(value)) return 0;
  value = trunc(value);
  if (range_mode == RANGE_CLAMP) {
    if (value <= 0.0) return 0;
    if (value >= 255.0) return 255;
    return (uint8_t)value;
  }

  value = fmod(value, 256.0);
  if (value < 0.0) value += 256.0;
  return (uint8_t)value;
}

static void tint(uint8_t value, const uint8_t color[3], uint8_t *pixel) {
  for (size_t i = 0; i < 3; i++) pixel[i] = (uint8_t)(value * color[i] / 255U);
}

void algorithm_render_row(const render_spec_t *spec, uint32_t frame, uint32_t y, uint8_t *row) {
  double x_factor = PI * spec->scale / spec->width;
  double y_norm = y * PI * spec->scale / spec->height;
  double delta = frame * PI / 32.0;

  for (uint32_t x = 0; x < spec->width; x++) {
    uint8_t value = 0;
    double x_norm = x * x_factor;

    switch (spec->algorithm) {
      case ALGORITHM_CHECKERBOARD:
        value = ((x / 10 + (y / 10) % 2) % 2 == 0) ? 255 : 0;
        break;
      case ALGORITHM_LASAGNA:
        value = range_map_channel(
            (cos(x_norm * 2.0 + y_norm) + tan(y_norm * 1.45)) * 128.0 + 128.0,
            spec->range_mode);
        break;
      case ALGORITHM_CARREAUX:
        value = range_map_channel(tan(y_norm) * cos(x_norm + PI / 2.0 + delta) /
                                          sin(x_norm + PI / 2.0) *
                                          128.0 +
                                      128.0,
                                  spec->range_mode);
        break;
    }

    tint(value, spec->color, row + (size_t)x * 3);
  }
}
