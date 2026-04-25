#include "audio_capture.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

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

    if (capture->recording_scream)
    {
        unsigned int room_left = 0;
        if (capture->scream_frame_count < AUDIO_CAPTURE_MAX_SCREAM_FRAMES)
        {
            room_left = AUDIO_CAPTURE_MAX_SCREAM_FRAMES - capture->scream_frame_count;
        }

        unsigned int frames_to_copy = (frame_count < room_left) ? frame_count : room_left;
        unsigned int channels = device->capture.channels;

        for (unsigned int i = 0; i < frames_to_copy; ++i)
        {
            capture->scream_samples[capture->scream_frame_count++] = samples[i * channels];
        }

        if (capture->scream_frame_count >= AUDIO_CAPTURE_MAX_SCREAM_FRAMES)
        {
            capture->recording_scream = false;
            capture->scream_clip_ready = true;
        }
    }
}

bool audio_capture_init(AudioCapture *capture)
{
    if (!capture)
    {
        return false;
    }

    capture->amplitude = 0.0f;
    capture->trigger_threshold = 0.01f;
    capture->release_threshold = 0.02f;
    capture->above_threshold = false;
    capture->scream_triggered = false;
    capture->print_counter = 0;
    capture->scream_frame_count = 0;
    capture->recording_scream = false;
    capture->scream_clip_ready = false;
    capture->ready = false;

    capture->scream_samples = (float*)malloc(sizeof(float) * AUDIO_CAPTURE_MAX_SCREAM_FRAMES);

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
        capture->scream_frame_count = 0;
        capture->recording_scream = true;
        capture->scream_clip_ready = false;
        printf("[mic] scream detected: %.3f\n", capture->amplitude);
        fflush(stdout);
    }
    else if (capture->above_threshold && capture->amplitude <= capture->release_threshold)
    {
        capture->above_threshold = false;
        if (capture->recording_scream && capture->scream_frame_count > 0)
        {
            capture->recording_scream = false;
            capture->scream_clip_ready = true;
        }
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

bool audio_capture_consume_modified_scream(AudioCapture *capture, short *out_samples,
                                           unsigned int max_frames,
                                           unsigned int *out_frames)
{
    if (out_frames)
    {
        *out_frames = 0;
    }

    if (!capture || !capture->ready || !out_samples || max_frames == 0 || !out_frames)
    {
        return false;
    }

    if (!capture->scream_clip_ready || capture->scream_frame_count < 64)
    {
        return false;
    }

    const float speed_up = 1.35f;
    unsigned int produced_frames = 0;

    for (unsigned int i = 0; i < max_frames; ++i)
    {
        float source_pos = (float)i * speed_up;
        unsigned int base_index = (unsigned int)source_pos;
        if (base_index + 1 >= capture->scream_frame_count)
        {
            break;
        }

        float blend = source_pos - (float)base_index;
        float s0 = capture->scream_samples[base_index];
        float s1 = capture->scream_samples[base_index + 1];
        float pitched = s0 + (s1 - s0) * blend;

        float overdriven = pitched * 2.0f;
        float clipped = clampf(overdriven, -1.0f, 1.0f);
        float crushed = roundf(clipped * 28.0f) / 28.0f;

        out_samples[produced_frames++] = (short)(clampf(crushed, -1.0f, 1.0f) * 32767.0f);
    }

    capture->scream_clip_ready = false;
    capture->scream_frame_count = 0;
    *out_frames = produced_frames;
    if (produced_frames > 0)
    {
        printf("[mic] modified scream ready: %u frames\n", produced_frames);
        fflush(stdout);
    }
    return produced_frames > 0;
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
