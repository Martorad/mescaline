#ifndef MESCALINE_ALGORITHMS_H
#define MESCALINE_ALGORITHMS_H

#include "shader.h"

bool algorithm_from_name(const char *name, algorithm_t *algorithm);
const char *algorithm_name(algorithm_t algorithm);
bool range_mode_from_name(const char *name, range_mode_t *range_mode);
const char *range_mode_name(range_mode_t range_mode);
uint8_t range_map_channel(double value, range_mode_t range_mode);
void algorithm_render_row(const render_spec_t *spec, uint32_t frame, uint32_t y, uint8_t *row);

#endif
