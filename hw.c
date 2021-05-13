#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

#include <libde265/de265.h>

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define xstr(a) str(a)
#define str(a) #a

void DumpHex(const void *data, size_t size) {
  char ascii[17];
  size_t i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char *)data)[i]);
    if (((unsigned char *)data)[i] >= ' ' &&
        ((unsigned char *)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char *)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i + 1) % 8 == 0 || i + 1 == size) {
      printf(" ");
      if ((i + 1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i + 1 == size) {
        ascii[(i + 1) % 16] = '\0';
        if ((i + 1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i + 1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}

void check_err_de265(de265_error err) {
  if (err == DE265_OK)
    return;

  fprintf(stderr, "ERROR: %s\n", de265_get_error_text(err));
  exit(EXIT_FAILURE);
}

#ifdef HAVE_SDL2
SDL_Window *screen;
SDL_Renderer *renderer;
SDL_Texture *texture;
#endif

int width = 0, height = 0;
struct timeval start, stv, etv;
int alcnt, decnt, allocated, maxalloc;
int frames[10];
uint8_t *frbuf[10];

enum frame_statuses { FRAME_NOT_ALLOCATED = 0, FRAME_EMPTY, FRAME_BUSY };

const char *find_nal(const char *ptr, const char *end) {
  const char *res = NULL;

  while (ptr < end) {
    if (ptr + 4 >= end)
      break;
    if (ptr[0] == 0) {
      if (ptr[1] == 0) {
        if (ptr[2] == 0) {
          if (ptr[3] == 1) {
            return ptr + 4;
          } // else
          // ptr += 3;
        } // else
          // ptr += 2;
      }   // else
          // ptr += 1;
    }

    ptr++;
  }
  return NULL;
}

int custom_libde265_dec_get_buffer(de265_decoder_context *ctx,
                                   struct de265_image_spec *spec,
                                   struct de265_image *img, void *userdata) {
  int frame_number;

  // printf("ALLOC called\n");
  alcnt++;
  allocated++;
  if (allocated > maxalloc)
    maxalloc = allocated;
  frame_number = (uintptr_t)de265_get_image_user_data(img) - 1;

  // should not happen...
  // Frame has no number assigned!
  // unlikely
  //    if (frame_number == -1) {
  //      goto fallback;
  //    }

  int num_frame_buf = -1;
  for (int i = 0; i < sizeof(frames) / sizeof(int); i++) {
    if (frames[i] == FRAME_NOT_ALLOCATED || frames[i] == FRAME_EMPTY) {
      if (frames[i] == FRAME_NOT_ALLOCATED) {
        frbuf[i] = malloc(1920 * 1080 * 2);
        // printf("malloc for %d, %p\n", i, &frames[i]);
      }
      num_frame_buf = i;
      frames[i] = FRAME_BUSY;
      break;
    }
  }
  // assert(num_frame_buf != -1);
  de265_set_image_plane(img, 0, frbuf[num_frame_buf], 1920,
                        (void *)&frames[num_frame_buf]);
  de265_set_image_plane(img, 1, frbuf[num_frame_buf] + 1920 * 1080, 960, NULL);
  de265_set_image_plane(img, 2, frbuf[num_frame_buf] + 1920 * 1080 * 15 / 10,
                        960, NULL);

  return 1;

fallback:
  return de265_get_default_image_allocation_functions()->get_buffer(
      ctx, spec, img, userdata);
}

void custom_libde265_dec_release_buffer(de265_decoder_context *ctx,
                                        struct de265_image *img,
                                        void *userdata) {
  decnt++;
  allocated--;
  int *ref = de265_get_image_plane_user_data(img, 0);
  // printf("DEALLOC called for %p\n", ref);
  if (ref == NULL) {
    de265_get_default_image_allocation_functions()->release_buffer(ctx, img,
                                                                   userdata);
    return;
  } else {
    *ref = FRAME_EMPTY;
    // printf("Got %p data for image: \n", ref);
    // exit(0);
  }
}

const uint_least32_t crc32table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

uint_least32_t CRC32(const unsigned char *buf, size_t len) {
  uint_least32_t crc = 0xFFFFFFFF;
  while (len--)
    crc = (crc >> 8) ^ crc32table[(crc ^ *buf++) & 0xFF];
  return crc ^ 0xFFFFFFFF;
}

uint_least32_t global_CRC;

struct global_args_t {
  int verbosity;             /* -v param */
  const char *dump_filename; /* -w param */
  char **input_files;        /* input files array */
  int n_ifiles;              /* number of input files */
  bool calc_crc;             /* calculate CRC */
  int crc_from, crc_to; /* from and to (including) frames to calculate CRC */
} global_args;

static const struct option long_opts[] = {{"help", no_argument, NULL, 'h'},
                                          {"verbose", no_argument, NULL, 'v'},
                                          {"crc", required_argument, NULL, 0},
                                          {NULL, no_argument, NULL, 0}};

static const char *opt_string = "hvw";

void display_usage(char *prg) {
  printf("Usage: %s [-options] [input_file]...\n\n"
         "\t-h\t\tshow this help\n"
         "\t-v loglevel\tset logging level\n"
         "\t-w filename\twrite dump to file\n"
         "\nAdvanced tests support:\n"
         "\t--crc N:M\tcalculate CRC32 for each frame from N to M\n",
         prg);
  exit(2);
}

void parse_opts(int argc, char **argv) {
  int opt, long_ndx;

  while ((opt = getopt_long(argc, argv, opt_string, long_opts, &long_ndx)) !=
         -1) {
    switch (opt) {
    case 'h':
      display_usage(argv[0]);
      break;
    case 'v':
      global_args.verbosity++;
      break;
    case 'w':
      global_args.dump_filename = optarg;
      break;
    case 0: /* long options without short versions */
      if (strcmp("crc", long_opts[long_ndx].name) == 0) {
        global_args.calc_crc = true;
        sscanf(optarg, "%d:%d", &global_args.crc_from, &global_args.crc_to);
      }
      break;
    case '?':
      exit(EXIT_FAILURE);
    }
  }
  global_args.input_files = argv + optind;
  global_args.n_ifiles = argc - optind;
  if (!global_args.n_ifiles) {
    display_usage(argv[0]);
  }
}

int main(int argc, char **argv) {
#ifndef H265_FILENAME
  parse_opts(argc, argv);
#endif

  // threads for decoder
  int number_of_threads = 4;
  de265_error err;

  // First, we need to create a decoder context instance using
  de265_decoder_context *ctx = de265_new_decoder();

  struct de265_image_allocation allocation;
  allocation.get_buffer = custom_libde265_dec_get_buffer;
  allocation.release_buffer = custom_libde265_dec_release_buffer;
  de265_set_image_allocation_functions(ctx, &allocation, NULL);

  /* NOTE: we explicitly disable hash checks for now */
  de265_set_parameter_bool(ctx, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, 0);

  //  de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DUMP_SPS_HEADERS, 1);
  //  de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DUMP_VPS_HEADERS, 1);
  //  de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DUMP_PPS_HEADERS, 1);
  //  de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DUMP_SLICE_HEADERS, 1);

  de265_set_verbosity(global_args.verbosity);

  // de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, 1);
  // de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DISABLE_SAO, 1);

  // The decoder uses background threads to do the actual decoding. Before using
  // the decoder, we have to start these background threads with
#ifdef THREADS_SUPPORT
  if (global_args.verbosity)
    printf("Started with %d decoder threads\n", number_of_threads);
  err = de265_start_worker_threads(ctx, number_of_threads);
  check_err_de265(err);
#endif

#ifdef H265_FILENAME
  int fd = open(xstr(H265_FILENAME), O_RDONLY);
#else
  int fd = open(global_args.input_files[0], O_RDONLY);
#endif
  if (fd < 0) {
    printf("Could not open %s file\n", global_args.input_files[0]);
    exit(EXIT_FAILURE);
  }
  struct stat sb;
  fstat(fd, &sb);
  const char *memblock = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (memblock == MAP_FAILED)
    handle_error("mmap");

#ifdef HAVE_SDL2
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    return 1;
  }
#endif

  int nlen = 0, imgcnt = 0;

  const char *end = memblock + sb.st_size;
  const char *next = find_nal(memblock, end);
  const char *nal;
  int i = 0;
  int iteration = 0;
  while (true) {
    if (!next) {
      iteration++;
      if (iteration == 1) {
        // end of warmup phase
        gettimeofday(&start, NULL);
        etv = start;
        imgcnt = 0;
      } else if (iteration == 11)
        break;

      next = find_nal(memblock, end);
    }

    nal = next;
    next = find_nal(next, end);
    size_t len = next ? next - nal : end - nal;
#if 0
    printf("[%d] len = %ld ", i++, len);
    DumpHex(nal, 16);
#endif

    nlen += len;

    // This pushes len bytes of buf into the decoder.
    // The data will be copied into the decoder context
    //
    // These are not actually used by the decoder, but returned again in the
    // decoded image decoded from this data for your use:
    // pts is not provided
    // user_data is not used
    err = de265_push_NAL(ctx, nal, len, 0, NULL);
    check_err_de265(err);

    int more = 0;
    // decode some more
    while (true) {
      err = de265_decode(ctx, &more);
      if (err != DE265_OK) {
        // Need to push some more data
        if (global_args.verbosity) {
          fprintf(stderr, "de265_decode() ret == %s\n",
                  de265_get_error_text(err));
        }
        break;
      }

      /* Get next decoded picture and remove this picture from the decoder
         output queue. Returns NULL is there is no decoded picture ready. You
         can use the picture only until you call any other de265_* function.
       */
      const struct de265_image *img =
          de265_get_next_picture(ctx); // may return NULL
      if (img) {
        if (!width) {
          width = de265_get_image_width(img, 0);
          height = de265_get_image_height(img, 0);

          int out_stride = 0;
          const uint8_t *y = de265_get_image_plane(img, 0, &out_stride);

          enum de265_chroma fmt = de265_get_chroma_format(img);

          // FILE *fd = fopen("dump.gray", "wb");
          // fwrite(y, 1, height * out_stride, fd);
          // fclose(fd);

#ifdef HAVE_SDL2
          // Make a screen to put our video
          screen = SDL_CreateWindow("Video", SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED, width, height,
                                    SDL_WINDOW_ALLOW_HIGHDPI);
          if (!screen) {
            fprintf(stderr, "SDL: could not create window - exiting\n");
            exit(1);
          }

          renderer = SDL_CreateRenderer(screen, -1, 0);
          if (!renderer) {
            fprintf(stderr, "SDL: could not create renderer - exiting\n");
            exit(1);
          }

          // Allocate a place to put our YUV image on that screen
          texture =
              SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                SDL_TEXTUREACCESS_STREAMING, width, height);
          if (!texture) {
            fprintf(stderr, "SDL: could not create texture - exiting\n");
            exit(1);
          }
#endif

          if (global_args.verbosity) {
            printf("Got image %dx%d, bytes per line=%d\n", width, height,
                   out_stride);
          }
        }

        int y_stride, u_stride, v_stride = 0;
        const uint8_t *y = de265_get_image_plane(img, 0, &y_stride);
        const uint8_t *u = de265_get_image_plane(img, 2, &u_stride);
        const uint8_t *v = de265_get_image_plane(img, 1, &v_stride);
        // printf("y_stride = %d, u_stride = %d, v_stride = %d\n", y_stride,
        // u_stride, v_stride);
        imgcnt++;

        if (global_args.calc_crc) {
          stv = etv;
          gettimeofday(&etv, NULL);
          if (imgcnt >= global_args.crc_from && imgcnt <= global_args.crc_to) {
            uint_least32_t crc = CRC32(y, height * y_stride) ^
                                 CRC32(u, height * u_stride) ^
                                 CRC32(v, height * v_stride);
            global_CRC ^= crc;
            printf("Image#%d: %f, CRC=%x\n", imgcnt,
                   etv.tv_sec - stv.tv_sec +
                       (double)(etv.tv_usec - stv.tv_usec) / 1000000,
                   crc);
          }
        }

#ifdef HAVE_SDL2
        SDL_UpdateYUVTexture(texture, NULL, y, width, v, width / 2, u,
                             width / 2);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
#endif
        if (imgcnt == 10) {
          //  FILE *fd = fopen("dump.yuv", "wb");
          //  fwrite(y, 1, height * y_stride, fd);
          //  fwrite(u, 1, height * u_stride, fd);
          //  fwrite(v, 1, height * v_stride, fd);
          //  fclose(fd);
          //  printf("Written:\nY: %d bytes\nU: %d bytes\nV: %d bytes\n",
          //         height * y_stride, height * u_stride, height * v_stride);
          // exit(0);
        }
        // de265_release_next_picture(ctx);
      } else {
        if (global_args.verbosity)
          printf("No picture, got internal frame?\n");
      }

#ifdef HAVE_SDL2
      SDL_Event event;

      SDL_PollEvent(&event);
      switch (event.type) {
      case SDL_QUIT:
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(screen);
        SDL_Quit();
        exit(1);
        break;
      default:
        break;
      }
#endif
    }
  }

  // tell the decoder that no more data will follow this call. It will then
  // flush all its output queues.
  err = de265_flush_data(ctx);
  check_err_de265(err);

  close(fd);

  //  Hence, this function must be called. When the decoder is not needed any
  //  more, the decoder context must be freed again after use by
  err = de265_free_decoder(ctx);
  check_err_de265(err);

  printf("Read overall %d bytes, %d images\n", nlen, imgcnt);
  gettimeofday(&etv, NULL);
  double total = etv.tv_sec - start.tv_sec +
                 (double)(etv.tv_usec - start.tv_usec) / 1000000;
  printf("Overall time: %f secs, FPS: %f\n", total, imgcnt / total);
  if (global_args.verbosity) {
    printf("Allocated #%d bufs, deallocated #%d, rest of #%d, max #%d\n", alcnt,
           decnt, allocated, maxalloc);
  }
  if (global_args.calc_crc) {
    printf("Global CRC code from %d to %d frames: %x\n", global_args.crc_from,
           global_args.crc_to, global_CRC);
  }
}
