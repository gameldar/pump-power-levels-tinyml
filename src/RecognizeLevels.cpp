#include "RecognizeLevels.h"

#include <limits>

RecognizeLevels::RecognizeLevels(tflite::ErrorReporter* error_reporter,
                                 int32_t average_window_duration_ms,
                                 uint8_t detection_threshold,
                                 int32_t suppression_ms,
                                 int32_t minimum_count)
    : _error_reporter(error_reporter),
      _average_window_duration_ms(average_window_duration_ms),
      _detection_threshold(detection_threshold),
      _suppression_ms(suppression_ms),
      _minimum_count(minimum_count),
      _previous_results(error_reporter) {
    _previous_top_label = PowerLevel::NONE;
    _previous_top_label_time = std::numeric_limits<int32_t>::min();
}

TfLiteStatus RecognizeLevels::ProcessLatestResults(
    const TfLiteTensor* latest_results, const int32_t current_time_ms,
    PowerLevel* level, uint8_t* score, bool* is_new_command) {
    if ((latest_results->dims->size != 2) ||
        (latest_results->dims->data[0] != 1) ||
        (latest_results->dims->data[1] != kCategoryCount)) {
        TF_LITE_REPORT_ERROR(
            _error_reporter,
            "The results for recognition should contain %d elements, but there are "
            "%d in an %d-dimensional shape",
            kCategoryCount, latest_results->dims->data[1],
            latest_results->dims->size);
        return kTfLiteError;
    }

    if (latest_results->type != kTfLiteInt8) {
        TF_LITE_REPORT_ERROR(
            _error_reporter,
            "The results for recognition should be int8_t elements, but are %d",
            latest_results->type);
        return kTfLiteError;
    }

    if ((!_previous_results.empty()) &&
        (current_time_ms < _previous_results.front().time_)) {
        TF_LITE_REPORT_ERROR(
            _error_reporter,
            "Results must be fed in increasing time order, but received a "
            "timestamp of %d that was earlier than the previous one of %d",
            current_time_ms, _previous_results.front().time_
                             );
        return kTfLiteError;
    }

    //TF_LITE_REPORT_ERROR(_error_reporter, "pushback");
    _previous_results.push_back({current_time_ms, latest_results->data.int8});

    // Prune any earlier results that are too old for the averaging window.
    const int64_t time_limit = current_time_ms - _average_window_duration_ms;
    //TF_LITE_REPORT_ERROR(_error_reporter, "pop_front");
    while ((!_previous_results.empty()) &&
           _previous_results.front().time_ < time_limit) {
        _previous_results.pop_front();
    }

    // If there are too few results, assume the result will be unreliable and bail
    const int64_t how_many_results = _previous_results.size();
    const int64_t earliest_time = _previous_results.front().time_;
    const int64_t samples_duration = current_time_ms - earliest_time;
    //TF_LITE_REPORT_ERROR(_error_reporter, "check result count");
    if ((how_many_results < _minimum_count) ||
        (samples_duration < (_average_window_duration_ms/ 4))) {
        //TF_LITE_REPORT_ERROR(_error_reporter, "returning early from result count 1");
        *level = _previous_top_label;
        //TF_LITE_REPORT_ERROR(_error_reporter, "returning early from result count 2");
        *score = 0;
        //TF_LITE_REPORT_ERROR(_error_reporter, "returning early from result count 3");
        *is_new_command = false;
        //TF_LITE_REPORT_ERROR(_error_reporter, "returning early from result count 4");
        return kTfLiteOk;
    }

    // calculate the average score across all the results in the window.
    int32_t average_scores[kCategoryCount];
    //TF_LITE_REPORT_ERROR(_error_reporter, "calculate average scores");
    for (int offset = 0; offset < _previous_results.size(); ++offset) {
        PreviousResultsQueue::Result previous_result =
            _previous_results.from_front(offset);
        const int8_t* scores = previous_result.scores;
        for (int i = 0; i < kCategoryCount; ++i) {
            if (offset == 0) {
                average_scores[i] = scores[i] + 128;
            } else {
                average_scores[i] += scores[i] + 128;
            }
        }
    }
    for (int i = 0; i < kCategoryCount; ++i) {
        average_scores[i] /= how_many_results;
    }

    // Find the current highest scoring category.
    int current_top_index = 0;
    int32_t current_top_score = 0;
    //TF_LITE_REPORT_ERROR(_error_reporter, "find current top");
    for (int i = 0; i < kCategoryCount; ++i) {
        if (average_scores[i] > current_top_score) {
            current_top_score = average_scores[i];
            current_top_index = i;
        }
    }

    //TF_LITE_REPORT_ERROR(_error_reporter, "convert to label: %d", current_top_index);
    PowerLevel current_top_label = kCategoryLabels[current_top_index];

    // If we've recently had another label trigger, assume one that occurs too soon afterwards is a bad result.
    //TF_LITE_REPORT_ERROR(_error_reporter, "check how recently we got a result");
    int64_t time_since_last_top;
    if ((_previous_top_label == kCategoryLabels[0]) ||
        (_previous_top_label_time == std::numeric_limits<int32_t>::min())) {
        time_since_last_top = std::numeric_limits<int32_t>::max();
    } else {
        time_since_last_top = current_time_ms - _previous_top_label_time;
    }

    //TF_LITE_REPORT_ERROR(_error_reporter, "assign is_new_command");
    if ((current_top_score > _detection_threshold) &&
        ((current_top_label != _previous_top_label) ||
         (time_since_last_top > _suppression_ms))) {
        _previous_top_label = current_top_label;
        _previous_top_label_time = current_time_ms;
        *is_new_command = true;
    } else {
        *is_new_command = false;
    }

    //TF_LITE_REPORT_ERROR(_error_reporter, "store current stuff");
    *level = current_top_label;
    *score = current_top_score;

    return kTfLiteOk;
}
