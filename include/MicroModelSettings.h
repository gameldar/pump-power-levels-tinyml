#ifndef __MICROMODELSETTINGS_H_
#define __MICROMODELSETTINGS_H_

constexpr int kMaxAudioSampleSize = 512;
constexpr int kAudioSampleFrequency = 16000;

constexpr int kFeatureSliceSize = 40;
constexpr int kFeatureSliceCount = 49;
constexpr int kFeatureElementCount = (kFeatureSliceSize * kFeatureSliceCount);
constexpr int kFeatureSliceStrideMs = 20;
constexpr int kFeatureSliceDurationMs = 30;

enum PowerLevel {
NONE,
LOW,
HIGH,
SILENCE,
UNKNOWN,
};
constexpr int kCategoryCount = 5;
extern PowerLevel kCategoryLabels[kCategoryCount];
extern const char* kCategoryTexts[kCategoryCount];

#endif // __MICROMODELSETTINGS_H_
