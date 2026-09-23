#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <signal.h>
#include <string.h>

#include "render.h"

static volatile sig_atomic_t cancel_signal;

int main(void) {
  mescaline_error_t error = {0};
  image_t image = {0};
  assert(!image_create(&image, 100000, 100000, &error));

  assert(image_create(&image, 12, 11, &error));
  render_spec_t spec = {
      .width = 12,
      .height = 11,
      .algorithm = ALGORITHM_CHECKERBOARD,
      .scale = 1.0,
      .color = {0x20, 0x40, 0x60},
  };
  assert(render_frame(&spec, 0, &image, &cancel_signal, &error) == RENDER_OK);
  for (uint32_t y = 0; y < spec.height; y++) {
    for (uint32_t x = 0; x < spec.width; x++) {
      bool on = ((x / 10 + (y / 10) % 2) % 2) == 0;
      const uint8_t *pixel = image.pixels + ((size_t)y * spec.width + x) * 3;
      assert(pixel[0] == (on ? 0x20 : 0));
      assert(pixel[1] == (on ? 0x40 : 0));
      assert(pixel[2] == (on ? 0x60 : 0));
    }
  }

  uint8_t first[12 * 11 * 3];
  for (algorithm_t algorithm = ALGORITHM_CHECKERBOARD; algorithm <= ALGORITHM_CARREAUX;
       algorithm++) {
    spec.algorithm = algorithm;
    assert(render_frame(&spec, 2, &image, &cancel_signal, &error) == RENDER_OK);
    memcpy(first, image.pixels, image.size);
    assert(render_frame(&spec, 2, &image, &cancel_signal, &error) == RENDER_OK);
    assert(memcmp(first, image.pixels, image.size) == 0);
  }

  cancel_signal = SIGTERM;
  assert(render_frame(&spec, 0, &image, &cancel_signal, &error) == RENDER_CANCELLED);
  image_destroy(&image);
  return 0;
}
