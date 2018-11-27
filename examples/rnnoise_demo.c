
#include <stdio.h>
#include "rnnoise.h"
#include <stdlib.h>
#include <stdint.h>

#define DR_WAV_IMPLEMENTATION

#include "dr_wav.h"

#if   defined(__APPLE__)
# include <mach/mach_time.h>
#elif defined(_WIN32)
# define WIN32_LEAN_AND_MEAN

# include <windows.h>

#else // __linux

# include <time.h>

# ifndef  CLOCK_MONOTONIC //_RAW
#  define CLOCK_MONOTONIC CLOCK_REALTIME
# endif
#endif

static
uint64_t nanotimer() {
    static int ever = 0;
#if defined(__APPLE__)
    static mach_timebase_info_data_t frequency;
    if (!ever) {
        if (mach_timebase_info(&frequency) != KERN_SUCCESS) {
            return 0;
        }
        ever = 1;
    }
    return  (mach_absolute_time() * frequency.numer / frequency.denom);
#elif defined(_WIN32)
    static LARGE_INTEGER frequency;
    if (!ever) {
        QueryPerformanceFrequency(&frequency);
        ever = 1;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (t.QuadPart * (uint64_t) 1e9) / frequency.QuadPart;
#else // __linux
    struct timespec t = {0};
    if (!ever) {
        if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
            return 0;
        }
        ever = 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec * (uint64_t) 1e9) + t.tv_nsec;
#endif
}

static double now() {
    static uint64_t epoch = 0;
    if (!epoch) {
        epoch = nanotimer();
    }
    return (nanotimer() - epoch) / 1e9;
};

static double calcElapsed(double start, double end) {
    double took = -start;
    return took + end;
}


void wavWrite_int16(char *filename, int16_t *buffer, int sampleRate, uint32_t totalSampleCount) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = (drwav_uint32) sampleRate;
    format.bitsPerSample = 16;
    drwav *pWav = drwav_open_file_write(filename, &format);
    if (pWav) {
        drwav_uint64 samplesWritten = drwav_write(pWav, totalSampleCount, buffer);
        drwav_uninit(pWav);
        if (samplesWritten != totalSampleCount) {
            fprintf(stderr, "ERROR\n");
            exit(1);
        }
    }
}

int16_t *wavRead_int16(char *filename, uint32_t *sampleRate, uint64_t *totalSampleCount) {
    unsigned int channels;
    int16_t *buffer = drwav_open_and_read_file_s16(filename, &channels, sampleRate, totalSampleCount);
    if (buffer == NULL) {
        fprintf(stderr, "ERROR\n");
        exit(1);
    }
    if (channels != 1) {
        drwav_free(buffer);
        buffer = NULL;
        *sampleRate = 0;
        *totalSampleCount = 0;
    }
    return buffer;
}

void splitpath(const char *path, char *drv, char *dir, char *name, char *ext) {
    const char *end;
    const char *p;
    const char *s;
    if (path[0] && path[1] == ':') {
        if (drv) {
            *drv++ = *path++;
            *drv++ = *path++;
            *drv = '\0';
        }
    } else if (drv)
        *drv = '\0';
    for (end = path; *end && *end != ':';)
        end++;
    for (p = end; p > path && *--p != '\\' && *p != '/';)
        if (*p == '.') {
            end = p;
            break;
        }
    if (ext)
        for (s = end; (*ext = *s++);)
            ext++;
    for (p = end; p > path;)
        if (*--p == '\\' || *p == '/') {
            p++;
            break;
        }
    if (name) {
        for (s = p; s < end;)
            *name++ = *s++;
        *name = '\0';
    }
    if (dir) {
        for (s = path; s < p;)
            *dir++ = *s++;
        *dir = '\0';
    }
}

