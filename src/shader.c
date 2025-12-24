#include "shader.h"

int main(int argc, char **argv) {
  int opt;
  uint32_t h = 5000, v = 5000, algo = 0;
  const char *o = NULL, *algo_raw = NULL;
  double tile = 1.0;
  color_t color = {1.0f, 1.0f, 1.0f};

  static struct option long_opts[] = {
      {"h", required_argument, 0, '1'},
      {"v", required_argument, 0, '2'},
      {"o", required_argument, 0, '3'},
      {"tile", required_argument, 0, '4'},
      {"color", required_argument, 0, '5'},
      {"algo", required_argument, 0, '6'},
  };

  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case '1': h = atoi(optarg); break;
      case '2': v = atoi(optarg); break;
      case '3': o = optarg; break;
      case '4': tile = atof(optarg); break;
      case '5':
        uint32_t color_raw = (uint32_t)strtol(optarg, NULL, 16);
        color.r = ((color_raw & 0xFF0000) >> 16) / 255.0f;
        color.g = ((color_raw & 0xFF00) >> 8) / 255.0f;
        color.b = (color_raw & 0xFF) / 255.0f;
        break;
      case '6': algo_raw = optarg; break;
      default: fprintf(stderr, "Usage: %s --h=N --v=N --o=output/path\n", argv[0]); return 1;
    }
  }

  if (o == NULL) {
    fprintf(stderr, "Error: --o is required.\n");
    fprintf(stderr, "Usage: %s --o=output/path\n", argv[0]);
    return 1;
  }
  if (algo_raw == NULL) {
    fprintf(stderr, "Error: --algo is required.\n");
    fprintf(stderr,
            "Available algorithms:\n"
            " - checkerboard\n"
            " - lasagna\n"
            " - carreaux\n");
    return 1;
  } else {
    if (strcmp("checkerboard", algo_raw) == 0) algo = 0;
    else if (strcmp("lasagna", algo_raw) == 0) algo = 1;
    else if (strcmp("carreaux", algo_raw) == 0) algo = 2;
    else {
      algo = 0xFFFFFFFF;
      // fprintf(stderr, "Unrecognized algorithm.\n");
      // return 1;
    }
  }

  srand(time(NULL));
  FILE *f = fopen(o, "wb");

  if (!f) {
    printf("F is null\n");
    return 1;
  }

  printf("Generating %s at %ux%u...\n", o, h, v);

  fprintf(f, "P6\n");
  fprintf(f, "%u %u\n", h, v);
  fprintf(f, "255\n");

  uint64_t start_ts = micros();

  switch (algo) {
    case 0: {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < v; x++) {
          // Checkerboard
          if (((x / 10) + ((y / 10) % 2)) % 2 == 0) {
            pixel(0xFF * color.r, 0xFF * color.g, 0xFF * color.b, f);
          } else {
            pixel(0x00, 0x00, 0x00, f);
          }
        }
      }
      break;
    }
    case 1: {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < v; x++) {
          // Lasagna
          float x_norm = (float)x / ((float)h / (M_PI * tile)),
                y_norm = (float)y / ((float)v / (M_PI * tile));
          float out = cos(x_norm * 2 + y_norm) + tan(y_norm * 1.45);
          uint8_t final = out * 128 + 128;
          pixel(final * color.r, final * color.g, final * color.b, f);
        }
      }
      break;
    }
    case 2: {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < v; x++) {
          // Carreaux
          double x_norm = (double)x / ((double)h / (M_PI * tile)),
                 y_norm = (double)y / ((double)v / (M_PI * tile));
          double out = tan(y_norm) * cos(x_norm + M_PI_2) / sin(x_norm + M_PI_2);
          uint8_t final = out * 128 + 128;
          pixel(final * color.r, final * color.g, final * color.b, f);
        }
      }
      break;
    }
    case 0xFFFFFFFF: {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < v; x++) {
          // test
          double x_norm = (double)x / ((double)h / (M_PI * tile)),
                 y_norm = (double)y / ((double)v / (M_PI * tile));
          double out =
              sin(x_norm) * sqrt(y_norm) + cos(y_norm + M_PI_2) * tan(x_norm) * log(y_norm);
          uint8_t final = out * 128 + 128;
          pixel(final * color.r, final * color.g, final * color.b, f);
        }
      }
      break;
    }
    default: return 1;
  }

  printf("Done. Took %.3fs\n", (micros() - start_ts) / 1e6);

  fclose(f);

  return 0;
}
