#ifndef __RECOGNIZELEVELS_H_
#define __RECOGNIZELEVELS_H_

#include <cstdint>

#include "tensorflow/lite/c/common.h"
#include "MicroModelSettings.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

class PreviousResultsQueue {
 public:
  PreviousResultsQueue(tflite::ErrorReporter* error_reporter)
      : error_reporter_(error_reporter), front_index_(0), size_(0) {}

  // Data structure that holds an inference result, and the time when it
  // was recorded.
  struct Result {
    Result() : time_(0), scores() {}
    Result(int32_t time, int8_t* input_scores) : time_(time) {
      for (int i = 0; i < kCategoryCount; ++i) {
        scores[i] = input_scores[i];
      }
    }
    int32_t time_;
    int8_t scores[kCategoryCount];
  };

  int size() { return size_; }
  bool empty() { return size_ == 0; }
  Result& front() { return results_[front_index_]; }
  Result& back() {
    int back_index = front_index_ + (size_ - 1);
    if (back_index >= kMaxResults) {
      back_index -= kMaxResults;
    }
    return results_[back_index];
  }

  void push_back(const Result& entry) {
    if (size() >= kMaxResults) {
      TF_LITE_REPORT_ERROR(
          error_reporter_,
          "Couldn't push_back latest result, too many already!");
      return;
    }
    size_ += 1;
    back() = entry;
  }

  Result pop_front() {
    if (size() <= 0) {
      TF_LITE_REPORT_ERROR(error_reporter_,
                           "Couldn't pop_front result, none present!");
      return Result();
    }
    Result result = front();
    front_index_ += 1;
    if (front_index_ >= kMaxResults) {
      front_index_ = 0;
    }
    size_ -= 1;
    return result;
  }

  // Most of the functions are duplicates of dequeue containers, but this
  // is a helper that makes it easy to iterate through the contents of the
  // queue.
  Result& from_front(int offset) {
    if ((offset < 0) || (offset >= size_)) {
      TF_LITE_REPORT_ERROR(error_reporter_,
                           "Attempt to read beyond the end of the queue!");
      offset = size_ - 1;
    }
    int index = front_index_ + offset;
    if (index >= kMaxResults) {
      index -= kMaxResults;
    }
    return results_[index];
  }

 private:
  tflite::ErrorReporter* error_reporter_;
  static constexpr int kMaxResults = 50;
  Result results_[kMaxResults];

  int front_index_;
  int size_;
};


class RecognizeLevels {
public:
    explicit RecognizeLevels(tflite::ErrorReporter* error_reporter,
                             int32_t average_window_duration_ms = 1000,
                             uint8_t detection_threshold = 200,
                             int32_t suppression_ms = 1500,
                             int32_t minimum_count = 5);

    TfLiteStatus ProcessLatestResults(const TfLiteTensor* latest_results,
                                      const int32_t current_time_ms,
                                      PowerLevel* level,
                                      uint8_t* score,
                                      bool* is_new_level);

private:
    tflite::ErrorReporter* _error_reporter;
    int32_t _average_window_duration_ms;
    uint8_t _detection_threshold;
    int32_t _suppression_ms;
    int32_t _minimum_count;

    PreviousResultsQueue _previous_results;
    PowerLevel _previous_top_label;
    int32_t _previous_top_label_time;
};


#endif // __RECOGNIZELEVELS_H_
