#ifndef __RECORDER_H_
#define __RECORDER_H_

#include <Arduino.h>
#include "driver/i2s.h"

class Recorder
{
private:
    int16_t* audioBuffer1; // buffer for the sample
    int16_t* audioBuffer2; // double buffer so we can keep sampling while processing previous one
    int16_t audioBufferPos = 0;
    int16_t *currentAudioBuffer;
    int16_t *capturedAudioBuffer;
    int32_t bufferSizeInBytes;
    int32_t bufferSizeInSamples;

    // I2S reader task
    TaskHandle_t readerTaskHandle;
    TaskHandle_t writerTaskHandle;

    QueueHandle_t i2sQueue;
    i2s_port_t i2sPort;

    adc_unit_t adcUnit;
    adc1_channel_t adcChannel;

protected:
    void addSample(int16_t sample);
    void configureI2S();
    void processI2SData(uint8_t *i2sData, size_t bytesReader);
    i2s_port_t getI2SPort() {
        return i2sPort;
    }

public:
    int32_t getBufferSizeInBytes() {
        return bufferSizeInBytes;
    }
    int16_t* getCapturedAudioBuffer() {
        return capturedAudioBuffer;
    }

    void start(i2s_port_t i2sPort, i2s_config_t &i2sConfig, int32_t bufferSizeInSamples, TaskHandle_t writerTaskHandle);

    friend void i2sReaderTask(void* param);

    Recorder(adc_unit_t adc_unit, adc1_channel_t adc_channel);
};


#endif // __RECORDER_H_
