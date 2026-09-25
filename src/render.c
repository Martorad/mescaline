#include "render.h"

#include "algorithms.h"
#include "expression.h"
#include "palette.h"

#include <math.h>
#include <omp.h>
#include <stdlib.h>

bool image_create(image_t *image, uint32_t width, uint32_t height, mescaline_error_t *error) {
  uint64_t pixels = (uint64_t)width * height;
  if (width == 0 || height == 0 || width > MESCALINE_MAX_DIMENSION ||
      height > MESCALINE_MAX_DIMENSION || pixels > MESCALINE_MAX_PIXELS ||
      pixels > SIZE_MAX / 3) {
    mescaline_error_set(error, MESCALINE_RENDER, 0, "Image dimensions are too large");
    return false;
  }

  image->width = width;
  image->height = height;
  image->size = (size_t)pixels * 3;
  image->pixels = malloc(image->size);
  if (image->pixels == NULL) {
    mescaline_error_set(error, MESCALINE_RENDER, 0, "Could not allocate %zu image bytes",
                        image->size);
    return false;
  }
  return true;
}

void image_destroy(image_t *image) {
  free(image->pixels);
  *image = (image_t){0};
}

uint32_t render_worker_count(const render_spec_t *spec) {
  int processors = omp_get_num_procs();
  uint32_t workers = spec->threads == 0 && processors > 0 ? (uint32_t)processors : spec->threads;
  if (workers == 0) workers = 1;
  if (workers > MESCALINE_MAX_THREADS) workers = MESCALINE_MAX_THREADS;
  int thread_limit = omp_get_thread_limit();
  if (thread_limit > 0 && workers > (uint32_t)thread_limit) workers = (uint32_t)thread_limit;
  return workers < spec->height ? workers : spec->height;
}

static uint64_t render_expression_row(const render_spec_t *spec, uint32_t frame, uint32_t frames,
                                      uint32_t y, uint8_t *row) {
  uint64_t nonfinite_count = 0;
  expression_context_t context = {
      .py = y,
      .width = spec->width,
      .height = spec->height,
      .frame = frame,
      .frames = frames,
      .seed = spec->seed,
  };

  for (uint32_t x = 0; x < spec->width; x++) {
    context.px = x;
    uint8_t *pixel = row + (size_t)x * 3;
    unsigned channels = spec->mode == RENDER_EXPRESSION_SCALAR ? 1 : 3;
    uint8_t values[3] = {0};
    for (unsigned channel = 0; channel < channels; channel++) {
      context.stream = channel;
      bool nonfinite;
      double value = expression_evaluate(spec->expressions[channel], &context, &nonfinite);
      double scaled = value * 255.0;
      if (nonfinite || !isfinite(scaled)) {
        nonfinite_count++;
        scaled = 0.0;
      }
      values[channel] = range_map_channel(scaled, spec->range_mode);
    }

    if (spec->mode == RENDER_EXPRESSION_SCALAR) {
      palette_apply(spec->palette, values[0], spec->color, pixel);
    } else {
      for (unsigned channel = 0; channel < 3; channel++) pixel[channel] = values[channel];
    }
  }
  return nonfinite_count;
}

render_result_t render_frame(const render_spec_t *spec, uint32_t frame, uint32_t frames,
                             image_t *image, uint64_t *nonfinite_count,
                             const volatile sig_atomic_t *cancel_signal,
                             mescaline_error_t *error) {
  return render_frame_stream(spec, frame, frames, image, nonfinite_count, cancel_signal, NULL,
                             NULL, error);
}

render_result_t render_frame_stream(const render_spec_t *spec, uint32_t frame, uint32_t frames,
                                    image_t *image, uint64_t *nonfinite_count,
                                    const volatile sig_atomic_t *cancel_signal,
                                    render_row_callback_t callback, void *context,
                                    mescaline_error_t *error) {
  if (image->width != spec->width || image->height != spec->height || image->pixels == NULL) {
    mescaline_error_set(error, MESCALINE_RENDER, 0, "Render buffer does not match the canvas");
    return RENDER_FAILED;
  }

  int workers = (int)render_worker_count(spec);
  uint64_t invalid = 0;
  int stream_failed = 0;
  omp_set_dynamic(0);
#pragma omp parallel for schedule(static) num_threads(workers) reduction(+ : invalid) reduction(| : stream_failed)
  for (int64_t y = 0; y < spec->height; y++) {
    if (*cancel_signal == 0) {
      uint8_t *row = image->pixels + (size_t)y * spec->width * 3;
      if (spec->mode == RENDER_BUILTIN) algorithm_render_row(spec, frame, (uint32_t)y, row);
      else invalid += render_expression_row(spec, frame, frames, (uint32_t)y, row);
      if (callback != NULL) {
        // ponytail: serialize row packets; batch writes if pipe throughput limits rendering.
#pragma omp critical(mescaline_preview_stream)
        {
          if (!callback((uint32_t)y, row, (size_t)spec->width * 3, context)) stream_failed = 1;
        }
      }
    }
  }
  *nonfinite_count = invalid;
  if (stream_failed != 0 && *cancel_signal == 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Could not write preview stream");
    return RENDER_FAILED;
  }
  return *cancel_signal == 0 ? RENDER_OK : RENDER_CANCELLED;
}
