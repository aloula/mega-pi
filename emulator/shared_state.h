#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <circle/types.h>
#include "audio_ring_buffer.h"

#define MAX_ROMS 100

struct SharedState {
    volatile u16 pad1;
    volatile u16 pad2;
    volatile boolean escape_pressed;
    volatile boolean save_state_requested;
    volatile boolean load_state_requested;

    volatile boolean in_menu;
    volatile boolean menu_needs_redraw;
    char menu_lines[MAX_ROMS][80];
    int menu_num_lines;
    int menu_selected_idx;

    // Emulator frame buffer (320x240, RGB565)
    u16 emu_frame_buffer[320 * 240];
    volatile boolean video_frame_ready;

    // Audio ring buffer
    AudioRingBuffer audio_ring_buffer;
};

extern SharedState g_SharedState;

#endif
