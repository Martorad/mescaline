#include "progress.h"

#include "algorithms.h"

#include <string.h>

static void json_string(FILE *stream, const char *value) {
  fputc('"', stream);
  for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; cursor++) {
    switch (*cursor) {
      case '"': fputs("\\\"", stream); break;
      case '\\': fputs("\\\\", stream); break;
      case '\b': fputs("\\b", stream); break;
      case '\f': fputs("\\f", stream); break;
      case '\n': fputs("\\n", stream); break;
      case '\r': fputs("\\r", stream); break;
      case '\t': fputs("\\t", stream); break;
      default:
        if (*cursor < 0x20) fprintf(stream, "\\u%04x", *cursor);
        else fputc(*cursor, stream);
    }
  }
  fputc('"', stream);
}

static const char *kind_name(output_kind_t kind) {
  switch (kind) {
    case OUTPUT_PPM: return "ppm";
    case OUTPUT_SEQUENCE: return "sequence";
    case OUTPUT_GIF: return "gif";
  }
  return "unknown";
}

void progress_init(progress_t *progress, progress_mode_t mode, FILE *stream) {
  progress->mode = mode;
  progress->stream = stream;
}

void progress_start(progress_t *progress, const mescaline_options_t *options, output_kind_t kind) {
  if (progress->mode == PROGRESS_NONE) return;
  if (progress->mode == PROGRESS_JSON) {
    fprintf(progress->stream,
            "{\"version\":1,\"event\":\"start\",\"algorithm\":\"%s\",\"width\":%u,"
            "\"height\":%u,\"frames\":%u,\"output_kind\":\"%s\"}\n",
            algorithm_name(options->render.algorithm), options->render.width, options->render.height,
            options->frames, kind_name(kind));
  } else {
    fprintf(progress->stream, "Rendering %u frame%s of %s at %ux%u...\n", options->frames,
            options->frames == 1 ? "" : "s", algorithm_name(options->render.algorithm),
            options->render.width, options->render.height);
  }
  fflush(progress->stream);
}

void progress_frame(progress_t *progress, uint32_t frame, uint32_t total) {
  if (progress->mode == PROGRESS_NONE) return;
  if (progress->mode == PROGRESS_JSON) {
    fprintf(progress->stream,
            "{\"version\":1,\"event\":\"frame\",\"index\":%u,\"completed\":%u,"
            "\"total\":%u}\n",
            frame, frame + 1, total);
  } else {
    fprintf(progress->stream, "Rendered frame %u/%u\n", frame + 1, total);
  }
  fflush(progress->stream);
}

void progress_encoding(progress_t *progress) {
  if (progress->mode == PROGRESS_NONE) return;
  if (progress->mode == PROGRESS_JSON) {
    fputs("{\"version\":1,\"event\":\"encoding\"}\n", progress->stream);
  } else {
    fputs("Encoding GIF...\n", progress->stream);
  }
  fflush(progress->stream);
}

void progress_complete(progress_t *progress, const char *path) {
  if (progress->mode == PROGRESS_NONE) return;
  if (progress->mode == PROGRESS_JSON) {
    fputs("{\"version\":1,\"event\":\"complete\",\"output\":", progress->stream);
    json_string(progress->stream, path);
    fputs("}\n", progress->stream);
  } else {
    fprintf(progress->stream, "Created %s\n", path);
  }
  fflush(progress->stream);
}

void progress_cancelled(progress_t *progress, int signal_number) {
  if (progress->mode == PROGRESS_JSON) {
    fprintf(progress->stream,
            "{\"version\":1,\"event\":\"cancelled\",\"signal\":%d}\n", signal_number);
  } else {
    fprintf(progress->stream, "Cancelled by signal %d\n", signal_number);
  }
  fflush(progress->stream);
}

void progress_error(progress_t *progress, const mescaline_error_t *error) {
  const char *system_message = error->system_error == 0 ? NULL : strerror(error->system_error);
  if (progress->mode == PROGRESS_JSON) {
    fprintf(progress->stream, "{\"version\":1,\"event\":\"error\",\"status\":%d,\"message\":",
            error->status);
    json_string(progress->stream, error->message);
    if (system_message != NULL) {
      fputs(",\"system\":", progress->stream);
      json_string(progress->stream, system_message);
    }
    fputs("}\n", progress->stream);
  } else {
    fprintf(progress->stream, "Error: %s", error->message);
    if (system_message != NULL) fprintf(progress->stream, ": %s", system_message);
    fputc('\n', progress->stream);
  }
  fflush(progress->stream);
}
