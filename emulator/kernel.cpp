#include "kernel.h"
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/timer.h>
#include <circle/alloc.h>
#include <circle/font.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UTYPES_DEFINED 1
#include <pico/pico_int.h>
#ifdef __cplusplus
}
#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

// Global shared state
SharedState g_SharedState;
CFATFileSystem *g_pFileSystem = nullptr;

static CKernel *s_pThis = nullptr;

// Helper drawing utilities
static void DrawRect(u16 *pBuffer, u32 nPitch, int x1, int y1, int x2, int y2, u16 color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            pBuffer[y * nPitch + x] = color;
        }
    }
}

static void DrawBox(u16 *pBuffer, u32 nPitch, int x1, int y1, int x2, int y2, u16 color, int border_width = 1) {
    for (int b = 0; b < border_width; b++) {
        // Top and bottom lines
        for (int x = x1 + b; x <= x2 - b; x++) {
            pBuffer[(y1 + b) * nPitch + x] = color;
            pBuffer[(y2 - b) * nPitch + x] = color;
        }
        // Left and right lines
        for (int y = y1 + b; y <= y2 - b; y++) {
            pBuffer[y * nPitch + (x1 + b)] = color;
            pBuffer[y * nPitch + (x2 - b)] = color;
        }
    }
}

static void DrawChar(u16 *pBuffer, u32 nPitch, char c, int x, int y, u16 fg, u16 bg) {
    if (c < Font8x16.first_char || c > Font8x16.last_char) return;
    const u8 *char_data = (const u8 *)Font8x16.data + (c - Font8x16.first_char) * Font8x16.height;
    for (unsigned row = 0; row < Font8x16.height; row++) {
        u8 pixels = char_data[row];
        for (unsigned col = 0; col < Font8x16.width; col++) {
            if (pixels & (0x80 >> col)) {
                pBuffer[(y + row) * nPitch + (x + col)] = fg;
            } else if (bg != 0) {
                pBuffer[(y + row) * nPitch + (x + col)] = bg;
            }
        }
    }
}

static void DrawString(u16 *pBuffer, u32 nPitch, const char *str, int x, int y, u16 fg, u16 bg) {
    int cur_x = x;
    while (*str) {
        DrawChar(pBuffer, nPitch, *str, cur_x, y, fg, bg);
        cur_x += Font8x16.width;
        str++;
    }
}

// CEmulatorMultiCore Implementation
CEmulatorMultiCore::CEmulatorMultiCore(CMemorySystem *pMemorySystem, CKernel *pKernel)
    : CMultiCoreSupport(pMemorySystem),
      m_pKernel(pKernel)
{
}

void CEmulatorMultiCore::Run(unsigned nCore) {
    switch (nCore) {
        case 0:
            m_pKernel->RunOrchestrator();
            break;
        case 1:
            m_pKernel->RunVideoDomain();
            break;
        case 2:
            m_pKernel->RunAudioDomain();
            break;
        case 3:
            m_pKernel->RunInputDomain();
            break;
    }
}

// CKernel Implementation
CKernel::CKernel(void)
    : m_Screen(SCREEN_WIDTH, SCREEN_HEIGHT),
      m_Timer(&m_Interrupt),
      m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
      m_MultiCore(CMemorySystem::Get(), this),
      m_pKeyboard(nullptr),
      m_pOSDMenu(nullptr),
      m_pEmuOrchestrator(nullptr)
{
    s_pThis = this;
    m_pGamePad[0] = nullptr;
    m_pGamePad[1] = nullptr;
    m_ActLED.Blink(5);
}

CKernel::~CKernel(void) {
    s_pThis = nullptr;
}

boolean CKernel::Initialize(void) {
    boolean bOK = TRUE;

    if (bOK) bOK = m_Screen.Initialize();
    if (bOK) bOK = m_Serial.Initialize(115200);

    if (bOK) {
        CDevice *pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (pTarget == nullptr) pTarget = &m_Screen;
        bOK = m_Logger.Initialize(pTarget);
    }

    if (bOK) bOK = m_Interrupt.Initialize();
    if (bOK) bOK = m_Timer.Initialize();
    if (bOK) bOK = m_USBHCI.Initialize();
    if (bOK) bOK = m_MultiCore.Initialize();

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write("kernel", LogNotice, "MEGA-PI Baremetal Sega Mega Drive Emulator Booting...");
    
    // Start secondary cores
    m_MultiCore.Run(0);

    return ShutdownHalt;
}

