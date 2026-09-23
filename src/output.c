#include "output.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct output_plan {
  output_kind_t kind;
  char *path;
  uint32_t frames;
  bool force;
  char *staging_directory;
  char *frame_pattern;
  char *temporary_gif;
  char *encoder_log;
  dev_t staging_device;
  ino_t staging_inode;
  bool preserve_staging;
};

typedef struct {
  bool has_backup;
  dev_t published_device;
  ino_t published_inode;
} sequence_publication_t;

static char *path_format(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int length = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (length < 0) {
    va_end(args);
    return NULL;
  }

  char *result = malloc((size_t)length + 1);
  if (result != NULL) vsnprintf(result, (size_t)length + 1, format, args);
  va_end(args);
  return result;
}

static int path_exists(const char *path, struct stat *status) {
  if (lstat(path, status) == 0) return 1;
  return errno == ENOENT ? 0 : -1;
}

static char *absolute_path(const char *path, mescaline_error_t *error) {
  if (path[0] == '/') return strdup(path);

  size_t size = 256;
  char *directory = NULL;
  while (true) {
    char *larger = realloc(directory, size);
    if (larger == NULL) {
      free(directory);
      mescaline_error_set(error, MESCALINE_OUTPUT, errno,
                          "Could not allocate current directory path");
      return NULL;
    }
    directory = larger;
    if (getcwd(directory, size) != NULL) break;
    if (errno != ERANGE) {
      int saved_errno = errno;
      free(directory);
      mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                          "Could not determine the current directory");
      return NULL;
    }
    size *= 2;
  }

  char *result = path_format("%s/%s", directory, path);
  free(directory);
  if (result == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate absolute output path");
  }
  return result;
}

static bool ensure_directory(const char *path, mescaline_error_t *error) {
  char *copy = strdup(path);
  if (copy == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate directory path");
    return false;
  }

  size_t length = strlen(copy);
  while (length > 1 && copy[length - 1] == '/') copy[--length] = '\0';
  for (size_t i = 1; i <= length; i++) {
    if (copy[i] != '/' && copy[i] != '\0') continue;
    char saved = copy[i];
    copy[i] = '\0';
    if (copy[0] != '\0' && mkdir(copy, 0755) != 0 && errno != EEXIST) {
      int saved_errno = errno;
      mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                          "Could not create directory '%s'", copy);
      free(copy);
      return false;
    }

    struct stat status;
    if (copy[0] != '\0' && stat(copy, &status) != 0) {
      int saved_errno = errno;
      mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                          "Could not inspect directory '%s'", copy);
      free(copy);
      return false;
    }
    if (copy[0] != '\0' && !S_ISDIR(status.st_mode)) {
      mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Path component '%s' is not a directory",
                          copy);
      free(copy);
      return false;
    }
    copy[i] = saved;
  }

  free(copy);
  return true;
}

static bool ensure_parent_directory(const char *path, mescaline_error_t *error) {
  char *copy = strdup(path);
  if (copy == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate output path");
    return false;
  }

  char *slash = strrchr(copy, '/');
  if (slash == NULL) {
    free(copy);
    return true;
  }
  if (slash == copy) slash[1] = '\0';
  else *slash = '\0';

  bool result = ensure_directory(copy, error);
  free(copy);
  return result;
}

static bool preflight_target(const char *path, bool force, mescaline_error_t *error) {
  struct stat status;
  int exists = path_exists(path, &status);
  if (exists < 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not inspect output '%s'", path);
    return false;
  }
  if (exists == 0) return true;
  if (S_ISDIR(status.st_mode)) {
    mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Output '%s' is a directory", path);
    return false;
  }
  if (!force) {
    mescaline_error_set(error, MESCALINE_OUTPUT, 0,
                        "Output '%s' already exists; use --force to replace it", path);
    return false;
  }
  return true;
}

static char *escape_ffmpeg_pattern(const char *directory) {
  size_t length = strlen(directory);
  size_t percent_count = 0;
  for (size_t i = 0; i < length; i++) percent_count += directory[i] == '%';

  const char suffix[] = "/frame-%06d.ppm";
  char *pattern = malloc(length + percent_count + sizeof(suffix));
  if (pattern == NULL) return NULL;

  size_t output = 0;
  for (size_t i = 0; i < length; i++) {
    pattern[output++] = directory[i];
    if (directory[i] == '%') pattern[output++] = '%';
  }
  memcpy(pattern + output, suffix, sizeof(suffix));
  return pattern;
}

