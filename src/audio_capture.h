#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <stdbool.h>

#include "miniaudio.h"

#define AUDIO_CAPTURE_SAMPLE_RATE 48000
#define AUDIO_CAPTURE_MAX_SCREAM_FRAMES (AUDIO_CAPTURE_SAMPLE_RATE * 2)

typedef struct AudioCapture
{
    ma_device device;
    float amplitude;
    float trigger_threshold;
    float release_threshold;
    bool above_threshold;
    bool scream_triggered;
    int print_counter;
    float scream_samples[AUDIO_CAPTURE_MAX_SCREAM_FRAMES];
    unsigned int scream_frame_count;
    bool recording_scream;
    bool scream_clip_ready;
    bool ready;
} AudioCapture;

bool audio_capture_init(AudioCapture *capture);
void audio_capture_update(AudioCapture *capture);
bool audio_capture_consume_scream(AudioCapture *capture);
bool audio_capture_consume_modified_scream(AudioCapture *capture, short *out_samples,
                                           unsigned int max_frames,
                                           unsigned int *out_frames);
void audio_capture_uninit(AudioCapture *capture);

#endif