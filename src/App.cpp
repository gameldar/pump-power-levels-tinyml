#include "App.h"
#include "model.h"
#include "FeatureProvider.h"
#include "RecognizeLevels.h"
#include "MicroModelSettings.h"
#include "AudioProvider.h"

#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* model_input = nullptr;
    FeatureProvider* feature_provider = nullptr;
    RecognizeLevels* recognizer = nullptr;
    int32_t previous_time = 0;

    // Create an area of memory to use for input, output and intermediate arrays.
    constexpr int kTensorArenaSize = 10 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
    int8_t feature_buffer[kFeatureElementCount];
    int8_t* model_input_buffer = nullptr;
}

const char* getLevelText(PowerLevel level) {
    switch (level) {
        case LOW:
            return "Low";
        case HIGH:
            return "High";
        case NONE:
            return "None";
        default:
            return "Unknown";
    }
}

void RespondToLevel(tflite::ErrorReporter* error_reporter,
                       int32_t current_time, PowerLevel level,
                       uint8_t score, bool is_new_level) {
    if (is_new_level) {
        TF_LITE_REPORT_ERROR(error_reporter, "Heard %s (%d) @%dms", getLevelText(level));
    }
}

void setup_app() {
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        TF_LITE_REPORT_ERROR(error_reporter,
                             "Model provided is schema version %d not equal "
                             "to supported version %d.",
                             model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::MicroMutableOpResolver<4> micro_op_resolver(error_reporter);
    if (micro_op_resolver.AddDepthwiseConv2D() != kTfLiteOk) {
        return;
    }
    if (micro_op_resolver.AddFullyConnected() != kTfLiteOk) {
        return;
    }
    if (micro_op_resolver.AddSoftmax() != kTfLiteOk) {
        return;
    }
    if (micro_op_resolver.AddReshape() != kTfLiteOk) {
        return;
    }

    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
      TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
      return;
    }

    model_input = interpreter->input(0);
    if ((model_input->dims->size != 2)
        || (model_input->dims->data[0] != 1)
        || (model_input->dims->data[1] != (kFeatureSliceCount * kFeatureSliceSize))
        || (model_input->type != kTfLiteInt8)) {
        TF_LITE_REPORT_ERROR(error_reporter,
                             "Bad input tensor parameters in model");
        return;
    }

    TF_LITE_REPORT_ERROR(error_reporter, "model_input->data.data = %d", model_input->data.data);
    model_input_buffer = model_input->data.int8;

    static FeatureProvider static_feature_provider(kFeatureElementCount, feature_buffer);
    feature_provider = &static_feature_provider;

    static RecognizeLevels static_recognizer(error_reporter);
    recognizer = &static_recognizer;

    previous_time = 0;

    TF_LITE_REPORT_ERROR(error_reporter, "Setup Complete");
}

void loop_app() {
    //TF_LITE_REPORT_ERROR(error_reporter, "Starting Loop");
    const int32_t current_time = LatestAudioTimestamp();
    //TF_LITE_REPORT_ERROR(error_reporter, "latest timestamp %d", current_time);
    int how_many_new_slices = 0;
    TfLiteStatus feature_status = feature_provider->PopulateFeatureData(
        error_reporter, previous_time, current_time, &how_many_new_slices
    );
    if (feature_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(error_reporter, "Feature generation failed");
        return;
    }

    //TF_LITE_REPORT_ERROR(error_reporter, "got features");
    previous_time = current_time;

    if (how_many_new_slices == 0) {
        TF_LITE_REPORT_ERROR(error_reporter, "no slices");
        return;
    }

    //TF_LITE_REPORT_ERROR(error_reporter, "copying features");
    for (int i = 0; i < kFeatureElementCount; i++) {
        model_input_buffer[i] = feature_buffer[i];
    }

    //TF_LITE_REPORT_ERROR(error_reporter, "invoking");
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed");
        return;
    }

    //TF_LITE_REPORT_ERROR(error_reporter, "copying output");
    TfLiteTensor* output = interpreter->output(0);
    PowerLevel* found_level = new PowerLevel[1];
    uint8_t score = 0;
    bool is_new_level = false;
    //TF_LITE_REPORT_ERROR(error_reporter, "processing results: %d", output);
    TfLiteStatus process_status = recognizer->ProcessLatestResults(
        output, current_time, found_level, &score, &is_new_level
                                                                   );
    if (process_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(error_reporter,
                             "RecognizeLevels::ProcessLatestResults() failed");
        return;
    }
    //TF_LITE_REPORT_ERROR(error_reporter, "responding");
    RespondToLevel(error_reporter, current_time, *found_level, score, is_new_level);
}
