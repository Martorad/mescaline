#include "shader.h"

static void print_usage(FILE *stream, const char *program) {
  fprintf(stream,
          "Usage: %s --output PATH --algo NAME [OPTIONS]\n"
          "\n"
          "Options:\n"
          "  --output PATH       Output path (required)\n"
          "  --algo NAME         checkerboard, lasagna, or carreaux (required)\n"
          "  --horizontal N      Horizontal pixel count (default: 5000)\n"
          "  --vertical N        Vertical pixel count (default: 5000)\n"
          "  --tile N            Algorithm scale (default: 1)\n"
          "  --color RRGGBB      RGB tint (default: ffffff)\n"
          "  --frames N          Frame count from 1 to 1000 (default: 1)\n"
          "  --help              Show this help and exit\n",
          program);
}

int main(int argc, char **argv) {
  int opt;
  uint32_t h = 5000, v = 5000, algo = 0, frames = 1;
  const char *o = NULL, *algo_raw = NULL;
  double tile = 1.0;
  color_t color = {1.0f, 1.0f, 1.0f};

  static struct option long_opts[] = {
      {"horizontal", required_argument, 0, '1'},
      {"vertical", required_argument, 0, '2'},
      {"output", required_argument, 0, '3'},
      {"tile", required_argument, 0, '4'},
      {"color", required_argument, 0, '5'},
      {"algo", required_argument, 0, '6'},
      {"frames", required_argument, 0, '7'},
      {"help", no_argument, 0, '8'},
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
      case '7': frames = atoi(optarg); break;
      case '8': print_usage(stdout, argv[0]); return 0;
      default: print_usage(stderr, argv[0]); return 1;
    }
  }

  if (o == NULL) {
    fprintf(stderr, "Error: --output is required.\n");
    print_usage(stderr, argv[0]);
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

  if (frames == 0 || frames > 1000) {
    fprintf(stderr, "Error: Bad frames argument.\n");
    return 1;
  }

  srand(time(NULL));
  verify_path(o);
  uint64_t start_ts = micros();
  double delta = 0;

  for (uint32_t current_frame = 0; current_frame < frames; current_frame++) {
    char file_path[256] = {'\0'};
    if (frames == 1) {
      memcpy(file_path, o, strlen(o));
    } else {
      size_t backslash_index = find_final_dir(o);
      memcpy(file_path, o, backslash_index);

      char temp_filename[32] = {'\0'};
      sprintf(temp_filename, "%u.ppm", current_frame);
      strcat(file_path, temp_filename);
    }

    FILE *f = fopen(file_path, "wb");

    if (!f) {
      printf("F is null\n");
      return 1;
    }

    printf("Generating %s at %ux%u...\n", file_path, h, v);

    fprintf(f, "P6\n");
    fprintf(f, "%u %u\n", h, v);
    fprintf(f, "255\n");

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
            double out = tan(y_norm) * cos(x_norm + M_PI_2 + delta) / sin(x_norm + M_PI_2);
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

    delta += M_PI / 32;

    fclose(f);
  }

  printf("Done. Took %.3fs\n", (micros() - start_ts) / 1e6);
  start_ts = micros();
  printf("Beginning encode...\n");
  int status = system("ffmpeg -framerate 30 -i outputs/%d.ppm outputs/output.gif");
  printf("Done (%d).", status);

  return 0;
}