static char *frame_path(const output_plan_t *plan, uint32_t frame) {
  const char *directory = plan->staging_directory == NULL ? plan->path : plan->staging_directory;
  return path_format("%s/frame-%06u.ppm", directory, frame);
}

static bool create_staging_directory(output_plan_t *plan, const char *format,
                                     mescaline_error_t *error) {
  plan->staging_directory = path_format(format, plan->path);
  if (plan->staging_directory == NULL || mkdtemp(plan->staging_directory) == NULL) {
    int saved_errno = errno;
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno, "Could not create staging directory");
    return false;
  }

  struct stat status;
  if (lstat(plan->staging_directory, &status) != 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not inspect staging directory");
    return false;
  }
  plan->staging_device = status.st_dev;
  plan->staging_inode = status.st_ino;
  return true;
}

static bool prepare_gif(output_plan_t *plan, mescaline_error_t *error) {
  if (!create_staging_directory(plan, "%s.frames.XXXXXX", error)) return false;

  plan->frame_pattern = escape_ffmpeg_pattern(plan->staging_directory);
  plan->encoder_log = path_format("%s/ffmpeg.log", plan->staging_directory);
  plan->temporary_gif = path_format("%s.tmp.XXXXXX", plan->path);
  if (plan->frame_pattern == NULL || plan->encoder_log == NULL || plan->temporary_gif == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate GIF paths");
    return false;
  }

  int descriptor = mkstemp(plan->temporary_gif);
  if (descriptor < 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno,
                        "Could not create temporary GIF beside '%s'", plan->path);
    return false;
  }
  if (close(descriptor) != 0) {
    int saved_errno = errno;
    unlink(plan->temporary_gif);
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno, "Could not close temporary GIF");
    return false;
  }
  return true;
}

bool output_prepare(output_plan_t **result, const mescaline_options_t *options,
                    mescaline_error_t *error) {
  *result = NULL;
  output_plan_t *plan = calloc(1, sizeof(*plan));
  if (plan == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate output plan");
    return false;
  }

  plan->path = absolute_path(options->output, error);
  plan->frames = options->frames;
  plan->force = options->force;
  if (plan->path == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate output path");
    output_destroy(plan);
    return false;
  }

  size_t length = strlen(plan->path);
  bool trailing_slash = length > 0 && plan->path[length - 1] == '/';
  while (length > 1 && plan->path[length - 1] == '/') plan->path[--length] = '\0';

  struct stat status;
  int stat_result = stat(plan->path, &status);
  if (stat_result == 0 && S_ISDIR(status.st_mode)) {
    plan->kind = OUTPUT_SEQUENCE;
  } else if (stat_result != 0 && errno != ENOENT) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not inspect output '%s'",
                        plan->path);
    output_destroy(plan);
    return false;
  } else if (trailing_slash) {
    plan->kind = OUTPUT_SEQUENCE;
  } else {
    const char *base = strrchr(plan->path, '/');
    base = base == NULL ? plan->path : base + 1;
    const char *extension = strrchr(base, '.');
    if (extension == NULL || extension == base) plan->kind = OUTPUT_SEQUENCE;
    else if (strcmp(extension, ".ppm") == 0) plan->kind = OUTPUT_PPM;
    else if (strcmp(extension, ".gif") == 0) plan->kind = OUTPUT_GIF;
    else {
      mescaline_error_set(error, MESCALINE_USAGE, 0,
                          "Unsupported output extension '%s'; use .ppm, .gif, or a directory",
                          extension);
      output_destroy(plan);
      return false;
    }
  }

  if (plan->kind == OUTPUT_PPM && plan->frames != 1) {
    mescaline_error_set(error, MESCALINE_USAGE, 0,
                        "A .ppm output requires exactly one frame");
    output_destroy(plan);
    return false;
  }

  if (plan->kind == OUTPUT_SEQUENCE) {
    if (!ensure_directory(plan->path, error)) {
      output_destroy(plan);
      return false;
    }
    if (!plan->force) {
      for (uint32_t frame = 0; frame < plan->frames; frame++) {
        char *path = frame_path(plan, frame);
        if (path == NULL) {
          mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate frame path");
          output_destroy(plan);
          return false;
        }
        bool ready = preflight_target(path, false, error);
        free(path);
        if (!ready) {
          output_destroy(plan);
          return false;
        }
      }
    }
    if (!create_staging_directory(plan, "%s/.mescaline-frames-XXXXXX", error)) {
      output_destroy(plan);
      return false;
    }
  } else {
    if (!ensure_parent_directory(plan->path, error) ||
        !preflight_target(plan->path, plan->force, error)) {
      output_destroy(plan);
      return false;
    }
  }

  if (plan->kind == OUTPUT_GIF && !prepare_gif(plan, error)) {
    mescaline_error_t ignored;
    output_cleanup(plan, false, &ignored);
    output_destroy(plan);
    return false;
  }

  *result = plan;
  return true;
}

