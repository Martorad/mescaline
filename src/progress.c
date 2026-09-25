#include "progress.h"

#include "algorithms.h"
#include "palette.h"
#include "render.h"

#include <string.h>

static size_t utf8_length(const unsigned char *value) {
  if (value[0] >= 0xc2 && value[0] <= 0xdf && value[1] >= 0x80 && value[1] <= 0xbf) return 2;
  if (value[0] == 0xe0 && value[1] >= 0xa0 && value[1] <= 0xbf && value[2] >= 0x80 &&
      value[2] <= 0xbf) {
    return 3;
  }
  if (((value[0] >= 0xe1 && value[0] <= 0xec) || (value[0] >= 0xee && value[0] <= 0xef)) &&
      value[1] >= 0x80 && value[1] <= 0xbf && value[2] >= 0x80 && value[2] <= 0xbf) {
    return 3;
  }
  if (value[0] == 0xed && value[1] >= 0x80 && value[1] <= 0x9f && value[2] >= 0x80 &&
      value[2] <= 0xbf) {
    return 3;
  }
  if (value[0] == 0xf0 && value[1] >= 0x90 && value[1] <= 0xbf && value[2] >= 0x80 &&
      value[2] <= 0xbf && value[3] >= 0x80 && value[3] <= 0xbf) {
    return 4;
  }
  if (value[0] >= 0xf1 && value[0] <= 0xf3 && value[1] >= 0x80 && value[1] <= 0xbf &&
      value[2] >= 0x80 && value[2] <= 0xbf && value[3] >= 0x80 && value[3] <= 0xbf) {
    return 4;
  }
  if (value[0] == 0xf4 && value[1] >= 0x80 && value[1] <= 0x8f && value[2] >= 0x80 &&
      value[2] <= 0xbf && value[3] >= 0x80 && value[3] <= 0xbf) {
    return 4;
  }
  return 0;
}

static void json_string(FILE *stream, const char *value) {
  fputc('"', stream);
  for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0';) {
    switch (*cursor) {
      case '"': fputs("\\\"", stream); cursor++; break;
      case '\\': fputs("\\\\", stream); cursor++; break;
      case '\b': fputs("\\b", stream); cursor++; break;
      case '\f': fputs("\\f", stream); cursor++; break;
      case '\n': fputs("\\n", stream); cursor++; break;
      case '\r': fputs("\\r", stream); cursor++; break;
      case '\t': fputs("\\t", stream); cursor++; break;
      default:
        if (*cursor < 0x20) {
          fprintf(stream, "\\u%04x", *cursor++);
        } else if (*cursor < 0x80) {
          fputc(*cursor++, stream);
        } else {
          size_t length = utf8_length(cursor);
          if (length == 0) {
            fputs("\\ufffd", stream);
            cursor++;
          } else {
            fwrite(cursor, 1, length, stream);
            cursor += length;
          }
        }
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

static const char *mode_name(render_mode_t mode) {
  switch (mode) {
    case RENDER_BUILTIN: return "algorithm";
    case RENDER_EXPRESSION_SCALAR: return "scalar";
    case RENDER_EXPRESSION_RGB: return "rgb";
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
            "{\"version\":1,\"event\":\"start\",\"mode\":\"%s\","
            "\"algorithm\":",
            mode_name(options->render.mode));
    if (options->render.mode == RENDER_BUILTIN) {
      fprintf(progress->stream, "\"%s\"", algorithm_name(options->render.algorithm));
    } else {
      fputs("null", progress->stream);
    }
    fputs(",\"palette\":", progress->stream);
    if (options->render.mode == RENDER_EXPRESSION_SCALAR) {
      fprintf(progress->stream, "\"%s\"", palette_name(options->render.palette));
    } else {
      fputs("null", progress->stream);
    }
    fprintf(progress->stream,
            ",\"range_mode\":\"%s\",\"width\":%u,\"height\":%u,\"frames\":%u,"
            "\"threads\":%u,\"output_kind\":\"%s\"}\n",
            range_mode_name(options->render.range_mode), options->render.width,
            options->render.height, options->frames,
            render_worker_count(&options->render), kind_name(kind));
  } else {
    uint32_t workers = render_worker_count(&options->render);
    const char *subject = options->render.mode == RENDER_BUILTIN
                              ? algorithm_name(options->render.algorithm)
                              : options->render.mode == RENDER_EXPRESSION_SCALAR
                                    ? "a scalar expression"
                                    : "RGB expressions";
    fprintf(progress->stream, "Rendering %u frame%s of %s at %ux%u using %u thread%s...\n",
            options->frames, options->frames == 1 ? "" : "s", subject,
            options->render.width, options->render.height, workers, workers == 1 ? "" : "s");
  }
  fflush(progress->stream);
}

void progress_nonfinite(progress_t *progress, uint64_t count) {
  if (progress->mode == PROGRESS_NONE || count == 0) return;
  if (progress->mode == PROGRESS_JSON) {
    fprintf(progress->stream,
            "{\"version\":1,\"event\":\"warning\",\"kind\":\"nonfinite\","
            "\"count\":%llu}\n",
            (unsigned long long)count);
  } else {
    fprintf(progress->stream, "Warning: mapped %llu non-finite expression result%s to zero\n",
            (unsigned long long)count, count == 1 ? "" : "s");
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
