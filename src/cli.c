#include "cli.h"

#include "algorithms.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  OPTION_ALGORITHM,
  OPTION_OUTPUT,
  OPTION_WIDTH,
  OPTION_HEIGHT,
  OPTION_SCALE,
  OPTION_COLOR,
  OPTION_RANGE_MODE,
  OPTION_THREADS,
  OPTION_FRAMES,
  OPTION_FPS,
  OPTION_PROGRESS,
  OPTION_FORCE,
  OPTION_HELP,
} option_id_t;

typedef struct {
  const char *name;
  option_id_t id;
  bool requires_value;
} option_definition_t;

static const option_definition_t OPTION_DEFINITIONS[] = {
    {"algorithm", OPTION_ALGORITHM, true}, {"output", OPTION_OUTPUT, true},
    {"width", OPTION_WIDTH, true},           {"height", OPTION_HEIGHT, true},
    {"scale", OPTION_SCALE, true},           {"color", OPTION_COLOR, true},
    {"range-mode", OPTION_RANGE_MODE, true}, {"threads", OPTION_THREADS, true},
    {"frames", OPTION_FRAMES, true},         {"fps", OPTION_FPS, true},
    {"progress", OPTION_PROGRESS, true},     {"force", OPTION_FORCE, false},
    {"help", OPTION_HELP, false},
};

void cli_print_usage(FILE *stream, const char *program) {
  fprintf(stream,
          "Usage: %s --algorithm NAME --output PATH [OPTIONS]\n"
          "\n"
          "Required:\n"
          "  --algorithm NAME    checkerboard, lasagna, or carreaux\n"
          "  --output PATH       .ppm file, .gif file, or frame directory\n"
          "\n"
          "Options:\n"
          "  --width N           Canvas width (default: 1000)\n"
          "  --height N          Canvas height (default: 1000)\n"
          "  --scale N           Algorithm scale (default: 1)\n"
          "  --color RRGGBB      RGB tint (default: ffffff)\n"
          "  --range-mode MODE   wrap or clamp (default: wrap)\n"
          "  --threads N         Worker count from 0 to 1024; 0 is automatic (default: 0)\n"
          "  --frames N          Frame count from 1 to 1000 (default: 1)\n"
          "  --fps N             GIF frame rate from 1 to 1000 (default: 30)\n"
          "  --progress MODE     text, json, or none (default: text)\n"
          "  --force             Replace existing target files\n"
          "  --help              Show this help and exit\n",
          program);
}

static const option_definition_t *find_option(const char *name, size_t length) {
  for (size_t i = 0; i < sizeof(OPTION_DEFINITIONS) / sizeof(OPTION_DEFINITIONS[0]); i++) {
    if (strlen(OPTION_DEFINITIONS[i].name) == length &&
        strncmp(name, OPTION_DEFINITIONS[i].name, length) == 0) {
      return &OPTION_DEFINITIONS[i];
    }
  }
  return NULL;
}

static bool parse_uint(const char *value, uint32_t minimum, uint32_t maximum, uint32_t *result) {
  if (*value == '\0') return false;
  for (const char *cursor = value; *cursor != '\0'; cursor++) {
    if (!isdigit((unsigned char)*cursor)) return false;
  }

  errno = 0;
  char *end;
  unsigned long long parsed = strtoull(value, &end, 10);
  if (errno == ERANGE || *end != '\0' || parsed < minimum || parsed > maximum) return false;
  *result = (uint32_t)parsed;
  return true;
}

static bool parse_scale(const char *value, double *result) {
  if (*value == '\0' || isspace((unsigned char)*value)) return false;
  errno = 0;
  char *end;
  double parsed = strtod(value, &end);
  if (errno == ERANGE || *end != '\0' || !isfinite(parsed) || parsed <= 0.0 || parsed > 1e6) {
    return false;
  }
  *result = parsed;
  return true;
}

static int hex_digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  value = (char)tolower((unsigned char)value);
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

