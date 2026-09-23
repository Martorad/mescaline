#include "encode.h"

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static bool wait_after_cancel(pid_t process) {
  kill(process, SIGTERM);
  struct timespec delay = {.tv_nsec = 50000000};
  for (int attempt = 0; attempt < 40; attempt++) {
    pid_t result = waitpid(process, NULL, WNOHANG);
    if (result == process || (result < 0 && errno == ECHILD)) return true;
    nanosleep(&delay, NULL);
  }
  kill(process, SIGKILL);
  while (true) {
    pid_t result = waitpid(process, NULL, 0);
    if (result == process || (result < 0 && errno == ECHILD)) return true;
    if (result < 0 && errno != EINTR) return false;
  }
}

static void terminate_and_reap(pid_t process) {
  kill(process, SIGKILL);
  while (waitpid(process, NULL, 0) < 0 && errno == EINTR) {}
}

encode_result_t ffmpeg_encode(const char *frame_pattern, const char *temporary_gif,
                              const char *log_path, uint32_t frames, uint32_t fps,
                              const volatile sig_atomic_t *cancel_signal,
                              mescaline_error_t *error) {
  if (*cancel_signal != 0) return ENCODE_CANCELLED;

  int log = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (log < 0) {
    mescaline_error_set(error, MESCALINE_ENCODER, errno, "Could not create FFmpeg log");
    return ENCODE_FAILED;
  }
  if (log <= STDERR_FILENO) {
    int duplicate = fcntl(log, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    close(log);
    log = duplicate;
    if (log < 0) {
      mescaline_error_set(error, MESCALINE_ENCODER, errno, "Could not prepare FFmpeg log");
      return ENCODE_FAILED;
    }
  }

  char frame_count[16];
  char frame_rate[16];
  snprintf(frame_count, sizeof(frame_count), "%u", frames);
  snprintf(frame_rate, sizeof(frame_rate), "%u", fps);
  char *arguments[] = {"ffmpeg",       "-nostdin",    "-hide_banner", "-loglevel",
                       "error",        "-y",          "-framerate",   frame_rate,
                       "-start_number", "0",           "-i",           (char *)frame_pattern,
                       "-frames:v",    frame_count,    "-f",           "gif",
                       (char *)temporary_gif, NULL};

  posix_spawn_file_actions_t actions;
  int spawn_error = posix_spawn_file_actions_init(&actions);
  bool actions_initialized = spawn_error == 0;
  pid_t process = 0;
  if (spawn_error == 0) {
    spawn_error = posix_spawn_file_actions_adddup2(&actions, log, STDOUT_FILENO);
  }
  if (spawn_error == 0) {
    spawn_error = posix_spawn_file_actions_adddup2(&actions, log, STDERR_FILENO);
  }
  if (spawn_error == 0) spawn_error = posix_spawn_file_actions_addclose(&actions, log);
  if (spawn_error == 0) {
    spawn_error = posix_spawnp(&process, "ffmpeg", &actions, NULL, arguments, environ);
  }
  if (actions_initialized) posix_spawn_file_actions_destroy(&actions);
  close(log);
  if (spawn_error != 0) {
    mescaline_error_set(error, MESCALINE_ENCODER, spawn_error, "Could not start FFmpeg");
    return ENCODE_FAILED;
  }

  struct timespec delay = {.tv_nsec = 20000000};
  int status;
  while (true) {
    pid_t result = waitpid(process, &status, WNOHANG);
    if (result == process) break;
    if (result < 0 && errno != EINTR) {
      int saved_errno = errno;
      terminate_and_reap(process);
      mescaline_error_set(error, MESCALINE_ENCODER, saved_errno, "Could not wait for FFmpeg");
      return ENCODE_FAILED;
    }
    if (*cancel_signal != 0) {
      if (wait_after_cancel(process)) return ENCODE_CANCELLED;
      mescaline_error_set(error, MESCALINE_ENCODER, errno,
                          "Could not reap FFmpeg after cancellation");
      return ENCODE_FAILED;
    }
    nanosleep(&delay, NULL);
  }

  if (*cancel_signal != 0) return ENCODE_CANCELLED;

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (WIFEXITED(status)) {
      mescaline_error_set(error, MESCALINE_ENCODER, 0, "FFmpeg exited with status %d",
                          WEXITSTATUS(status));
    } else {
      mescaline_error_set(error, MESCALINE_ENCODER, 0, "FFmpeg was terminated by signal %d",
                          WTERMSIG(status));
    }
    return ENCODE_FAILED;
  }
  return ENCODE_OK;
}
