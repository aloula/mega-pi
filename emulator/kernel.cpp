#include "kernel.h"
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
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
#define COLOR15(red, green, blue) (((red) & 0x1F) << 10 | ((green) & 0x1F) << 5 | ((blue) & 0x1F))

// Global shared state
SharedState g_SharedState;
FATFS *g_pFileSystem = nullptr;

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
    unsigned char uc = (unsigned char)c;
    if (uc < Font8x16.first_char || uc > Font8x16.last_char) return;
    const u8 *char_data = (const u8 *)Font8x16.data + (uc - Font8x16.first_char) * Font8x16.height;
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
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
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

    // 1. Initialize Serial first so we can always output logs to serial
    bOK = m_Serial.Initialize(115200);

    // 2. Initialize Logger next, always targeting Serial (matching mega-pi-metal)
    if (bOK) {
        bOK = m_Logger.Initialize(&m_Serial);
    }

    if (bOK) {
        m_Logger.Write("kernel", LogNotice, "MEGA-PI Kernel Initializing...");
    }

    // 3. Initialize Screen (non-fatal if it fails)
    boolean bScreenOK = FALSE;
    if (bOK) {
        bScreenOK = m_Screen.Initialize();
        if (!bScreenOK) {
            m_Logger.Write("kernel", LogWarning, "Screen device initialization failed!");
        } else {
            m_Logger.Write("kernel", LogNotice, "Screen device initialized successfully.");
        }
    }

    // 4. Initialize Core Subsystems
    if (bOK) {
        bOK = m_Interrupt.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "Interrupt system initialization failed!");
    }
    if (bOK) {
        bOK = m_Timer.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "Timer initialization failed!");
    }
    if (bOK) {
        bOK = m_EMMC.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "EMMC driver initialization failed!");
    }
    if (bOK) {
        bOK = m_USBHCI.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "USB HCI initialization failed!");
    }
    if (bOK) {
        bOK = m_MultiCore.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "MultiCore system initialization failed!");
    }

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

    // Mount Elm ChaN's FAT Filesystem on SD card
    FRESULT mount_res = f_mount(&m_FileSystem, "SD:", 1);
    if (mount_res != FR_OK) {
        m_Logger.Write("orchestrator", LogPanic, "Cannot mount Elm ChaN fatfs filesystem on SD card (error %d)", mount_res);
        return;
    }

    g_pFileSystem = &m_FileSystem;

    m_pOSDMenu = new COSDMenu(&m_FileSystem);
    m_pOSDMenu->Initialize();

    m_pEmuOrchestrator = new CEmuOrchestrator(&m_FileSystem);
    m_pEmuOrchestrator->Initialize();

    // Turn activity LED ON once the emulator has fully booted
    m_ActLED.On();

    g_SharedState.in_menu = TRUE;
    g_SharedState.start_line = 8;
    g_SharedState.game_h = 224;

    static boolean just_entered_menu = TRUE;

    while (1) {
        if (g_SharedState.in_menu) {
            // OSD Menu navigation using gamepad/keyboard pad state with auto scrolling
            static u16 prev_pad1 = 0;
            static boolean start_released = TRUE;
            u16 pad1 = g_SharedState.pad1;
            
            if (just_entered_menu) {
                prev_pad1 = pad1;
                start_released = FALSE;
                just_entered_menu = FALSE;
            }

            if ((pad1 & ((1 << 6) | (1 << 7))) == 0) {
                start_released = TRUE;
            }
            
            u16 pressed = pad1 & ~prev_pad1;
            prev_pad1 = pad1;

            boolean isUpHeld = (pad1 & (1 << 0)) != 0;
            boolean isDownHeld = (pad1 & (1 << 1)) != 0;

            int repeatKey = 0;
            if (isUpHeld) {
                repeatKey = 1; // UP
            } else if (isDownHeld) {
                repeatKey = 2; // DOWN
            }

            static int activeRepeatKey = 0;
            static u64 lastRepeatTime = 0;
            static boolean repeatPhase = FALSE;

            boolean doUp = (pressed & (1 << 0)) != 0;
            boolean doDown = (pressed & (1 << 1)) != 0;

            if (repeatKey != 0) {
                if (activeRepeatKey != repeatKey) {
                    activeRepeatKey = repeatKey;
                    lastRepeatTime = CTimer::GetClockTicks64();
                    repeatPhase = FALSE;
                } else {
                    u64 elapsedMs = (CTimer::GetClockTicks64() - lastRepeatTime) / 1000;
                    u64 threshold = repeatPhase ? 80 : 400;
                    if (elapsedMs >= threshold) {
                        if (repeatKey == 1) doUp = TRUE;
                        if (repeatKey == 2) doDown = TRUE;
                        repeatPhase = TRUE;
                        lastRepeatTime = CTimer::GetClockTicks64();
                    }
                }
            } else {
                activeRepeatKey = 0;
                repeatPhase = FALSE;
            }

            if (doUp) {
                m_pOSDMenu->MoveUp();
            }
            if (doDown) {
                m_pOSDMenu->MoveDown();
            }

            if ((pressed & ((1 << 6) | (1 << 7))) && start_released) { // A or Start -> Select ROM
                const char *pRomName = m_pOSDMenu->GetSelectedRom();
                unsigned nRomSize = m_pOSDMenu->GetSelectedRomSize();
                if (pRomName) {
                    char fullPath[256];
                    snprintf(fullPath, sizeof(fullPath), "SD:/roms/%s", pRomName);
                    // Clear emulator frame buffer to black to prevent previous game flash
                    memset((void *)g_SharedState.emu_frame_buffer, 0, sizeof(g_SharedState.emu_frame_buffer));
                    g_SharedState.video_frame_ready = FALSE;
                    DataMemBarrier();

                    if (m_pEmuOrchestrator->LoadROM(fullPath, nRomSize)) {
                        g_SharedState.audio_ring_buffer.Init();
                        g_SharedState.in_menu = FALSE;
                        g_SharedState.escape_pressed = FALSE;
                        m_ActLED.On(); // Ensure activity LED remains ON after SD card read
                        just_entered_menu = TRUE;
                    }
                }
            }
            CTimer::SimpleMsDelay(10);
        } else {
            // Run emulator frame
            m_pEmuOrchestrator->RunFrame();

            // Check if user requested return to OSD menu
            if (g_SharedState.escape_pressed) {
                g_SharedState.in_menu = TRUE;
                g_SharedState.escape_pressed = FALSE;
                m_pOSDMenu->Update();
                m_ActLED.On(); // Ensure activity LED remains ON after SD card read
                just_entered_menu = TRUE;
            }

            // Check if save or load state is requested
            if (g_SharedState.save_state_requested) {
                g_SharedState.save_state_requested = FALSE;
                m_pEmuOrchestrator->SaveState(0);
                // Blink the activity LED 3 times quickly to confirm save
                m_ActLED.Blink(3, 50, 50);
                m_ActLED.On(); // Keep ON afterwards
            }
            if (g_SharedState.load_state_requested) {
                g_SharedState.load_state_requested = FALSE;
                m_pEmuOrchestrator->LoadState(0);
                // Blink the activity LED 3 times quickly to confirm load
                m_ActLED.Blink(3, 50, 50);
                m_ActLED.On(); // Keep ON afterwards
            }

            // Lock to 60 FPS (using microsecond precision ticks)
            static u64 last_time = CTimer::GetClockTicks64();
            u64 current_time = CTimer::GetClockTicks64();
            s64 elapsed = current_time - last_time;
            s64 frame_time = 16666; // 16.666 ms (16666 us)
            if (elapsed < frame_time) {
                CTimer::SimpleusDelay(frame_time - elapsed);
            }
            last_time = CTimer::GetClockTicks64();
        }
    }
}