static output_result_t write_all(int descriptor, const uint8_t *data, size_t length,
                                 const volatile sig_atomic_t *cancel_signal,
                                 mescaline_error_t *error) {
  while (length > 0) {
    if (*cancel_signal != 0) return OUTPUT_RESULT_CANCELLED;
    ssize_t written = write(descriptor, data, length);
    if (written < 0) {
      if (errno == EINTR) continue;
      mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not write image data");
      return OUTPUT_RESULT_ERROR;
    }
    data += written;
    length -= (size_t)written;
  }
  return OUTPUT_RESULT_OK;
}

static bool publish_file(const char *temporary, const char *target, bool force,
                         mescaline_error_t *error) {
  if (force) {
    if (rename(temporary, target) == 0) return true;
  } else if (link(temporary, target) == 0) {
    unlink(temporary);
    return true;
  }

  int saved_errno = errno;
  if (!force && saved_errno == EEXIST) {
    mescaline_error_set(error, MESCALINE_OUTPUT, 0,
                        "Output '%s' was created while rendering; use --force to replace it",
                        target);
  } else {
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                        "Could not publish output '%s'", target);
  }
  return false;
}

static output_result_t write_ppm(const char *path, const image_t *image, bool force,
                                 const volatile sig_atomic_t *cancel_signal,
                                 mescaline_error_t *error) {
  char *temporary = path_format("%s.tmp.XXXXXX", path);
  if (temporary == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate temporary path");
    return OUTPUT_RESULT_ERROR;
  }

  int descriptor = mkstemp(temporary);
  if (descriptor < 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno,
                        "Could not create temporary file beside '%s'", path);
    free(temporary);
    return OUTPUT_RESULT_ERROR;
  }

  char header[64];
  int header_length = snprintf(header, sizeof(header), "P6\n%u %u\n255\n", image->width,
                               image->height);
  output_result_t result = write_all(descriptor, (const uint8_t *)header, (size_t)header_length,
                                     cancel_signal, error);
  if (result == OUTPUT_RESULT_OK) {
    result = write_all(descriptor, image->pixels, image->size, cancel_signal, error);
  }
  if (result == OUTPUT_RESULT_OK && fsync(descriptor) != 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not synchronize '%s'", path);
    result = OUTPUT_RESULT_ERROR;
  }
  if (close(descriptor) != 0 && result == OUTPUT_RESULT_OK) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not close '%s'", path);
    result = OUTPUT_RESULT_ERROR;
  }
  if (result == OUTPUT_RESULT_OK && *cancel_signal != 0) result = OUTPUT_RESULT_CANCELLED;
  if (result == OUTPUT_RESULT_OK && !publish_file(temporary, path, force, error)) {
    result = OUTPUT_RESULT_ERROR;
  }
  if (result != OUTPUT_RESULT_OK) unlink(temporary);
  free(temporary);
  return result;
}

output_result_t output_write_frame(output_plan_t *plan, uint32_t frame, const image_t *image,
                                   const volatile sig_atomic_t *cancel_signal,
                                   mescaline_error_t *error) {
  if (plan->kind == OUTPUT_PPM) {
    return write_ppm(plan->path, image, plan->force, cancel_signal, error);
  }

  char *path = frame_path(plan, frame);
  if (path == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate frame path");
    return OUTPUT_RESULT_ERROR;
  }
  output_result_t result = write_ppm(path, image, plan->kind == OUTPUT_GIF || plan->force,
                                     cancel_signal, error);
  free(path);
  return result;
}