void CKernel::RunOrchestrator() {
    m_Logger.Write("orchestrator", LogNotice, "Core 0: Orchestrator Active");

    // Mount FAT Filesystem on SD card
    CDevice *pPartition = m_DeviceNameService.GetDevice("sd1-1", TRUE);
    if (pPartition == nullptr) {
        pPartition = m_DeviceNameService.GetDevice("umsd1-1", TRUE);
    }

    if (pPartition == nullptr || !m_FileSystem.Mount(pPartition)) {
        m_Logger.Write("orchestrator", LogPanic, "Cannot mount filesystem on SD card (sd1-1 / umsd1-1)");
        return;
    }

    g_pFileSystem = &m_FileSystem;

    m_pOSDMenu = new COSDMenu(&m_FileSystem);
    m_pOSDMenu->Initialize();

    m_pEmuOrchestrator = new CEmuOrchestrator(&m_FileSystem);
    m_pEmuOrchestrator->Initialize();

    g_SharedState.in_menu = TRUE;

    while (1) {
        if (g_SharedState.in_menu) {
            // OSD Menu navigation using gamepad/keyboard pad state
            static u16 prev_pad1 = 0;
            u16 pad1 = g_SharedState.pad1;
            u16 pressed = pad1 & ~prev_pad1;
            prev_pad1 = pad1;

            if (pressed & (1 << 0)) { // Up
                m_pOSDMenu->MoveUp();
            }
            if (pressed & (1 << 1)) { // Down
                m_pOSDMenu->MoveDown();
            }
            if (pressed & ((1 << 6) | (1 << 7))) { // A or Start -> Select ROM
                const char *pRomName = m_pOSDMenu->GetSelectedRom();
                unsigned nRomSize = m_pOSDMenu->GetSelectedRomSize();
                if (pRomName) {
                    if (m_pEmuOrchestrator->LoadROM(pRomName, nRomSize)) {
                        g_SharedState.audio_ring_buffer.Init();
                        g_SharedState.in_menu = FALSE;
                        g_SharedState.escape_pressed = FALSE;
                    }
                }
            }
            CTimer::SimpleMsDelay(100);
        } else {
            // Run emulator frame
            m_pEmuOrchestrator->RunFrame();

            // Check if user requested return to OSD menu
            if (g_SharedState.escape_pressed) {
                g_SharedState.in_menu = TRUE;
                g_SharedState.escape_pressed = FALSE;
                m_pOSDMenu->Update();
            }

            // Check if save or load state is requested
            if (g_SharedState.save_state_requested) {
                g_SharedState.save_state_requested = FALSE;
                m_pEmuOrchestrator->SaveState(0);
            }
            if (g_SharedState.load_state_requested) {
                g_SharedState.load_state_requested = FALSE;
                m_pEmuOrchestrator->LoadState(0);
            }

            // Lock to 60 FPS
            static u64 last_time = CTimer::Get()->GetTicks();
            u64 current_time = CTimer::Get()->GetTicks();
            s64 elapsed = current_time - last_time;
            s64 frame_time = 16666; // 16.666 ms
            if (elapsed < frame_time) {
                CTimer::SimpleusDelay(frame_time - elapsed);
            }
            last_time = CTimer::Get()->GetTicks();
        }
    }
}

