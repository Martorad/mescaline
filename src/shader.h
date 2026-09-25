#ifndef MESCALINE_H
#define MESCALINE_H

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MESCALINE_MAX_DIMENSION 100000U
#define MESCALINE_MAX_PIXELS 100000000ULL
#define MESCALINE_MAX_FRAMES 1000U
#define MESCALINE_MAX_FPS 1000U
#define MESCALINE_MAX_THREADS 1024U

typedef enum {
  MESCALINE_OK = 0,
  MESCALINE_INTERNAL = 1,
  MESCALINE_USAGE = 2,
  MESCALINE_OUTPUT = 3,
  MESCALINE_RENDER = 4,
  MESCALINE_ENCODER = 5,
} mescaline_status_t;

typedef enum {
  ALGORITHM_CHECKERBOARD,
  ALGORITHM_LASAGNA,
  ALGORITHM_CARREAUX,
} algorithm_t;

typedef enum {
  RENDER_BUILTIN,
  RENDER_EXPRESSION_SCALAR,
  RENDER_EXPRESSION_RGB,
} render_mode_t;

typedef enum {
  RANGE_WRAP,
  RANGE_CLAMP,
} range_mode_t;

typedef enum {
  PALETTE_GRAYSCALE,
  PALETTE_MONOCHROME,
  PALETTE_VIRIDIS,
  PALETTE_PLASMA,
  PALETTE_MAGMA,
  PALETTE_INFERNO,
  PALETTE_TURBO,
} palette_t;

typedef struct expression expression_t;

typedef enum {
  PROGRESS_TEXT,
  PROGRESS_JSON,
  PROGRESS_NONE,
} progress_mode_t;

typedef struct {
  uint32_t width;
  uint32_t height;
  render_mode_t mode;
  algorithm_t algorithm;
  range_mode_t range_mode;
  palette_t palette;
  const expression_t *expressions[3];
  uint64_t seed;
  uint32_t threads;
  double scale;
  uint8_t color[3];
} render_spec_t;

typedef struct {
  render_spec_t render;
  const char *output;
  uint32_t frames;
  uint32_t fps;
  progress_mode_t progress;
  bool force;
  bool preview_stream;
  const char *expression_sources[3];
} mescaline_options_t;

typedef struct {
  mescaline_status_t status;
  int system_error;
  char message[512];
} mescaline_error_t;

static inline void mescaline_error_set(mescaline_error_t *error, mescaline_status_t status,
                                       int system_error, const char *format, ...) {
  va_list args;
  error->status = status;
  error->system_error = system_error;
  va_start(args, format);
  vsnprintf(error->message, sizeof(error->message), format, args);
  va_end(args);
}

#endif
