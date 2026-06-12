#ifndef AUDIO_RING_BUFFER_H
#define AUDIO_RING_BUFFER_H

#include <circle/types.h>
#include <circle/string.h>
#include <circle/synchronize.h>
#include <string.h>

class AudioRingBuffer {
public:
    static const unsigned SIZE = 8192;
    static const unsigned MASK = SIZE - 1;

    s16 buffer[SIZE * 2]; // interleaved stereo
    volatile unsigned read_idx;
    volatile unsigned write_idx;

    void Init() {
        read_idx = 0;
        write_idx = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    unsigned GetAvailable() const {
        return (write_idx - read_idx) & MASK;
    }

    unsigned GetFreeSpace() const {
        return (read_idx - write_idx - 1) & MASK;
    }

    void Write(const s16 *samples, unsigned num_stereo_samples) {
        unsigned free = GetFreeSpace();
        if (num_stereo_samples > free) {
            num_stereo_samples = free;
        }
        for (unsigned i = 0; i < num_stereo_samples; i++) {
            unsigned idx = (write_idx + i) & MASK;
            buffer[idx * 2] = samples[i * 2];
            buffer[idx * 2 + 1] = samples[i * 2 + 1];
        }
        DataMemBarrier();
        write_idx = (write_idx + num_stereo_samples) & MASK;
    }

    unsigned Read(s16 *samples, unsigned num_stereo_samples) {
        unsigned avail = GetAvailable();
        if (num_stereo_samples > avail) {
            num_stereo_samples = avail;
        }
        DataMemBarrier();
        for (unsigned i = 0; i < num_stereo_samples; i++) {
            unsigned idx = (read_idx + i) & MASK;
            samples[i * 2] = buffer[idx * 2];
            samples[i * 2 + 1] = buffer[idx * 2 + 1];
        }
        DataMemBarrier();
        read_idx = (read_idx + num_stereo_samples) & MASK;
        return num_stereo_samples;
    }
};

#endif
