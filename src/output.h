#ifndef MESCALINE_OUTPUT_H
#define MESCALINE_OUTPUT_H

#include "render.h"
#include "shader.h"

typedef enum {
  OUTPUT_PPM,
  OUTPUT_SEQUENCE,
  OUTPUT_GIF,
} output_kind_t;

typedef enum {
  OUTPUT_RESULT_OK,
  OUTPUT_RESULT_CANCELLED,
  OUTPUT_RESULT_ERROR,
} output_result_t;

typedef struct output_plan output_plan_t;

bool output_prepare(output_plan_t **plan, const mescaline_options_t *options,
                    mescaline_error_t *error);
output_kind_t output_kind(const output_plan_t *plan);
const char *output_path(const output_plan_t *plan);
const char *output_frame_pattern(const output_plan_t *plan);
const char *output_temporary_gif(const output_plan_t *plan);
const char *output_encoder_log(const output_plan_t *plan);
const char *output_staging_directory(const output_plan_t *plan);
output_result_t output_write_frame(output_plan_t *plan, uint32_t frame, const image_t *image,
                                   const volatile sig_atomic_t *cancel_signal,
                                   mescaline_error_t *error);
output_result_t output_publish_sequence(output_plan_t *plan,
                                        const volatile sig_atomic_t *cancel_signal,
                                        mescaline_error_t *error);
bool output_publish_gif(output_plan_t *plan, mescaline_error_t *error);
bool output_cleanup(output_plan_t *plan, bool retain_staging, mescaline_error_t *error);
void output_destroy(output_plan_t *plan);

#endif
