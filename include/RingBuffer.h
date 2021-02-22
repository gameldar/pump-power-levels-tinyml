#ifndef __RINGBUFFER_H_
#define __RINGBUFFER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RB_FAIL ESP_FAIL
#define RB_ABORT -1
#define RB_WRITER_FINISHED -2
#define RB_READER_UNBLOCK -3

    typedef struct ringbuf {
        char* name;
        uint8_t* base;
        uint8_t* volatile readptr;
        uint8_t* volatile writeptr;
        volatile ssize_t fill_cnt;
        ssize_t size;
        xSemaphoreHandle can_read;
        xSemaphoreHandle can_write;
        xSemaphoreHandle lock;
        int abort_read;
        int abort_write;
        int writer_finished;
        int reader_unblock;
    } ringbuf_t;

    ringbuf_t* rb_init(const char* rb_name, uint32_t size);
    void rb_abort_read(ringbuf_t* rb);
    void rb_abort_write(ringbuf_t* rb);
    void rb_abort(ringbuf_t* rb);
    void rb_reset(ringbuf_t* rb);
    void rb_reset_and_abort_write(ringbuf_t* rb);
    void rb_stat(ringbuf_t* rb);
    ssize_t rb_filled(ringbuf_t* rb);
    ssize_t rb_available(ringbuf_t* rb);
    int rb_read(ringbuf_t* rb, uint8_t* buf, int len, uint32_t ticks_to_wait);
    int rb_write(ringbuf_t* rb, const uint8_t* buf, int len,
                 uint32_t ticks_to_wait);
    void rb_cleanup(ringbuf_t* rb);
    void rb_signal_writer_finished(ringbuf_t* rb);
    void rb_wakeup_reader(ringbuf_t* rb);
    int rb_is_writer_finished(ringbuf_t* rb);
#ifdef __cplusplus
}
#endif


#endif // __RINGBUFFER_H_
