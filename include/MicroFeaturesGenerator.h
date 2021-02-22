#ifndef __MICROFEATURESGENERATOR_H_
#define __MICROFEATURESGENERATOR_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

TfLiteStatus InitializeMicroFeatures(tflite::ErrorReporter* error_reporter);

TfLiteStatus GenerateMicroFeatures(tflite::ErrorReporter* error_reporter,
                                   const int16_t* input, int input_size,
                                   int output_sze, int8_t* output,
                                   size_t* num_samples_read);


#endif // __MICROFEATURESGENERATOR_H_
