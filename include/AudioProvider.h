#ifndef __AUDIOPROVIDER_H_
#define __AUDIOPROVIDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

TfLiteStatus GetAudioSamples(tflite::ErrorReporter* error_reporter,
                             int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples);

int32_t LatestAudioTimestamp();

#endif // __AUDIOPROVIDER_H_
