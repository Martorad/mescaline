#ifndef MESCALINE_RENDER_H
#define MESCALINE_RENDER_H

#include "shader.h"

#include <stddef.h>

typedef struct {
  uint32_t width;
  uint32_t height;
  size_t size;
  uint8_t *pixels;
} image_t;

typedef enum {
  RENDER_OK,
  RENDER_CANCELLED,
  RENDER_FAILED,
} render_result_t;

bool image_create(image_t *image, uint32_t width, uint32_t height, mescaline_error_t *error);
void image_destroy(image_t *image);
render_result_t render_frame(const render_spec_t *spec, uint32_t frame, image_t *image,
                             const volatile sig_atomic_t *cancel_signal,
                             mescaline_error_t *error);

#endif
