#ifndef MESCALINE_ENCODE_H
#define MESCALINE_ENCODE_H

#include "shader.h"

typedef enum {
  ENCODE_OK,
  ENCODE_CANCELLED,
  ENCODE_FAILED,
} encode_result_t;

encode_result_t ffmpeg_encode(const char *frame_pattern, const char *temporary_gif,
                              const char *log_path, uint32_t frames, uint32_t fps,
                              const volatile sig_atomic_t *cancel_signal,
                              mescaline_error_t *error);

#endif
