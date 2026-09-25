#ifndef MESCALINE_PROGRESS_H
#define MESCALINE_PROGRESS_H

#include "output.h"
#include "shader.h"

typedef struct {
  progress_mode_t mode;
  FILE *stream;
} progress_t;

void progress_init(progress_t *progress, progress_mode_t mode, FILE *stream);
void progress_start(progress_t *progress, const mescaline_options_t *options, output_kind_t kind);
void progress_frame(progress_t *progress, uint32_t frame, uint32_t total);
void progress_nonfinite(progress_t *progress, uint64_t count);
void progress_encoding(progress_t *progress);
void progress_complete(progress_t *progress, const char *path);
void progress_cancelled(progress_t *progress, int signal_number);
void progress_error(progress_t *progress, const mescaline_error_t *error);

#endif
