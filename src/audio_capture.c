#include "audio_capture.h"

#include <math.h>
#include <stdio.h>

static void audio_capture_callback(ma_device *device, void *output,
                                   const void *input, ma_uint32 frame_count)
{
    (void)output;

    AudioCapture *capture = (AudioCapture *)device->pUserData;
    if (!capture || !input || frame_count == 0)
    {
        if (capture)
        {
            capture->amplitude = 0.0f;
        }
        return;
    }

    const float *samples = (const float *)input;
    ma_uint32 sample_count = frame_count * device->capture.channels;
    double sum = 0.0;

    for (ma_uint32 i = 0; i < sample_count; ++i)
    {
        double sample = samples[i];
        sum += sample * sample;
    }

    capture->amplitude = (sample_count > 0) ? (float)sqrt(sum / sample_count) : 0.0f;
}

bool audio_capture_init(AudioCapture *capture)
{
    if (!capture)
    {
        return false;
    }

    capture->amplitude = 0.0f;
    capture->trigger_threshold = 0.18f;
    capture->release_threshold = 0.12f;
    capture->above_threshold = false;
    capture->scream_triggered = false;
    capture->print_counter = 0;
    capture->ready = false;

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 48000;
    config.dataCallback = audio_capture_callback;
    config.pUserData = capture;

    ma_result result = ma_device_init(NULL, &config, &capture->device);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "[mic] failed to initialize capture device: %d\n",
                (int)result);
        return false;
    }

    result = ma_device_start(&capture->device);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "[mic] failed to start capture device: %d\n", (int)result);
        ma_device_uninit(&capture->device);
        return false;
    }

    capture->ready = true;
    printf("[mic] capture enabled\n");
    return true;
}

void audio_capture_update(AudioCapture *capture)
{
    if (!capture || !capture->ready)
    {
        return;
    }

    if (!capture->above_threshold && capture->amplitude >= capture->trigger_threshold)
    {
        capture->above_threshold = true;
        capture->scream_triggered = true;
        printf("[mic] scream detected: %.3f\n", capture->amplitude);
        fflush(stdout);
    }
    else if (capture->above_threshold && capture->amplitude <= capture->release_threshold)
    {
        capture->above_threshold = false;
    }
}

bool audio_capture_consume_scream(AudioCapture *capture)
{
    if (!capture || !capture->ready)
    {
        return false;
    }

    bool triggered = capture->scream_triggered;
    capture->scream_triggered = false;
    return triggered;
}

void audio_capture_uninit(AudioCapture *capture)
{
    if (!capture || !capture->ready)
    {
        return;
    }

    ma_device_uninit(&capture->device);
    capture->ready = false;
}