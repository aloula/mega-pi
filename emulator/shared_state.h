#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <circle/types.h>
#include "audio_ring_buffer.h"

#define MAX_ROMS 1000

struct SharedState {
    volatile u16 pad1 __attribute__((aligned(64)));
    volatile u16 pad2 __attribute__((aligned(64)));
    volatile boolean escape_pressed __attribute__((aligned(64)));
    volatile boolean save_state_requested __attribute__((aligned(64)));
    volatile boolean load_state_requested __attribute__((aligned(64)));

    volatile boolean in_menu __attribute__((aligned(64)));
    volatile boolean menu_needs_redraw __attribute__((aligned(64)));
    char menu_lines[MAX_ROMS][80] __attribute__((aligned(64)));
    int menu_num_lines;
    int menu_selected_idx;
    char menu_tab_names[5][16] __attribute__((aligned(64)));
    int menu_active_tab;

    // Emulator frame buffer (320x320, RGB565) to support full PAL + overscan start lines
    u16 emu_frame_buffer[320 * 320] __attribute__((aligned(64)));
    volatile boolean video_frame_ready __attribute__((aligned(64)));
    volatile int start_line;
    volatile int game_h;

    // Audio ring buffer
    AudioRingBuffer audio_ring_buffer __attribute__((aligned(64)));
} __attribute__((aligned(64)));

extern SharedState g_SharedState;

#endif
