#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef HAS_SDL
#include <SDL.h>
#endif

#include <libde265/de265.h>

void check_err_de265(de265_error err) {
  if (err == DE265_OK)
    return;

  fprintf(stderr, "ERROR: %s\n", de265_get_error_text(err));
  exit(EXIT_FAILURE);
}

#ifdef HAS_SDL
SDL_Window *screen;
SDL_Renderer *renderer;
SDL_Texture *texture;
#endif

char *h265file = "/Users/dmitryilyin/var.h265";
int width = 0, height = 0;
struct timeval start, stv, etv;
int alcnt, decnt, allocated, maxalloc;
int frames[10];
uint8_t *frbuf[10];

enum frame_statuses { FRAME_NOT_ALLOCATED = 0, FRAME_EMPTY, FRAME_BUSY };

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

int main() {
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
  // de265_set_verbosity(3);

  // de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, 1);
  // de265_set_parameter_int(ctx, DE265_DECODER_PARAM_DISABLE_SAO, 1);

  // The decoder uses background threads to do the actual decoding. Before using
  // the decoder, we have to start these background threads with
  err = de265_start_worker_threads(ctx, number_of_threads);
  check_err_de265(err);

  FILE *f = fopen(h265file, "rb");
  if (f == NULL) {
    printf("Could not open %s file\n", h265file);
    exit(EXIT_FAILURE);
  }

#ifdef HAS_SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    return 1;
  }
#endif

  char buf[65000];
  int nlen = 0, imgcnt = 0;
  gettimeofday(&start, NULL);
  etv = start;

  while (!feof(f)) {
    int len = fread(buf, 1, sizeof(buf), f);
    if (!len && ferror(f)) {
      perror("Error while file reading");
      exit(EXIT_FAILURE);
    }
    nlen += len;

    // This pushes len bytes of buf into the decoder.
    // The data will be copied into the decoder context
    //
    // These are not actually used by the decoder, but returned again in the
    // decoded image decoded from this data for your use:
    // pts is not provided
    // user_data is not used
    err = de265_push_data(ctx, buf, len, 0, NULL);
    check_err_de265(err);

    int more = 0;
    // decode some more
    while (true) {
      err = de265_decode(ctx, &more);
      if (err != DE265_OK) {
        perror(de265_get_error_text(err));
        break;
      }
      // printf("Caught\n");

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

#ifdef HAS_SDL
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

          printf("Got image %dx%d, bytes per line=%d\nchroma format = %d\n",
                 width, height, out_stride, fmt);
        }

        int y_stride, u_stride, v_stride = 0;
        const uint8_t *y = de265_get_image_plane(img, 0, &y_stride);
        const uint8_t *u = de265_get_image_plane(img, 2, &u_stride);
        const uint8_t *v = de265_get_image_plane(img, 1, &v_stride);
        // printf("y_stride = %d, u_stride = %d, v_stride = %d\n", y_stride,
        // u_stride, v_stride);
        imgcnt++;
        stv = etv;
        gettimeofday(&etv, NULL);
        printf("Image#%d: %f\t", imgcnt,
               etv.tv_sec - stv.tv_sec +
                   (double)(etv.tv_usec - stv.tv_usec) / 1000000);
        de265_release_next_picture(ctx);

#ifdef HAS_SDL
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
        // if (imgcnt == 2) {
        //  getchar();
        //}
      } else {
        printf("No picture\n");
      }

#ifdef HAS_SDL
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

  fclose(f);

  //  Hence, this function must be called. When the decoder is not needed any
  //  more, the decoder context must be freed again after use by
  err = de265_free_decoder(ctx);
  check_err_de265(err);

  printf("Read overall %d bytes, %d images\n", nlen, imgcnt);
  double total = etv.tv_sec - start.tv_sec +
                 (double)(etv.tv_usec - start.tv_usec) / 1000000;
  printf("Overall time: %f secs, FPS: %f\n", total, imgcnt / total);
  printf("Allocated #%d bufs, deallocated #%d, rest of #%d, max #%d\n", alcnt,
         decnt, allocated, maxalloc);
}