void CKernel::RunVideoDomain() {
    m_Logger.Write("video", LogNotice, "Core 1: Video Engine Active");

    m_Logger.Write("video", LogNotice, "Font8x16: width=%u, height=%u, first=%u, last=%u, data=%p",
                   Font8x16.width, Font8x16.height, Font8x16.first_char, Font8x16.last_char, Font8x16.data);

    CBcmFrameBuffer *pFB = m_Screen.GetFrameBuffer();
    if (pFB == nullptr) {
        m_Logger.Write("video", LogPanic, "Cannot get screen frame buffer");
        return;
    }

    u16 *pBuf = (u16 *)pFB->GetBuffer();
    u32 nPitch = pFB->GetPitch() / 2;

    u16 *pBackBuffer = new u16[SCREEN_WIDTH * SCREEN_HEIGHT];
    if (pBackBuffer == nullptr) {
        m_Logger.Write("video", LogPanic, "Cannot allocate OSD backbuffer");
        return;
    }

    m_Logger.Write("video", LogNotice, "FrameBuffer: buf=%p, pitch=%u", pBuf, nPitch);

    // Clear screen to pure black
    DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));
    memcpy(pBuf, pBackBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));

    while (1) {
        if (g_SharedState.in_menu) {
            if (g_SharedState.menu_needs_redraw) {
                g_SharedState.menu_needs_redraw = FALSE;

                int selected = g_SharedState.menu_selected_idx;
                int num_lines = g_SharedState.menu_num_lines;
                m_Logger.Write("video", LogNotice, "OSD Redraw started. num_lines=%d, selected=%d", num_lines, selected);

                // Draw menu elements onto OSD backbuffer to prevent flickering (pure black background)
                DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));

                // Draw central card/container (cool dark green background!)
                int x1 = 40, y1 = 15, x2 = SCREEN_WIDTH - 40, y2 = SCREEN_HEIGHT - 15;
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, COLOR15(2, 6, 2));
                
                // Draw clean medium gray border (pure 15-bit gray)
                DrawBox(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, COLOR15(16, 16, 16), 2);

                // Draw Title (pure white)
                DrawString(pBackBuffer, SCREEN_WIDTH, "--- MEGA-PI BAREMETAL EMULATOR ---", 180, y1 + 15, COLOR15(31, 31, 31), 0);
                
                if (num_lines > 0) {
                    char count_str[32];
                    snprintf(count_str, sizeof(count_str), "[%d/%d]", selected + 1, num_lines);
                    DrawString(pBackBuffer, SCREEN_WIDTH, count_str, x2 - 100, y1 + 15, COLOR15(31, 31, 31), COLOR15(2, 6, 2));
                }
                
                // Draw separator (dark gray)
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y1 + 40, x2 - 20, y1 + 41, COLOR15(6, 6, 6));

                if (num_lines == 0) {
                    DrawString(pBackBuffer, SCREEN_WIDTH, "No ROMs found! Copy .bin, .md, .gen files to SD card.", 110, 150, COLOR15(31, 10, 10), 0);
                } else {
                    // List ROM files with centered viewport scrolling (mega-pi-metal style)
                    int view_size = 18;
                    int start_i = 0;
                    if (num_lines > view_size) {
                        start_i = selected - view_size / 2;
                        if (start_i < 0) start_i = 0;
                        if (start_i + view_size > num_lines) {
                            start_i = num_lines - view_size;
                        }
                    }

                    int start_y = y1 + 55;
                    for (int v = 0; v < view_size && (start_i + v) < num_lines; v++) {
                        int i = start_i + v;
                        int row_y = start_y + v * 20;
                        u16 fg = (i == selected) ? COLOR15(31, 31, 31) : COLOR15(16, 16, 16);
                        
                        if (i == selected) {
                            // Highlight selector bar (ends at x2 - 22 to not overlap scrollbar) (medium-dark gray)
                            DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 10, row_y - 2, x2 - 22, row_y + 16, COLOR15(8, 8, 8));
                        }
                        
                        DrawString(pBackBuffer, SCREEN_WIDTH, g_SharedState.menu_lines[i], x1 + 20, row_y, fg, 0);
                    }

                    // Draw scrollbar indicator if list is scrollable
                    if (num_lines > view_size) {
                        int track_x = x2 - 18;
                        int track_y = start_y;
                        int track_w = 4;
                        int track_h = view_size * 20 - 4;
                        
                        // Track (dark gray)
                        DrawRect(pBackBuffer, SCREEN_WIDTH, track_x, track_y, track_x + track_w, track_y + track_h, COLOR15(6, 6, 6));
                        
                        // Thumb (medium gray)
                        int thumb_h = (view_size * track_h) / num_lines;
                        if (thumb_h < 10) thumb_h = 10;
                        int thumb_y = track_y + (start_i * track_h) / num_lines;
                        if (thumb_y + thumb_h > track_y + track_h) {
                            thumb_y = track_y + track_h - thumb_h;
                        }
                        DrawRect(pBackBuffer, SCREEN_WIDTH, track_x, thumb_y, track_x + track_w, thumb_y + thumb_h, COLOR15(16, 16, 16));
                    }
                }

                // Draw separator before instructions (dark gray)
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y2 - 28, x2 - 20, y2 - 27, COLOR15(6, 6, 6));

                // Draw instructions (medium-dark gray)
                DrawString(pBackBuffer, SCREEN_WIDTH, "UP/DOWN: Navigate  |  A/START: Boot ROM  |  START+SELECT: Menu", x1 + 30, y2 - 20, COLOR15(12, 12, 12), 0);

                // Copy fully rendered backbuffer to the active framebuffer in a single fast operation
                memcpy(pBuf, pBackBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
            }
            pFB->WaitForVerticalSync();
            CTimer::SimpleMsDelay(16);
        } else {
            // Scale and draw game frame
            if (g_SharedState.video_frame_ready) {
                DataMemBarrier();
                g_SharedState.video_frame_ready = FALSE;

                int game_h = g_SharedState.game_h;
                int start_line = g_SharedState.start_line;

                if (game_h < 1) game_h = 224;
                if (game_h > 320) game_h = 320;
                if (start_line + game_h > 320) {
                    game_h = 320 - start_line;
                }
                if (game_h < 1) {
                    start_line = 0;
                    game_h = 224;
                }

                int scale_h = game_h * 2;

                int start_y = (SCREEN_HEIGHT - scale_h) / 2;

                // Wait for vertical sync to prevent screen tearing (especially in games like Battle Squadron)
                pFB->WaitForVerticalSync();

                // Top and bottom borders
                DrawRect(pBuf, nPitch, 0, 0, SCREEN_WIDTH - 1, start_y - 1, 0);
                DrawRect(pBuf, nPitch, 0, start_y + scale_h, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 0);

                // Upscale using optimized 64-bit integer 2x nearest neighbor
                for (int y = 0; y < game_h; y++) {
                    u32 *src32 = (u32 *)(g_SharedState.emu_frame_buffer + (start_line + y) * 320);
                    u64 *dest64_1 = (u64 *)(pBuf + (start_y + 2 * y) * nPitch);
                    u64 *dest64_2 = dest64_1 + (nPitch / 4);

                    for (int x = 0; x < 160; x++) {
                        u32 pixels = src32[x];
                        u32 p1 = pixels & 0xFFFF;
                        u32 p2 = pixels >> 16;
                        
                        u32 p1_32 = (p1 << 16) | p1;
                        u32 p2_32 = (p2 << 16) | p2;
                        u64 color64 = ((u64)p2_32 << 32) | p1_32;

                        dest64_1[x] = color64;
                        dest64_2[x] = color64;
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

    // Dynamic selection of audio device (HDMI or PWM) based on options
    const char *pSoundDeviceName = m_Options.GetSoundDevice();
    if (pSoundDeviceName == nullptr || pSoundDeviceName[0] == '\0') {
        pSoundDeviceName = m_Options.GetAppOptionString("snddevice");
    }

    CSoundBaseDevice *pSoundDevice = nullptr;
    if (pSoundDeviceName != nullptr && strcmp(pSoundDeviceName, "sndhdmi") == 0) {
        m_Logger.Write("audio", LogNotice, "Using HDMI audio device");
        pSoundDevice = new CHDMISoundBaseDevice(&m_Interrupt, 44100, 384 * 10);
    } else {
        m_Logger.Write("audio", LogNotice, "Using PWM audio device (headphones)");
        pSoundDevice = new CPWMSoundBaseDevice(&m_Interrupt, 44100, 2048);
    }

    if (pSoundDevice == nullptr) {
        m_Logger.Write("audio", LogPanic, "Failed to instantiate sound device");
        return;
    }

    if (!pSoundDevice->AllocateQueue(100)) { // Allocate 100ms queue
        m_Logger.Write("audio", LogPanic, "Cannot allocate sound queue");
        delete pSoundDevice;
        return;
    }

    pSoundDevice->SetWriteFormat(SoundFormatSigned16, 2);
    if (!pSoundDevice->Start()) {
        m_Logger.Write("audio", LogPanic, "Cannot start sound device");
        delete pSoundDevice;
        return;
    }

    s16 local_buf[512 * 2];

    while (1) {
        unsigned avail = g_SharedState.audio_ring_buffer.GetAvailable();
        if (avail > 0) {
            if (avail > 512) avail = 512;
            unsigned read = g_SharedState.audio_ring_buffer.Read(local_buf, avail);
            pSoundDevice->Write(local_buf, read * 4); // each stereo sample is 4 bytes
        } else {
            CTimer::SimpleusDelay(100); // 100us low-latency poll interval
        }
    }

    delete pSoundDevice;
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

        CTimer::SimpleMsDelay(100);
    }
}

// Event handlers
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState) {
    u16 pad = 0;

    // 1. Check D-pad buttons (for Xbox 360 and others mapping D-pad to buttons)
    if (pState->buttons & GamePadButtonUp)    pad |= (1 << 0); // Up
    if (pState->buttons & GamePadButtonDown)  pad |= (1 << 1); // Down
    if (pState->buttons & GamePadButtonLeft)  pad |= (1 << 2); // Left
    if (pState->buttons & GamePadButtonRight) pad |= (1 << 3); // Right

    // 2. Check hats (D-pad) as fallback
    if (pState->nhats > 0 && !(pad & 0xF)) {
        int hat = pState->hats[0];
        if (hat >= 0 && hat <= 7) {
            if (hat == 0 || hat == 1 || hat == 7) pad |= (1 << 0); // Up
            if (hat == 3 || hat == 4 || hat == 5) pad |= (1 << 1); // Down
            if (hat == 5 || hat == 6 || hat == 7) pad |= (1 << 2); // Left
            if (hat == 1 || hat == 2 || hat == 3) pad |= (1 << 3); // Right
        }
    }

    // 3. Check analog axes as fallback
    if (pState->naxes >= 2 && !(pad & 0xF)) {
        int x = pState->axes[0].value;
        int y = pState->axes[1].value;
        if (x < 64)  pad |= (1 << 2); // Left
        if (x > 192) pad |= (1 << 3); // Right
        if (y < 64)  pad |= (1 << 0); // Up
        if (y > 192) pad |= (1 << 1); // Down
    }

    // 4. Map Action buttons: A, B, C, Start, X, Y, Z, Mode
    // Sega Mega Drive virtual pads map:
    // pad bit 6: Sega A
    // pad bit 4: Sega B
    // pad bit 5: Sega C
    // pad bit 10: Sega X
    // pad bit 9: Sega Y
    // pad bit 8: Sega Z
    // pad bit 7: Sega Start
    // pad bit 11: Sega Mode
    if (pState->buttons & GamePadButtonA)     pad |= (1 << 6);  // Sega A
    if (pState->buttons & GamePadButtonB)     pad |= (1 << 4);  // Sega B
    if (pState->buttons & GamePadButtonRT)    pad |= (1 << 5);  // Sega C (R2/RT -> C)
    if (pState->buttons & GamePadButtonLT)    pad |= (1 << 8);  // Sega Z (L2/LT -> Z)
    if (pState->buttons & GamePadButtonX)     pad |= (1 << 10); // Sega X
    if (pState->buttons & GamePadButtonY)     pad |= (1 << 9);  // Sega Y
    if (pState->buttons & GamePadButtonRB)    pad |= (1 << 8);  // Sega Z (fallback RB -> Z)
    if (pState->buttons & GamePadButtonLB)    pad |= (1 << 10); // Sega X (fallback LB -> X)
    if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);  // Sega Start
    if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11); // Sega Mode

    // START + SELECT combo -> Exit to menu
    if ((pState->buttons & GamePadButtonStart) && (pState->buttons & GamePadButtonSelect)) {
        g_SharedState.escape_pressed = TRUE;
    }

    // SELECT + D-pad combos for state save/load
    if (pState->buttons & GamePadButtonSelect) {
        if (pad & (1 << 2)) { // D-pad Left -> Save state
            g_SharedState.save_state_requested = TRUE;
            pad = 0; // Mask inputs when combo is held
        }
        if (pad & (1 << 3)) { // D-pad Right -> Load state
            g_SharedState.load_state_requested = TRUE;
            pad = 0; // Mask inputs when combo is held
        }
    }

    static u16 last_pad1 = 0xFFFF;
    static u16 last_pad2 = 0xFFFF;

    if (nDeviceIndex == 0) {
        if (pad != last_pad1) {
            last_pad1 = pad;
            g_SharedState.pad1 = pad;
            DataMemBarrier();
        }
    } else {
        if (pad != last_pad2) {
            last_pad2 = pad;
            g_SharedState.pad2 = pad;
            DataMemBarrier();
        }
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

    static u16 last_kbd_pad = 0xFFFF;
    if (pad != last_kbd_pad) {
        last_kbd_pad = pad;
        g_SharedState.pad1 = pad;
        DataMemBarrier();
    }
    if (escape) {
        g_SharedState.escape_pressed = TRUE;
        DataMemBarrier();
    }
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogDebug, "USB Keyboard removed");
    pThis->m_pKeyboard = nullptr;
}
