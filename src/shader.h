#ifndef __SHADER_H__
#define __SHADER_H__

#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
  float r, g, b;
} color_t;

static uint64_t micros() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static void pixel(uint8_t r, uint8_t g, uint8_t b, FILE *f) {
  fputc(r, f);
  fputc(g, f);
  fputc(b, f);
}

static bool verify_path(const char *path) {
  char temp[1024] = {'\0'};
  size_t len = strlen(path);
  strcpy(temp, path);

  for (size_t i = 0; i < len; i++) {
    if (temp[i] == '/') {
      temp[i] = '\0';
      struct stat st = {0};
      if (stat(temp, &st) == -1) {
        if (mkdir(temp, 0755) == -1) {
          return false;
        }
        printf("Creating directory %s...\n", temp);
      }
      temp[i] = '/';
    }
  }

  return true;
}

#endif