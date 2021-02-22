#ifndef __FEATUREPROVIDER_H_
#define __FEATUREPROVIDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
class FeatureProvider
{
public:
    FeatureProvider(int feature_size, int8_t* feature_data);
    ~FeatureProvider();

    TfLiteStatus PopulateFeatureData(tflite::ErrorReporter* error_reporter,
                                     int32_t last_time_in_ms, int32_t time_in_ms,
                                     int* how_many_new_slices);

private:
    int _feature_size;
    int8_t* _feature_data;
    bool _is_first_run;
};


#endif // __FEATUREPROVIDER_H_