static bool rollback_sequence(const output_plan_t *plan, const sequence_publication_t *states,
                              uint32_t published) {
  bool success = true;
  while (published > 0) {
    uint32_t frame = --published;
    char *target = path_format("%s/frame-%06u.ppm", plan->path, frame);
    char *source = frame_path(plan, frame);
    char *backup = path_format("%s/backup-%06u", plan->staging_directory, frame);
    if (target == NULL || source == NULL || backup == NULL) {
      success = false;
    } else if (plan->force) {
      struct stat status;
      int exists = path_exists(target, &status);
      if (exists == 1 && status.st_dev == states[frame].published_device &&
          status.st_ino == states[frame].published_inode) {
        if (unlink(target) != 0) success = false;
        exists = 0;
      } else if (exists != 0) {
        success = false;
      }
      if (states[frame].has_backup && exists == 0 && rename(backup, target) != 0) success = false;
    } else {
      struct stat status;
      int exists = path_exists(target, &status);
      if (exists == 1 && status.st_dev == states[frame].published_device &&
          status.st_ino == states[frame].published_inode) {
        if (unlink(target) != 0) success = false;
      } else if (exists != 0) {
        success = false;
      }
    }
    free(target);
    free(source);
    free(backup);
  }
  return success;
}

static void preserve_failed_rollback(output_plan_t *plan, mescaline_error_t *error) {
  char original[sizeof(error->message)];
  memcpy(original, error->message, sizeof(original));
  original[sizeof(original) - 1] = '\0';
  plan->preserve_staging = true;
  mescaline_error_set(error, MESCALINE_OUTPUT, error->system_error,
                      "%s; rollback was incomplete, recovery files retained in '%s'", original,
                      plan->staging_directory);
}

output_result_t output_publish_sequence(output_plan_t *plan,
                                        const volatile sig_atomic_t *cancel_signal,
                                        mescaline_error_t *error) {
  sequence_publication_t *states = calloc(plan->frames, sizeof(*states));
  if (states == NULL) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate sequence state");
    return OUTPUT_RESULT_ERROR;
  }

  uint32_t published = 0;
  for (uint32_t frame = 0; frame < plan->frames; frame++) {
    if (*cancel_signal != 0) {
      mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Sequence publication was cancelled");
      bool rolled_back = rollback_sequence(plan, states, published);
      if (!rolled_back) preserve_failed_rollback(plan, error);
      free(states);
      return rolled_back ? OUTPUT_RESULT_CANCELLED : OUTPUT_RESULT_ERROR;
    }

    char *source = frame_path(plan, frame);
    char *target = path_format("%s/frame-%06u.ppm", plan->path, frame);
    char *backup = path_format("%s/backup-%06u", plan->staging_directory, frame);
    if (source == NULL || target == NULL || backup == NULL) {
      mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not allocate sequence path");
      free(source);
      free(target);
      free(backup);
      if (!rollback_sequence(plan, states, published)) preserve_failed_rollback(plan, error);
      free(states);
      return OUTPUT_RESULT_ERROR;
    }

    struct stat source_status;
    if (lstat(source, &source_status) != 0) {
      mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not inspect staged frame '%s'",
                          source);
      free(source);
      free(target);
      free(backup);
      if (!rollback_sequence(plan, states, published)) preserve_failed_rollback(plan, error);
      free(states);
      return OUTPUT_RESULT_ERROR;
    }
    states[frame].published_device = source_status.st_dev;
    states[frame].published_inode = source_status.st_ino;

    bool success = false;
    if (plan->force) {
      struct stat status;
      int exists = path_exists(target, &status);
      if (exists == 1 && S_ISDIR(status.st_mode)) {
        mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Output '%s' is a directory", target);
      } else if (exists < 0) {
        mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not inspect '%s'", target);
      } else {
        states[frame].has_backup = exists == 1;
        if (states[frame].has_backup && rename(target, backup) != 0) {
          mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not back up '%s'", target);
        } else if (rename(source, target) == 0) {
          success = true;
        } else {
          int saved_errno = errno;
          mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno, "Could not publish '%s'", target);
          if (states[frame].has_backup && rename(backup, target) != 0) {
            preserve_failed_rollback(plan, error);
          }
        }
      }
    } else if (link(source, target) == 0) {
      success = true;
    } else if (errno == EEXIST) {
      mescaline_error_set(error, MESCALINE_OUTPUT, 0,
                          "Output '%s' was created while rendering", target);
    } else {
      mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not publish '%s'", target);
    }

    free(source);
    free(target);
    free(backup);
    if (!success) {
      if (!rollback_sequence(plan, states, published)) preserve_failed_rollback(plan, error);
      free(states);
      return OUTPUT_RESULT_ERROR;
    }
    published++;
  }

  if (*cancel_signal != 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, 0, "Sequence publication was cancelled");
    bool rolled_back = rollback_sequence(plan, states, published);
    if (!rolled_back) preserve_failed_rollback(plan, error);
    free(states);
    return rolled_back ? OUTPUT_RESULT_CANCELLED : OUTPUT_RESULT_ERROR;
  }

  if (plan->force) {
    for (uint32_t frame = 0; frame < plan->frames; frame++) {
      if (!states[frame].has_backup) continue;
      char *backup = path_format("%s/backup-%06u", plan->staging_directory, frame);
      if (backup != NULL) unlink(backup);
      free(backup);
    }
  }
  free(states);
  return OUTPUT_RESULT_OK;
}

