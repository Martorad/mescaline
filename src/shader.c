#include "shader.h"

#include "cli.h"
#include "encode.h"
#include "expression.h"
#include "output.h"
#include "progress.h"
#include "render.h"

#include <errno.h>
#include <string.h>

static volatile sig_atomic_t cancel_signal;

static void handle_signal(int signal_number) {
  if (cancel_signal == 0) cancel_signal = signal_number;
}

static bool install_signal_handlers(mescaline_error_t *error) {
  struct sigaction action = {.sa_handler = handle_signal};
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGTERM, &action, NULL) != 0) {
    mescaline_error_set(error, MESCALINE_INTERNAL, errno, "Could not install signal handlers");
    return false;
  }
  return true;
}

static void retain_staging_message(mescaline_error_t *error, const output_plan_t *plan) {
  char original[sizeof(error->message)];
  memcpy(original, error->message, sizeof(original));
  original[sizeof(original) - 1] = '\0';
  mescaline_error_set(error, error->status, error->system_error, "%s; frames retained in '%s'",
                      original, output_staging_directory(plan));
}

static void destroy_expressions(expression_t *expressions[3]) {
  for (size_t i = 0; i < 3; i++) expression_destroy(expressions[i]);
}

int main(int argc, char **argv) {
  mescaline_options_t options;
  mescaline_error_t error = {0};
  cli_result_t cli_result = cli_parse(argc, argv, &options, &error);
  if (cli_result == CLI_HELP) {
    cli_print_usage(stdout, argv[0]);
    return MESCALINE_OK;
  }

  progress_t progress;
  progress_init(&progress, options.progress, stderr);
  if (cli_result == CLI_ERROR) {
    progress_error(&progress, &error);
    return error.status;
  }
  if (!install_signal_handlers(&error)) {
    progress_error(&progress, &error);
    return error.status;
  }

  expression_t *expressions[3] = {0};
  unsigned expression_count = options.render.mode == RENDER_EXPRESSION_RGB
                                  ? 3
                                  : options.render.mode == RENDER_EXPRESSION_SCALAR ? 1 : 0;
  for (unsigned i = 0; i < expression_count; i++) {
    if (!expression_compile(options.expression_sources[i], &expressions[i], &error)) {
      destroy_expressions(expressions);
      progress_error(&progress, &error);
      return error.status;
    }
    options.render.expressions[i] = expressions[i];
  }

  image_t image = {0};
  if (!image_create(&image, options.render.width, options.render.height, &error)) {
    destroy_expressions(expressions);
    progress_error(&progress, &error);
    return error.status;
  }

  output_plan_t *output = NULL;
  if (!output_prepare(&output, &options, &error)) {
    image_destroy(&image);
    destroy_expressions(expressions);
    progress_error(&progress, &error);
    return error.status;
  }

  bool retain_staging = false;
  bool output_committed = false;
  uint64_t nonfinite_count = 0;
  progress_start(&progress, &options, output_kind(output));
  for (uint32_t frame = 0; frame < options.frames; frame++) {
    uint64_t frame_nonfinite = 0;
    render_result_t render_result =
        render_frame(&options.render, frame, options.frames, &image, &frame_nonfinite,
                     &cancel_signal, &error);
    if (render_result == RENDER_CANCELLED) goto cancelled;
    if (render_result == RENDER_FAILED) goto failed;

    output_result_t output_result =
        output_write_frame(output, frame, &image, &cancel_signal, &error);
    if (output_result == OUTPUT_RESULT_CANCELLED) goto cancelled;
    if (output_result == OUTPUT_RESULT_ERROR) goto failed;
    nonfinite_count += frame_nonfinite;
    progress_frame(&progress, frame, options.frames);
    if (output_kind(output) == OUTPUT_PPM) output_committed = true;
  }

  progress_nonfinite(&progress, nonfinite_count);

  if (!output_committed && cancel_signal != 0) goto cancelled;
  if (output_kind(output) == OUTPUT_SEQUENCE) {
    output_result_t publish_result = output_publish_sequence(output, &cancel_signal, &error);
    if (publish_result == OUTPUT_RESULT_CANCELLED) goto cancelled;
    if (publish_result == OUTPUT_RESULT_ERROR) goto failed;
    output_committed = true;
  }

  if (output_kind(output) == OUTPUT_GIF) {
    progress_encoding(&progress);
    encode_result_t encode_result =
        ffmpeg_encode(output_frame_pattern(output), output_temporary_gif(output),
                      output_encoder_log(output), options.frames, options.fps, &cancel_signal,
                      &error);
    if (encode_result == ENCODE_CANCELLED) goto cancelled;
    if (encode_result == ENCODE_FAILED) {
      retain_staging = true;
      retain_staging_message(&error, output);
      goto failed;
    }
    if (cancel_signal != 0) goto cancelled;
    if (!output_publish_gif(output, &error)) {
      retain_staging = true;
      retain_staging_message(&error, output);
      goto failed;
    }
    output_committed = true;
  }

  if (!output_committed && cancel_signal != 0) goto cancelled;
  if (!output_cleanup(output, false, &error)) goto failed;
  progress_complete(&progress, output_path(output));
  output_destroy(output);
  image_destroy(&image);
  destroy_expressions(expressions);
  return MESCALINE_OK;

cancelled: {
  int signal_number = cancel_signal;
  mescaline_error_t cleanup_error = {0};
  output_cleanup(output, false, &cleanup_error);
  output_destroy(output);
  image_destroy(&image);
  destroy_expressions(expressions);
  progress_cancelled(&progress, signal_number);
  return 128 + signal_number;
}

failed: {
  mescaline_status_t status = error.status;
  mescaline_error_t cleanup_error = {0};
  output_cleanup(output, retain_staging, &cleanup_error);
  progress_error(&progress, &error);
  output_destroy(output);
  image_destroy(&image);
  destroy_expressions(expressions);
  return status;
}
}
