#include "render.h"

#include "algorithms.h"

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

render_result_t render_frame(const render_spec_t *spec, uint32_t frame, image_t *image,
                             const volatile sig_atomic_t *cancel_signal,
                             mescaline_error_t *error) {
  if (image->width != spec->width || image->height != spec->height || image->pixels == NULL) {
    mescaline_error_set(error, MESCALINE_RENDER, 0, "Render buffer does not match the canvas");
    return RENDER_FAILED;
  }

  int workers = (int)render_worker_count(spec);
  omp_set_dynamic(0);
#pragma omp parallel for schedule(static) num_threads(workers)
  for (int64_t y = 0; y < spec->height; y++) {
    if (*cancel_signal == 0) {
      algorithm_render_row(spec, frame, (uint32_t)y,
                           image->pixels + (size_t)y * spec->width * 3);
    }
  }
  return *cancel_signal == 0 ? RENDER_OK : RENDER_CANCELLED;
}