void CKernel::RunVideoDomain() {
    m_Logger.Write("video", LogNotice, "Core 1: Video Engine Active");

    CBcmFrameBuffer *pFB = m_Screen.GetFrameBuffer();
    if (pFB == nullptr) {
        m_Logger.Write("video", LogPanic, "Cannot get screen frame buffer");
        return;
    }

    u16 *pBuf = (u16 *)pFB->GetBuffer();
    u32 nPitch = pFB->GetPitch() / 2;

    // Clear screen to deep dark violet
    DrawRect(pBuf, nPitch, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR16(1, 1, 3));

    while (1) {
        if (g_SharedState.in_menu) {
            if (g_SharedState.menu_needs_redraw) {
                g_SharedState.menu_needs_redraw = FALSE;

                // Clear screen to deep dark violet
                DrawRect(pBuf, nPitch, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR16(1, 1, 3));

                // Draw central card/container
                int x1 = 80, y1 = 40, x2 = SCREEN_WIDTH - 80, y2 = SCREEN_HEIGHT - 40;
                DrawRect(pBuf, nPitch, x1, y1, x2, y2, COLOR16(4, 4, 8));
                
                // Draw glowing violet border
                DrawBox(pBuf, nPitch, x1, y1, x2, y2, COLOR16(16, 0, 31), 2);

                // Draw Title
                DrawString(pBuf, nPitch, "--- MEGA-PI BAREMETAL EMULATOR ---", 180, 60, COLOR16(31, 31, 31), 0);
                
                int selected = g_SharedState.menu_selected_idx;
                int num_lines = g_SharedState.menu_num_lines;
                if (num_lines > 0) {
                    char count_str[32];
                    snprintf(count_str, sizeof(count_str), "[%d/%d]", selected + 1, num_lines);
                    DrawString(pBuf, nPitch, count_str, x2 - 100, 60, COLOR16(31, 31, 15), COLOR16(4, 4, 8));
                }
                
                // Draw separator
                DrawRect(pBuf, nPitch, x1 + 20, 85, x2 - 20, 86, COLOR16(8, 8, 16));

                if (num_lines == 0) {
                    DrawString(pBuf, nPitch, "No ROMs found! Copy .bin, .md, .gen files to SD card.", 110, 150, COLOR16(31, 10, 10), 0);
                } else {
                    // List ROM files with viewport scrolling
                    int view_size = 15;
                    static int start_i = 0;
                    if (selected < start_i) {
                        start_i = selected;
                    } else if (selected >= start_i + view_size) {
                        start_i = selected - view_size + 1;
                    }
                    if (start_i < 0) start_i = 0;
                    if (start_i + view_size > num_lines) {
                        start_i = num_lines - view_size;
                        if (start_i < 0) start_i = 0;
                    }

                    for (int v = 0; v < view_size && (start_i + v) < num_lines; v++) {
                        int i = start_i + v;
                        int row_y = 110 + v * 20;
                        u16 fg = (i == selected) ? COLOR16(31, 31, 31) : COLOR16(20, 20, 20);
                        
                        if (i == selected) {
                            // Highlight selector bar
                            DrawRect(pBuf, nPitch, x1 + 10, row_y - 2, x2 - 10, row_y + 16, COLOR16(16, 0, 24));
                        }
                        
                        DrawString(pBuf, nPitch, g_SharedState.menu_lines[i], x1 + 20, row_y, fg, 0);
                    }
                }

                // Draw separator before instructions
                DrawRect(pBuf, nPitch, x1 + 20, y2 - 45, x2 - 20, y2 - 44, COLOR16(8, 8, 16));

                // Draw instructions
                DrawString(pBuf, nPitch, "UP/DOWN: Navigate  |  A/START: Boot ROM  |  ESC: Menu", x1 + 30, y2 - 30, COLOR16(15, 15, 15), 0);
            }
            pFB->WaitForVerticalSync();
            CTimer::SimpleMsDelay(16);
        } else {
            // Scale and draw game frame
            if (g_SharedState.video_frame_ready) {
                g_SharedState.video_frame_ready = FALSE;

                // Check active resolution H40 (320 wide) or H32 (256 wide)
                boolean h40 = (Pico.video.reg[0xC] & 1) != 0;
                int game_w = h40 ? 320 : 256;
                int game_h = 224;

                int scale_w = game_w * 2;
                int scale_h = game_h * 2;

                int start_x = (SCREEN_WIDTH - scale_w) / 2;
                int start_y = (SCREEN_HEIGHT - scale_h) / 2;

                // Clear borders
                if (!h40) {
                    // H32 border left and right
                    DrawRect(pBuf, nPitch, 0, 0, start_x - 1, SCREEN_HEIGHT - 1, 0);
                    DrawRect(pBuf, nPitch, start_x + scale_w, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 0);
                } else {
                    // H40 border left and right (screen width is 640, game scaled width is 640, so no side borders)
                }
                // Top and bottom borders
                DrawRect(pBuf, nPitch, 0, 0, SCREEN_WIDTH - 1, start_y - 1, 0);
                DrawRect(pBuf, nPitch, 0, start_y + scale_h, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 0);

                // Upscale using integer 2x nearest neighbor
                for (int y = 0; y < game_h; y++) {
                    u16 *src_row = g_SharedState.emu_frame_buffer + y * 320;
                    u16 *dest_row1 = pBuf + (start_y + 2 * y) * nPitch + start_x;
                    u16 *dest_row2 = dest_row1 + nPitch;

                    for (int x = 0; x < game_w; x++) {
                        u16 color = src_row[x];
                        dest_row1[2 * x] = color;
                        dest_row1[2 * x + 1] = color;
                        dest_row2[2 * x] = color;
                        dest_row2[2 * x + 1] = color;
                    }
                }
            } else {
                CTimer::SimpleusDelay(500);
            }
        }
    }
}

