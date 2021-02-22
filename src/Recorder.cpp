#include "Recorder.h"
#include <Arduino.h>

void Recorder::addSample(int16_t sample) {
    // add the sample to the current audio buffer
    currentAudioBuffer[audioBufferPos++] = sample;

    if (audioBufferPos == bufferSizeInSamples) {
        // swap to the other buffer
        std::swap(currentAudioBuffer, capturedAudioBuffer);
        // reset buffer position
        audioBufferPos = 0;
        xTaskNotify(writerTaskHandle, 1, eIncrement);
    }
}

void i2sReaderTask(void* param) {
    Recorder* recorder = (Recorder*) param;

    while (true) {
        i2s_event_t evt;
        if (xQueueReceive(recorder->i2sQueue, &evt, portMAX_DELAY) == pdPASS) {
            if (evt.type == I2S_EVENT_RX_DONE) {
                size_t bytesRead = 0;
                do  {
                    // read data from the I2S peripheral
                    uint8_t i2sData[1024];
                    i2s_read(recorder->getI2SPort(), i2sData, 1024, &bytesRead, 10);
                    recorder->processI2SData(i2sData, bytesRead);
                } while (bytesRead > 0);
            }
        }
    }
}

void Recorder::start(i2s_port_t _i2sPort, i2s_config_t &_i2sConfig, int32_t _bufferSizeInBytes, TaskHandle_t _writerTaskHandle) {
    i2sPort = _i2sPort;
    writerTaskHandle = _writerTaskHandle;
    bufferSizeInSamples = _bufferSizeInBytes / sizeof(int16_t);
    bufferSizeInBytes = _bufferSizeInBytes;
    audioBuffer1 = (int16_t*) malloc(bufferSizeInBytes);
    audioBuffer2 = (int16_t*) malloc(bufferSizeInBytes);

    currentAudioBuffer = audioBuffer1;
    capturedAudioBuffer = audioBuffer2;

    i2s_driver_install(i2sPort, &_i2sConfig, 4, &i2sQueue);
    configureI2S();
    TaskHandle_t readerTaskHandle;
    xTaskCreatePinnedToCore(i2sReaderTask, "i2s Reader Task", 4096, this, 1, &readerTaskHandle, 0);
}

Recorder::Recorder(adc_unit_t _adcUnit, adc1_channel_t _adcChannel) {
    adcUnit = _adcUnit;
    adcChannel = _adcChannel;
}

void Recorder::configureI2S() {
    i2s_set_adc_mode(adcUnit, adcChannel);
    i2s_adc_enable(getI2SPort());
}

void Recorder::processI2SData(uint8_t* i2sData, size_t bytesRead) {
    uint16_t* rawSamples = (uint16_t*) i2sData;
    for (int i = 0; i < bytesRead / 2; i++) {
        addSample((2048 - (rawSamples[i] & 0xfff)) * 15);
    }
}
