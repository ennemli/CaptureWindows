#pragma once
struct AudioData {
    int bits_per_sample;
    int sample_rate;
    size_t number_of_channels;
    size_t number_of_frames;
    const void* audio_data;
};