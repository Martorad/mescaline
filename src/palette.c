#include "palette.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} palette_color_t;

static const palette_color_t VIRIDIS[] = {
    {68, 1, 84}, {59, 82, 139}, {33, 145, 140}, {94, 201, 98}, {253, 231, 37}};
static const palette_color_t PLASMA[] = {
    {13, 8, 135}, {126, 3, 168}, {204, 71, 120}, {248, 149, 64}, {240, 249, 33}};
static const palette_color_t MAGMA[] = {
    {0, 0, 4}, {81, 18, 124}, {183, 55, 121}, {252, 137, 97}, {252, 253, 191}};
static const palette_color_t INFERNO[] = {
    {0, 0, 4}, {87, 16, 110}, {188, 55, 84}, {249, 142, 9}, {252, 255, 164}};
static const palette_color_t TURBO[] = {
    {48, 18, 59}, {50, 101, 190}, {31, 184, 163}, {164, 252, 60},
    {251, 163, 56}, {180, 4, 38}};

bool palette_from_name(const char *name, palette_t *palette) {
  if (strcmp(name, "grayscale") == 0) *palette = PALETTE_GRAYSCALE;
  else if (strcmp(name, "monochrome") == 0) *palette = PALETTE_MONOCHROME;
  else if (strcmp(name, "viridis") == 0) *palette = PALETTE_VIRIDIS;
  else if (strcmp(name, "plasma") == 0) *palette = PALETTE_PLASMA;
  else if (strcmp(name, "magma") == 0) *palette = PALETTE_MAGMA;
  else if (strcmp(name, "inferno") == 0) *palette = PALETTE_INFERNO;
  else if (strcmp(name, "turbo") == 0) *palette = PALETTE_TURBO;
  else return false;
  return true;
}

const char *palette_name(palette_t palette) {
  switch (palette) {
    case PALETTE_GRAYSCALE: return "grayscale";
    case PALETTE_MONOCHROME: return "monochrome";
    case PALETTE_VIRIDIS: return "viridis";
    case PALETTE_PLASMA: return "plasma";
    case PALETTE_MAGMA: return "magma";
    case PALETTE_INFERNO: return "inferno";
    case PALETTE_TURBO: return "turbo";
  }
  return "unknown";
}

static void interpolate(const palette_color_t *colors, size_t count, uint8_t value,
                        uint8_t pixel[3]) {
  unsigned scaled = value * (unsigned)(count - 1);
  size_t index = scaled / 255;
  if (index == count - 1) {
    pixel[0] = colors[index].red;
    pixel[1] = colors[index].green;
    pixel[2] = colors[index].blue;
    return;
  }

  unsigned remainder = scaled % 255;
  unsigned inverse = 255 - remainder;
  pixel[0] = (uint8_t)((colors[index].red * inverse + colors[index + 1].red * remainder) / 255);
  pixel[1] =
      (uint8_t)((colors[index].green * inverse + colors[index + 1].green * remainder) / 255);
  pixel[2] =
      (uint8_t)((colors[index].blue * inverse + colors[index + 1].blue * remainder) / 255);
}

void palette_apply(palette_t palette, uint8_t value, const uint8_t color[3], uint8_t pixel[3]) {
  switch (palette) {
    case PALETTE_GRAYSCALE: pixel[0] = pixel[1] = pixel[2] = value; break;
    case PALETTE_MONOCHROME:
      for (size_t i = 0; i < 3; i++) pixel[i] = (uint8_t)(value * color[i] / 255U);
      break;
    case PALETTE_VIRIDIS:
      interpolate(VIRIDIS, sizeof(VIRIDIS) / sizeof(*VIRIDIS), value, pixel);
      break;
    case PALETTE_PLASMA: interpolate(PLASMA, sizeof(PLASMA) / sizeof(*PLASMA), value, pixel); break;
    case PALETTE_MAGMA: interpolate(MAGMA, sizeof(MAGMA) / sizeof(*MAGMA), value, pixel); break;
    case PALETTE_INFERNO:
      interpolate(INFERNO, sizeof(INFERNO) / sizeof(*INFERNO), value, pixel);
      break;
    case PALETTE_TURBO: interpolate(TURBO, sizeof(TURBO) / sizeof(*TURBO), value, pixel); break;
  }
}
