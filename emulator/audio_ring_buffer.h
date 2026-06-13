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

    s16 buffer[SIZE * 2] __attribute__((aligned(64))); // interleaved stereo
    volatile unsigned read_idx __attribute__((aligned(64)));
    volatile unsigned write_idx __attribute__((aligned(64)));

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
        if (num_stereo_samples == 0) return;

        unsigned start = write_idx;
        if (start + num_stereo_samples <= SIZE) {
            // Single contiguous segment
            memcpy(&buffer[start * 2], samples, num_stereo_samples * 4);
        } else {
            // Wraps around the end
            unsigned first_chunk = SIZE - start;
            unsigned second_chunk = num_stereo_samples - first_chunk;
            memcpy(&buffer[start * 2], samples, first_chunk * 4);
            memcpy(&buffer[0], samples + first_chunk * 2, second_chunk * 4);
        }
        DataMemBarrier();
        write_idx = (write_idx + num_stereo_samples) & MASK;
    }

    unsigned Read(s16 *samples, unsigned num_stereo_samples) {
        unsigned avail = GetAvailable();
        if (num_stereo_samples > avail) {
            num_stereo_samples = avail;
        }
        if (num_stereo_samples == 0) return 0;

        DataMemBarrier();
        unsigned start = read_idx;
        if (start + num_stereo_samples <= SIZE) {
            // Single contiguous segment
            memcpy(samples, &buffer[start * 2], num_stereo_samples * 4);
        } else {
            // Wraps around the end
            unsigned first_chunk = SIZE - start;
            unsigned second_chunk = num_stereo_samples - first_chunk;
            memcpy(samples, &buffer[start * 2], first_chunk * 4);
            memcpy(samples + first_chunk * 2, &buffer[0], second_chunk * 4);
        }
        DataMemBarrier();
        read_idx = (read_idx + num_stereo_samples) & MASK;
        return num_stereo_samples;
    }
};

#endif
