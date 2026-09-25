#ifndef MESCALINE_PALETTE_H
#define MESCALINE_PALETTE_H

#include "shader.h"

bool palette_from_name(const char *name, palette_t *palette);
const char *palette_name(palette_t palette);
void palette_apply(palette_t palette, uint8_t value, const uint8_t color[3], uint8_t pixel[3]);

#endif