static bool parse_color(const char *value, uint8_t color[3]) {
  if (strlen(value) != 6) return false;
  for (size_t i = 0; i < 3; i++) {
    int high = hex_digit(value[i * 2]);
    int low = hex_digit(value[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    color[i] = (uint8_t)(high * 16 + low);
  }
  return true;
}

static bool parse_progress(const char *value, progress_mode_t *progress) {
  if (strcmp(value, "text") == 0) *progress = PROGRESS_TEXT;
  else if (strcmp(value, "json") == 0) *progress = PROGRESS_JSON;
  else if (strcmp(value, "none") == 0) *progress = PROGRESS_NONE;
  else return false;
  return true;
}

cli_result_t cli_parse(int argc, char *const argv[], mescaline_options_t *options,
                       mescaline_error_t *error) {
  *options = (mescaline_options_t){
      .render = {.width = 1000,
                 .height = 1000,
                 .algorithm = ALGORITHM_CHECKERBOARD,
                 .range_mode = RANGE_WRAP,
                 .threads = 0,
                 .scale = 1.0,
                 .color = {255, 255, 255}},
      .frames = 1,
      .fps = 30,
      .progress = PROGRESS_TEXT,
  };

  for (int i = 1; i < argc; i++) {
    const char *value = NULL;
    if (strncmp(argv[i], "--progress=", 11) == 0) value = argv[i] + 11;
    else if (strcmp(argv[i], "--progress") == 0 && i + 1 < argc) value = argv[i + 1];
    if (value != NULL) parse_progress(value, &options->progress);
  }

  uint32_t seen = 0;
  for (int i = 1; i < argc; i++) {
    const char *argument = argv[i];
    if (strncmp(argument, "--", 2) != 0 || argument[2] == '\0') {
      mescaline_error_set(error, MESCALINE_USAGE, 0, "Unexpected argument '%s'", argument);
      return CLI_ERROR;
    }

    const char *name = argument + 2;
    const char *equals = strchr(name, '=');
    size_t name_length = equals == NULL ? strlen(name) : (size_t)(equals - name);
    const option_definition_t *definition = find_option(name, name_length);
    if (definition == NULL) {
      mescaline_error_set(error, MESCALINE_USAGE, 0, "Unknown option '%.*s'", (int)name_length,
                          name);
      return CLI_ERROR;
    }

    uint32_t bit = 1U << definition->id;
    if ((seen & bit) != 0) {
      mescaline_error_set(error, MESCALINE_USAGE, 0, "Option '--%s' was provided more than once",
                          definition->name);
      return CLI_ERROR;
    }
    seen |= bit;

    const char *value = NULL;
    if (definition->requires_value) {
      if (equals != NULL) value = equals + 1;
      else if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) value = argv[++i];
      if (value == NULL || *value == '\0') {
        mescaline_error_set(error, MESCALINE_USAGE, 0, "Option '--%s' requires a value",
                            definition->name);
        return CLI_ERROR;
      }
    } else if (equals != NULL) {
      mescaline_error_set(error, MESCALINE_USAGE, 0, "Option '--%s' does not take a value",
                          definition->name);
      return CLI_ERROR;
    }

    switch (definition->id) {
      case OPTION_ALGORITHM:
        if (!algorithm_from_name(value, &options->render.algorithm)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Unknown algorithm '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_OUTPUT: options->output = value; break;
      case OPTION_WIDTH:
        if (!parse_uint(value, 1, MESCALINE_MAX_DIMENSION, &options->render.width)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid width '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_HEIGHT:
        if (!parse_uint(value, 1, MESCALINE_MAX_DIMENSION, &options->render.height)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid height '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_SCALE:
        if (!parse_scale(value, &options->render.scale)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid scale '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_COLOR:
        if (!parse_color(value, options->render.color)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0,
                              "Invalid color '%s'; expected exactly six hexadecimal digits", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_RANGE_MODE:
        if (!range_mode_from_name(value, &options->render.range_mode)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0,
                              "Invalid range mode '%s'; expected wrap or clamp", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_THREADS:
        if (!parse_uint(value, 0, MESCALINE_MAX_THREADS, &options->render.threads)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid thread count '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_FRAMES:
        if (!parse_uint(value, 1, MESCALINE_MAX_FRAMES, &options->frames)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid frame count '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_FPS:
        if (!parse_uint(value, 1, MESCALINE_MAX_FPS, &options->fps)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0, "Invalid frame rate '%s'", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_PROGRESS:
        if (!parse_progress(value, &options->progress)) {
          mescaline_error_set(error, MESCALINE_USAGE, 0,
                              "Invalid progress mode '%s'; expected text, json, or none", value);
          return CLI_ERROR;
        }
        break;
      case OPTION_FORCE: options->force = true; break;
      case OPTION_HELP: break;
    }
  }

  if ((seen & (1U << OPTION_HELP)) != 0) return CLI_HELP;
  if ((seen & (1U << OPTION_ALGORITHM)) == 0) {
    mescaline_error_set(error, MESCALINE_USAGE, 0, "--algorithm is required");
    return CLI_ERROR;
  }
  if (options->output == NULL) {
    mescaline_error_set(error, MESCALINE_USAGE, 0, "--output is required");
    return CLI_ERROR;
  }
  if ((uint64_t)options->render.width * options->render.height > MESCALINE_MAX_PIXELS) {
    mescaline_error_set(error, MESCALINE_USAGE, 0,
                        "Canvas exceeds the limit of %llu pixels",
                        (unsigned long long)MESCALINE_MAX_PIXELS);
    return CLI_ERROR;
  }
  return CLI_RUN;
}
