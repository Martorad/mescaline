#ifndef MESCALINE_ALGORITHMS_H
#define MESCALINE_ALGORITHMS_H

#include "shader.h"

bool algorithm_from_name(const char *name, algorithm_t *algorithm);
const char *algorithm_name(algorithm_t algorithm);
void algorithm_render_row(const render_spec_t *spec, uint32_t frame, uint32_t y, uint8_t *row);

#endif