bool output_publish_gif(output_plan_t *plan, mescaline_error_t *error) {
  int descriptor = open(plan->temporary_gif, O_RDONLY);
  if (descriptor < 0) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not open encoded GIF");
    return false;
  }

  struct stat status;
  bool valid = fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) && status.st_size > 0;
  if (!valid) {
    int saved_errno = errno;
    close(descriptor);
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                        "FFmpeg did not produce a non-empty GIF");
    return false;
  }
  int sync_result = fsync(descriptor);
  int sync_errno = errno;
  int close_result = close(descriptor);
  if (sync_result != 0 || close_result != 0) {
    int saved_errno = sync_result != 0 ? sync_errno : errno;
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                        "Could not synchronize encoded GIF");
    return false;
  }
  return publish_file(plan->temporary_gif, plan->path, plan->force, error);
}

static bool remove_staging_directory(const output_plan_t *plan, mescaline_error_t *error) {
  const char *path = plan->staging_directory;
  int descriptor = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
  if (descriptor < 0) {
    if (errno == ENOENT) return true;
    mescaline_error_set(error, MESCALINE_OUTPUT, errno,
                        "Could not open staging directory '%s' for cleanup", path);
    return false;
  }

  struct stat status;
  int stat_result = fstat(descriptor, &status);
  if (stat_result != 0 || status.st_dev != plan->staging_device ||
      status.st_ino != plan->staging_inode) {
    int saved_errno = stat_result != 0 ? errno : 0;
    close(descriptor);
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                        "Staging directory '%s' changed before cleanup", path);
    return false;
  }

  DIR *directory = fdopendir(descriptor);
  if (directory == NULL) {
    int saved_errno = errno;
    close(descriptor);
    mescaline_error_set(error, MESCALINE_OUTPUT, saved_errno,
                        "Could not read staging directory '%s' for cleanup", path);
    return false;
  }

  bool success = true;
  struct dirent *entry;
  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    if (unlinkat(descriptor, entry->d_name, 0) != 0) success = false;
  }
  if (closedir(directory) != 0 || rmdir(path) != 0) success = false;
  if (!success) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno,
                        "Could not completely remove staging directory '%s'", path);
  }
  return success;
}

bool output_cleanup(output_plan_t *plan, bool retain_staging, mescaline_error_t *error) {
  retain_staging = retain_staging || plan->preserve_staging;
  bool success = true;
  if (plan->temporary_gif != NULL && unlink(plan->temporary_gif) != 0 && errno != ENOENT) {
    mescaline_error_set(error, MESCALINE_OUTPUT, errno, "Could not remove temporary GIF '%s'",
                        plan->temporary_gif);
    success = false;
  }
  if (!retain_staging && plan->staging_directory != NULL &&
      !remove_staging_directory(plan, error)) {
    success = false;
  }
  return success;
}

output_kind_t output_kind(const output_plan_t *plan) { return plan->kind; }
const char *output_path(const output_plan_t *plan) { return plan->path; }
const char *output_frame_pattern(const output_plan_t *plan) { return plan->frame_pattern; }
const char *output_temporary_gif(const output_plan_t *plan) { return plan->temporary_gif; }
const char *output_encoder_log(const output_plan_t *plan) { return plan->encoder_log; }
const char *output_staging_directory(const output_plan_t *plan) {
  return plan->staging_directory;
}

void output_destroy(output_plan_t *plan) {
  if (plan == NULL) return;
  free(plan->path);
  free(plan->staging_directory);
  free(plan->frame_pattern);
  free(plan->temporary_gif);
  free(plan->encoder_log);
  free(plan);
}
