#include "AudioProvider.h"

#include <cstdlib>
#include <cstring>

#include "freertos/FreeRTOS.h"

#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "RingBuffer.h"
#include "MicroModelSettings.h"


using namespace std;

static const char* TAG = "TF_LITE_AUDIO_PROVIDER";
ringbuf_t* g_audio_capture_buffer;
volatile int32_t g_latest_audio_timestamp = 0;

constexpr int32_t history_samples_to_keep = ((kFeatureSliceDurationMs - kFeatureSliceStrideMs) *
    (kAudioSampleFrequency / 1000));

constexpr int32_t new_samples_to_get = (kFeatureSliceStrideMs * (kAudioSampleFrequency / 1000));

namespace {
    int16_t g_audio_output_buffer[kMaxAudioSampleSize];
    bool g_is_audio_initialized = false;
    int16_t g_history_buffer[history_samples_to_keep];
}

const int32_t kAudioCaptureBufferSize = 80000;
const int32_t i2s_bytes_to_read = 3200;

static void i2s_init(QueueHandle_t* i2sQueue) {
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_LSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
    };
    adc_unit_t adcUnit = ADC_UNIT_1;
    adc1_channel_t adcChannel = ADC1_CHANNEL_7;
    esp_err_t ret = 0;
    ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 4, i2sQueue);
    i2s_set_adc_mode(adcUnit, adcChannel);
    i2s_adc_enable(I2S_NUM_0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in i2s_driver_install");
    }
    ret = i2s_zero_dma_buffer(I2S_NUM_0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in initializing dma buffer with 0");
    }
}

static void CaptureSamples(void* arg) {
    QueueHandle_t i2sQueue;
    size_t bytes_read;
    uint8_t i2s_read_buffer[i2s_bytes_to_read] = {};
    i2s_init(&i2sQueue);
    while (1) {
        i2s_event_t evt;
        if (xQueueReceive(i2sQueue, &evt, portMAX_DELAY) == pdPASS) {
            if (evt.type == I2S_EVENT_RX_DONE) {
                // read 100ms data at once from i2s
                i2s_read(I2S_NUM_0, (void*)i2s_read_buffer, i2s_bytes_to_read, &bytes_read, 10);
                if (bytes_read <= 0) {
                    ESP_LOGE(TAG, "Error in I2S read: %d", bytes_read);
                } else {
                    if (bytes_read < i2s_bytes_to_read) {
                        ESP_LOGW(TAG, "Partial I2S read");
                    }
                    int bytes_written = rb_write(g_audio_capture_buffer,
                                         (uint8_t*) i2s_read_buffer, bytes_read, 10);
                    g_latest_audio_timestamp += ((1000 * (bytes_written / 2)) / kAudioSampleFrequency);
                    if (bytes_written <= 0) {
                        ESP_LOGE(TAG, "Could not write in Ring Buffer: %d ", bytes_written);
                    } else if (bytes_written < bytes_read) {
                        ESP_LOGW(TAG, "Partial Write");
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

TfLiteStatus InitAudioRecording(tflite::ErrorReporter* error_reporter) {
    g_audio_capture_buffer = rb_init("tf_ringbuffer", kAudioCaptureBufferSize);
    if (!g_audio_capture_buffer) {
        ESP_LOGE(TAG, "Error creating ring buffer");
        return kTfLiteError;
    }

    xTaskCreate(CaptureSamples, "CaptureSamples", 1024 * 32, NULL, 10, NULL);
    while (!g_latest_audio_timestamp);
    ESP_LOGI(TAG, "Audio Recording started");
    return kTfLiteOk;
}

TfLiteStatus GetAudioSamples(tflite::ErrorReporter* error_reporter,
                             int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
    if (!g_is_audio_initialized) {
        TfLiteStatus init_status = InitAudioRecording(error_reporter);
        if (init_status != kTfLiteOk) {
            return init_status;
        }
        g_is_audio_initialized = true;
    }

    // copy 160 samples (320 bytes) into output_buff from history
    memcpy((void*)(g_audio_output_buffer), (void*)(g_history_buffer),
           history_samples_to_keep * sizeof(int16_t));

    // copy 320 samples (640 bytes) from rb at (int16_t*(g_audio_output_buffer) + 160),
    // first 160 samples (320 bytes) will be from history
    int32_t bytes_read = rb_read(g_audio_capture_buffer,
                                 ((uint8_t*)(g_audio_output_buffer + history_samples_to_keep)),
                                 new_samples_to_get * sizeof(int16_t), 10);

    if (bytes_read < 0) {
        ESP_LOGE(TAG, " Model could no read data from Ring Buffer");
    } else if (bytes_read < new_samples_to_get * sizeof(int16_t)) {
        ESP_LOGD(TAG, "RB FILLED RIGHT NOW IS %d",
                 rb_filled(g_audio_capture_buffer));
        ESP_LOGD(TAG, " Partial Read of Data by Model ");
        ESP_LOGV(TAG, " Could only read %d bytes when required %d bytes ",
                 bytes_read, new_samples_to_get * sizeof(int16_t));
    }

    // copy 320 bytes from output_buff into history
    memcpy((void*)(g_history_buffer),
           (void*)(g_audio_output_buffer + new_samples_to_get),
           history_samples_to_keep * sizeof(int16_t) );
    *audio_samples_size = kMaxAudioSampleSize;
    *audio_samples = g_audio_output_buffer;
    return kTfLiteOk;
}

int32_t LatestAudioTimestamp() { return g_latest_audio_timestamp; }
