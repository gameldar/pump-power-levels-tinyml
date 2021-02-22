#include "FeatureProvider.h"
#include "MicroFeaturesGenerator.h"
#include "MicroModelSettings.h"
#include "AudioProvider.h"

FeatureProvider::FeatureProvider(int feature_size, int8_t* feature_data)
    : _feature_size(feature_size),
      _feature_data(feature_data),
      _is_first_run(true) {
    for (int n = 0; n < _feature_size; ++n) {
        _feature_data[n] = 0;
    }
}

FeatureProvider::~FeatureProvider() {}

TfLiteStatus FeatureProvider::PopulateFeatureData(
    tflite::ErrorReporter *error_reporter, int32_t last_time_in_ms, int32_t time_in_ms, int *how_many_new_slices) {
    if (_feature_size != kFeatureElementCount) {
        TF_LITE_REPORT_ERROR(error_reporter,
                             "Requested _feature_data size %d doesn't match %d",
                             _feature_size, kFeatureElementCount);
        return kTfLiteError;
    }

    // Quantize the time into steps as long as each window stride, so we can figure out which audio data we need to fetch
    const int last_step = (last_time_in_ms / kFeatureSliceStrideMs);
    const int current_step = (time_in_ms / kFeatureSliceStrideMs);

    int slices_needed = current_step - last_step;
    if (_is_first_run) {
        TfLiteStatus init_status = InitializeMicroFeatures(error_reporter);
        if (init_status != kTfLiteOk) {
            return init_status;
        }
        _is_first_run = false;
        slices_needed = kFeatureSliceCount;
    }
    if (slices_needed > kFeatureSliceCount) {
        slices_needed = kFeatureSliceCount;
    }

    *how_many_new_slices = slices_needed;

    const int slices_to_keep = kFeatureSliceCount - slices_needed;
    const int slices_to_drop = kFeatureSliceCount - slices_to_keep;

    if (slices_to_keep > 0) {
        for (int dest_slice = 0; dest_slice < slices_to_keep; ++dest_slice) {
            int8_t* dest_slice_data =
                _feature_data + (dest_slice * kFeatureSliceSize);
            const int src_slice = dest_slice + slices_to_drop;
            const int8_t* src_slice_data =
                _feature_data + (src_slice * kFeatureSliceSize);
            for (int i = 0; i < kFeatureSliceSize; ++i) {
                dest_slice_data[i] = src_slice_data[i];
            }
        }
    }

    if (slices_needed > 0) {
        for (int new_slice = slices_to_keep; new_slice < kFeatureSliceCount; ++new_slice) {
            const int new_step = (current_step - kFeatureSliceCount + 1) + new_slice;
            const int32_t slice_start_ms = (new_step * kFeatureSliceStrideMs);
            int16_t* audio_samples = nullptr;
            int audio_samples_size = 0;
            GetAudioSamples(error_reporter, (slice_start_ms > 0 ? slice_start_ms : 0),
                            kFeatureSliceDurationMs, &audio_samples_size,
                            &audio_samples);
            if (audio_samples_size < kMaxAudioSampleSize) {
                TF_LITE_REPORT_ERROR(error_reporter,
                                     "Audio data size %d too small, want %d",
                                     audio_samples_size, kMaxAudioSampleSize);
                return kTfLiteError;
            }

            int8_t* new_slice_data = _feature_data + (new_slice * kFeatureSliceSize);
            size_t num_samples_read;
            TfLiteStatus generate_status = GenerateMicroFeatures(
                error_reporter, audio_samples, audio_samples_size, kFeatureSliceSize,
                new_slice_data, &num_samples_read
                                                                 );
            if (generate_status != kTfLiteOk) {
                return generate_status;
            }
        }
    }
    return kTfLiteOk;
}