void CKernel::RunAudioDomain() {
    m_Logger.Write("audio", LogNotice, "Core 2: Audio Engine Active");

    CPWMSoundBaseDevice SoundDevice(&m_Interrupt, 44100, 512);
    if (!SoundDevice.AllocateQueue(100)) { // Allocate 100ms queue
        m_Logger.Write("audio", LogPanic, "Cannot allocate sound queue");
        return;
    }

    SoundDevice.SetWriteFormat(SoundFormatSigned16, 2);
    if (!SoundDevice.Start()) {
        m_Logger.Write("audio", LogPanic, "Cannot start sound device");
        return;
    }

    s16 local_buf[512 * 2];

    while (1) {
        unsigned avail = g_SharedState.audio_ring_buffer.GetAvailable();
        if (avail >= 256) {
            unsigned read = g_SharedState.audio_ring_buffer.Read(local_buf, 256);
            SoundDevice.Write(local_buf, read * 4); // each stereo sample is 4 bytes
        } else {
            CTimer::SimpleusDelay(1000);
        }
    }
}

void CKernel::RunInputDomain() {
    m_Logger.Write("input", LogNotice, "Core 3: Input Engine Active");

    while (1) {
        m_USBHCI.UpdatePlugAndPlay();

        // Detect and register gamepads
        for (unsigned nDevice = 1; nDevice <= 2; nDevice++) {
            if (m_pGamePad[nDevice-1] == nullptr) {
                m_pGamePad[nDevice-1] = (CUSBGamePadDevice *)
                    m_DeviceNameService.GetDevice("upad", nDevice, FALSE);
                
                if (m_pGamePad[nDevice-1] != nullptr) {
                    m_pGamePad[nDevice-1]->RegisterRemovedHandler(GamePadRemovedHandler, this);
                    m_pGamePad[nDevice-1]->RegisterStatusHandler(GamePadStatusHandler);
                    m_Logger.Write("input", LogNotice, "USB Gamepad %u Connected", nDevice);
                }
            }
        }

        // Detect and register keyboard
        if (m_pKeyboard == nullptr) {
            m_pKeyboard = (CUSBKeyboardDevice *)
                m_DeviceNameService.GetDevice("ukbd1", FALSE);
            
            if (m_pKeyboard != nullptr) {
                m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler, this);
                m_pKeyboard->RegisterKeyStatusHandlerRaw(KeyboardStatusHandlerRaw);
                m_Logger.Write("input", LogNotice, "USB Keyboard Connected");
            }
        }

        if (m_pKeyboard != nullptr) {
            m_pKeyboard->UpdateLEDs();
        }

        CTimer::SimpleMsDelay(10);
    }
}