//ref :https://github.com/cpuimage/resampler
//poly s16 version
void poly_resample_s16(const int16_t *input, int16_t *output, int in_frames, int out_frames, int channels) {
    double scale = (1.0 * in_frames) / out_frames;
    int head = (int) (1.0 / scale);
    float pos = 0;
    for (int i = 0; i < head; i++) {
        for (int c = 0; c < channels; c++) {
            int sample_1 = input[0 + c];
            int sample_2 = input[channels + c];
            int sample_3 = input[(channels << 1) + c];
            int poly_3 = sample_1 + sample_3 - (sample_2 << 1);
            int poly_2 = (sample_2 << 2) + sample_1 - (sample_1 << 2) - sample_3;
            int poly_1 = sample_1;
            output[i * channels + c] = (int16_t) ((poly_3 * pos * pos + poly_2 * pos) * 0.5f + poly_1);
        }
        pos += scale;
    }
    double in_pos = head * scale;
    for (int n = head; n < out_frames; n++) {
        int npos = (int) in_pos;
        pos = in_pos - npos + 1;
        for (int c = 0; c < channels; c++) {
            int sample_1 = input[(npos - 1) * channels + c];
            int sample_2 = input[(npos + 0) * channels + c];
            int sample_3 = input[(npos + 1) * channels + c];
            int poly_3 = sample_1 + sample_3 - (sample_2 << 1);
            int poly_2 = (sample_2 << 2) + sample_1 - (sample_1 << 2) - sample_3;
            int poly_1 = sample_1;
            output[n * channels + c] = (int16_t) ((poly_3 * pos * pos + poly_2 * pos) * 0.5f + poly_1);
        }
        in_pos += scale;
    }
}

void denoise_proc(int16_t *buffer, uint32_t buffen_len) {
#define  FRAME_SIZE   480
    DenoiseState *st;
    st = rnnoise_create();
    int16_t patch_buffer[FRAME_SIZE];
    if (st != NULL) {
        uint32_t frames = buffen_len / FRAME_SIZE;
        uint32_t lastFrame = buffen_len % FRAME_SIZE;
        for (int i = 0; i < frames; ++i) {
            rnnoise_process_frame(st, buffer, buffer);
            buffer += FRAME_SIZE;
        }
        if (lastFrame != 0) {
            memset(patch_buffer, 0, FRAME_SIZE * sizeof(int16_t));
            memcpy(patch_buffer, buffer, lastFrame * sizeof(int16_t));
            rnnoise_process_frame(st, patch_buffer, patch_buffer);
            memcpy(buffer, patch_buffer, lastFrame * sizeof(int16_t));
        }
    }
    rnnoise_destroy(st);
}

void rnnDeNoise(char *in_file, char *out_file) {
    uint32_t in_sampleRate = 0;
    uint64_t in_size = 0;
    int16_t *data_in = wavRead_int16(in_file, &in_sampleRate, &in_size);
    uint32_t out_sampleRate = 48000;
    uint32_t out_size = (uint32_t) (in_size * ((float) out_sampleRate / in_sampleRate));
    int16_t *data_out = (int16_t *) malloc(out_size * sizeof(int16_t));
    int channels = 1;
    if (data_in != NULL && data_out != NULL) {
        poly_resample_s16(data_in, data_out, in_size, out_size, channels);
        double startTime = now();
        denoise_proc(data_out, out_size);
        double time_interval = calcElapsed(startTime, now());
        printf("time interval: %d ms\n ", (int) (time_interval * 1000));
        poly_resample_s16(data_out, data_in, out_size, in_size, channels);
        wavWrite_int16(out_file, data_in, in_sampleRate, (uint32_t) in_size);
        free(data_in);
        free(data_out);
    } else {
        if (data_in) free(data_in);
        if (data_out) free(data_out);
    }
}


int main(int argc, char **argv) {
    printf("Audio Noise Reduction\n");
    printf("blog:http://cpuimage.cnblogs.com/\n");
    printf("e-mail:gaozhihan@vip.qq.com\n");
    if (argc < 2)
        return -1;

    char *in_file = argv[1];
    char drive[3];
    char dir[256];
    char fname[256];
    char ext[256];
    char out_file[1024];
    splitpath(in_file, drive, dir, fname, ext);
    sprintf(out_file, "%s%s%s_out%s", drive, dir, fname, ext);
    rnnDeNoise(in_file, out_file);
    printf("press any key to exit.\n");
    getchar();
    return 0;
}
