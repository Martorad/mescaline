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

typedef bool (*render_row_callback_t)(uint32_t y, const uint8_t *row, size_t length, void *context);

bool image_create(image_t *image, uint32_t width, uint32_t height, mescaline_error_t *error);
void image_destroy(image_t *image);
uint32_t render_worker_count(const render_spec_t *spec);
render_result_t render_frame(const render_spec_t *spec, uint32_t frame, uint32_t frames,
                             image_t *image, uint64_t *nonfinite_count,
                             const volatile sig_atomic_t *cancel_signal,
                             mescaline_error_t *error);
render_result_t render_frame_stream(const render_spec_t *spec, uint32_t frame, uint32_t frames,
                                    image_t *image, uint64_t *nonfinite_count,
                                    const volatile sig_atomic_t *cancel_signal,
                                    render_row_callback_t callback, void *context,
                                    mescaline_error_t *error);

#endif
