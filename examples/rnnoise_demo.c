
#include <stdio.h>
#include "rnnoise.h"
#include <stdlib.h>
#include <stdint.h>

#define DR_MP3_IMPLEMENTATION

#include "dr_mp3.h"

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


void wavWrite_f32(char *filename, float *buffer, int sampleRate, uint32_t totalSampleCount) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = (drwav_uint32) sampleRate;
    format.bitsPerSample = 32;
    for (int32_t i = 0; i < totalSampleCount; ++i) {
        buffer[i] = drwav_clamp(buffer[i], -32768, 32767) * (1.0f / 32768.0f);
    }
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

float *wavRead_f32(const char *filename, uint32_t *fs, uint64_t *sampleCount) {
    unsigned int channels = 1;
    unsigned int sampleRate = 0;
    drwav_uint64 totalSampleCount = 0;
    float *input = drwav_open_file_and_read_pcm_frames_f32(filename, &channels, &sampleRate, &totalSampleCount);
    if (input == NULL) {
        drmp3_config pConfig;
        input = drmp3_open_file_and_read_f32(filename, &pConfig, &totalSampleCount);
        if (input != NULL) {
            channels = pConfig.outputChannels;
            sampleRate = pConfig.outputSampleRate;
        }
    }
    if (input == NULL) {
        fprintf(stderr, "read file [%s] error.\n", filename);
        exit(1);
    }
    *fs = sampleRate;
    *sampleCount = totalSampleCount;

    float norm = (32768.0f) / channels;
    float *in = input;
    float *output = input;
    for (int32_t i = 0; i < totalSampleCount; ++i) {
        float out = 0;
        for (int32_t c = 0; c < channels; ++c) {
            out += in[c];
        }
        float sample = out * norm;
        if (sample >= 32766.5)
            out = (int16_t) 32767;
        else if (sample <= -32767.5)
            out = (int16_t) -32768;
        else {
            short s = (int16_t) (sample + .5f);
            s -= (s < 0);
            out = s;
        }
        output[i] = out;
        in += channels;
    }

    return input;
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

//poly f32 version
void poly_resample_f32(const float *input, float *output, int in_frames, int out_frames, int channels) {
    double scale = (1.0 * in_frames) / out_frames;
    int head = (int) (1.0f / scale);
    float pos = 0;
    for (int i = 0; i < head; i++) {
        for (int c = 0; c < channels; c++) {
            float sample_1 = input[0 + c];
            float sample_2 = input[channels + c];
            float sample_3 = input[(channels << 1) + c];
            float poly_3 = sample_1 + sample_3 - sample_2 * 2;
            float poly_2 = sample_2 * 4 - (sample_1 * 3) - sample_3;
            float poly_1 = sample_1;
            output[i * channels + c] = (poly_3 * pos * pos + poly_2 * pos) * 0.5f + poly_1;
        }
        pos += scale;
    }
    double in_pos = head * scale;
    for (int n = head; n < out_frames; n++) {
        int npos = (int) in_pos;
        pos = in_pos - npos + 1;
        for (int c = 0; c < channels; c++) {
            float sample_1 = input[(npos - 1) * channels + c];
            float sample_2 = input[(npos + 0) * channels + c];
            float sample_3 = input[(npos + 1) * channels + c];
            float poly_3 = sample_1 + sample_3 - sample_2 * 2;
            float poly_2 = sample_2 * 4 - (sample_1 * 3) - sample_3;
            float poly_1 = sample_1;
            output[n * channels + c] = (poly_3 * pos * pos + poly_2 * pos) * 0.5f + poly_1;
        }
        in_pos += scale;
    }
}

void denoise_proc(float *buffer, uint32_t buffen_len) {
#define  FRAME_SIZE   480
    DenoiseState *st;
    st = rnnoise_create();
    float patch_buffer[FRAME_SIZE];
    if (st != NULL) {
        uint32_t frames = buffen_len / FRAME_SIZE;
        uint32_t lastFrame = buffen_len % FRAME_SIZE;
        for (int i = 0; i < frames; ++i) {
            rnnoise_process_frame(st, buffer, buffer);
            buffer += FRAME_SIZE;
        }
        if (lastFrame != 0) {
            memset(patch_buffer, 0, FRAME_SIZE * sizeof(float));
            memcpy(patch_buffer, buffer, lastFrame * sizeof(float));
            rnnoise_process_frame(st, patch_buffer, patch_buffer);
            memcpy(buffer, patch_buffer, lastFrame * sizeof(float));
        }
    }
    rnnoise_destroy(st);
}

void rnnDeNoise(char *in_file, char *out_file) {
    uint32_t in_sampleRate = 0;
    uint64_t in_size = 0;
    float *data_in = wavRead_f32(in_file, &in_sampleRate, &in_size);
    uint32_t out_sampleRate = 48000;
    uint32_t out_size = (uint32_t) (in_size * ((float) out_sampleRate / in_sampleRate));
    float *data_out = (float *) malloc(out_size * sizeof(float));
    int channels = 1;
    if (data_in != NULL && data_out != NULL) {
        poly_resample_f32(data_in, data_out, in_size, out_size, channels);
        double startTime = now();
        denoise_proc(data_out, out_size);
        double time_interval = calcElapsed(startTime, now());
        printf("time interval: %d ms\n ", (int) (time_interval * 1000));
        poly_resample_f32(data_out, data_in, out_size, in_size, channels);
        wavWrite_f32(out_file, data_in, in_sampleRate, (uint32_t) in_size);
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

    if (argc < 2) {
        printf("usage:\n");
        printf("./rnnoise input.wav\n");
        printf("./rnnoise input.mp3\n");
        printf("or\n");
        printf("./rnnoise input.wav output.wav\n");
        printf("./rnnoise input.mp3 output.wav\n");
        return -1;
    }
    char *in_file = argv[1];
    if (argc > 2) {
        char *out_file = argv[2];
        rnnDeNoise(in_file, out_file);
    } else {
        char drive[3];
        char dir[256];
        char fname[256];
        char ext[256];
        char out_file[1024];
        splitpath(in_file, drive, dir, fname, ext);
        sprintf(out_file, "%s%s%s_out.wav", drive, dir, fname);
        rnnDeNoise(in_file, out_file);
    }
    printf("press any key to exit.\n");
    getchar();
    return 0;
}
