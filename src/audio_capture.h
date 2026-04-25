#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <stdbool.h>

#include "miniaudio.h"

typedef struct AudioCapture
{
    ma_device device;
    float amplitude;
    float trigger_threshold;
    float release_threshold;
    bool above_threshold;
    bool scream_triggered;
    int print_counter;
    bool ready;
} AudioCapture;

bool audio_capture_init(AudioCapture *capture);
void audio_capture_update(AudioCapture *capture);
bool audio_capture_consume_scream(AudioCapture *capture);
void audio_capture_uninit(AudioCapture *capture);

#endif