// Event handlers
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState) {
    u16 pad = 0;

    // Check hats (D-pad)
    if (pState->nhats > 0) {
        int hat = pState->hats[0];
        if (hat >= 0 && hat <= 7) {
            if (hat == 0 || hat == 1 || hat == 7) pad |= (1 << 0); // Up
            if (hat == 3 || hat == 4 || hat == 5) pad |= (1 << 1); // Down
            if (hat == 5 || hat == 6 || hat == 7) pad |= (1 << 2); // Left
            if (hat == 1 || hat == 2 || hat == 3) pad |= (1 << 3); // Right
        }
    }

    // Check axes (fallback if D-pad hat not used)
    if (pState->naxes >= 2 && !(pad & 0xF)) {
        int x = pState->axes[0].value;
        int y = pState->axes[1].value;
        if (x < 64)  pad |= (1 << 2); // Left
        if (x > 192) pad |= (1 << 3); // Right
        if (y < 64)  pad |= (1 << 0); // Up
        if (y > 192) pad |= (1 << 1); // Down
    }

    // Map buttons: A, B, C, Start, X, Y, Z, Mode
    if (pState->nbuttons > 0) {
        if (pState->buttons & (1 << 0)) pad |= (1 << 6);  // A
        if (pState->buttons & (1 << 1)) pad |= (1 << 4);  // B
        if (pState->buttons & (1 << 2)) pad |= (1 << 5);  // C
        if (pState->buttons & (1 << 3)) pad |= (1 << 10); // X
        if (pState->buttons & (1 << 4)) pad |= (1 << 9);  // Y
        if (pState->buttons & (1 << 5)) pad |= (1 << 8);  // Z
        if (pState->buttons & (1 << 6)) pad |= (1 << 7);  // Start
        if (pState->buttons & (1 << 7)) pad |= (1 << 11); // Mode

        // L1 + R1 combos
        if ((pState->buttons & (1 << 4)) && (pState->buttons & (1 << 5))) {
            if (pad & (1 << 1)) { // D-pad Down
                g_SharedState.escape_pressed = TRUE;
            }
            if (pad & (1 << 2)) { // D-pad Left
                g_SharedState.load_state_requested = TRUE;
            }
            if (pad & (1 << 3)) { // D-pad Right
                g_SharedState.save_state_requested = TRUE;
            }
            // Mask gamepad input when hotkeys are used so the game doesn't receive them
            pad = 0;
        }
    }

    if (nDeviceIndex == 0) {
        g_SharedState.pad1 = pad;
    } else {
        g_SharedState.pad2 = pad;
    }
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogDebug, "USB Gamepad removed");
    for (int i = 0; i < 2; i++) {
        if (pThis->m_pGamePad[i] == pDevice) {
            pThis->m_pGamePad[i] = nullptr;
        }
    }
}

void CKernel::KeyboardStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]) {
    u16 pad = 0;
    boolean escape = FALSE;

    for (unsigned i = 0; i < 6; i++) {
        unsigned char key = RawKeys[i];
        if (key == 0) continue;

        // Map keys to Sega Mega Drive format: MXYZ SACB RLDU
        if (key == 0x52) pad |= (1 << 0); // Up arrow
        if (key == 0x51) pad |= (1 << 1); // Down arrow
        if (key == 0x50) pad |= (1 << 2); // Left arrow
        if (key == 0x4F) pad |= (1 << 3); // Right arrow

        if (key == 0x1D) pad |= (1 << 6); // Z -> A
        if (key == 0x1B) pad |= (1 << 4); // X -> B
        if (key == 0x06) pad |= (1 << 5); // C -> C
        if (key == 0x28) pad |= (1 << 7); // Enter -> Start

        if (key == 0x04) pad |= (1 << 10); // A -> X
        if (key == 0x16) pad |= (1 << 9);  // S -> Y
        if (key == 0x07) pad |= (1 << 8);  // D -> Z
        if (key == 0x2C) pad |= (1 << 11); // Space -> Mode

        if (key == 0x29) escape = TRUE; // Escape
        if (key == 0x3E) g_SharedState.save_state_requested = TRUE; // F5
        if (key == 0x41) g_SharedState.load_state_requested = TRUE; // F8
    }

    g_SharedState.pad1 = pad;
    if (escape) {
        g_SharedState.escape_pressed = TRUE;
    }
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogDebug, "USB Keyboard removed");
    pThis->m_pKeyboard = nullptr;
}
