#include "emu_orchestrator.h"
#include "shared_state.h"
#include <circle/alloc.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#define UTYPES_DEFINED 1
#include <pico/pico_int.h>
#ifdef __cplusplus
}
#endif

#define ROM_BUFFER_SIZE (8 * 1024 * 1024)

extern "C" unsigned int p32x_event_times[1] = {0};

static const char FromOrchestrator[] = "orchestrator";

// Audio temp buffer for Picodrive output
static s16 g_AudioTempBuf[44100 / 50 * 2];

// Scanline callbacks for Picodrive
static int EmuScanBegin(unsigned int line) {
    if (line < 240) {
        // Point destination buffer directly to our shared frame buffer line
        Pico.est.DrawLineDest = (char *)(g_SharedState.emu_frame_buffer + line * 320);
    } else {
        Pico.est.DrawLineDest = nullptr;
    }
    return 0;
}

static int EmuScanEnd(unsigned int line) {
    return 0;
}

// Sound callback
static void EmuSoundCallback(int len) {
    // len is in bytes. Interleaved stereo 16-bit PCM (4 bytes per sample)
    unsigned num_stereo_samples = len / 4;
    g_SharedState.audio_ring_buffer.Write(g_AudioTempBuf, num_stereo_samples);
}

// lprintf implementation
extern "C" void lprintf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // Strip trailing newlines
    int len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[len-1] = '\0';
        len--;
    }
    CLogger::Get()->Write(FromOrchestrator, LogDebug, "%s", buf);
}

// plat_mmap stubs
extern "C" void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed) {
    return malloc(size);
}
extern "C" void *plat_mremap(void *ptr, size_t oldsize, size_t newsize) {
    return realloc(ptr, newsize);
}
extern "C" void plat_munmap(void *ptr, size_t size) {
    free(ptr);
}
extern "C" void *plat_mem_get_for_drc(size_t size) {
    return nullptr;
}
extern "C" int plat_mem_set_exec(void *ptr, size_t size) {
    return 0;
}

CEmuOrchestrator::CEmuOrchestrator(CFATFileSystem *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_pRomBuffer(nullptr),
      m_bRomLoaded(FALSE)
{
}

CEmuOrchestrator::~CEmuOrchestrator() {
    if (m_pRomBuffer) {
        delete[] m_pRomBuffer;
    }
}

boolean CEmuOrchestrator::Initialize() {
    m_pRomBuffer = new u8[ROM_BUFFER_SIZE];
    if (!m_pRomBuffer) {
        CLogger::Get()->Write(FromOrchestrator, LogPanic, "Failed to allocate ROM buffer");
        return FALSE;
    }

    // Initialize Picodrive options
    PicoIn.opt = POPT_EN_FM | POPT_EN_PSG | POPT_EN_Z80 | POPT_EN_STEREO | POPT_FM_YM2612;
    PicoIn.sndRate = 44100;
    PicoIn.sndOut = g_AudioTempBuf;
    PicoIn.writeSound = EmuSoundCallback;

    PicoInit();
    return TRUE;
}

boolean CEmuOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    if (nRomSize > ROM_BUFFER_SIZE) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "ROM too large: %u bytes", nRomSize);
        return FALSE;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading ROM: %s (%u bytes)", pRomName, nRomSize);
    
    unsigned hFile = m_pFileSystem->FileOpen(pRomName);
    if (!hFile) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to open ROM file: %s", pRomName);
        return FALSE;
    }

    unsigned bytesRead = m_pFileSystem->FileRead(hFile, m_pRomBuffer, nRomSize);
    m_pFileSystem->FileClose(hFile);

    if (bytesRead != nRomSize) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to read whole ROM. Read %u of %u bytes", bytesRead, nRomSize);
        return FALSE;
    }

    // Load media into Picodrive
    enum media_type_e type = PicoLoadMedia(pRomName, m_pRomBuffer, nRomSize, nullptr, nullptr, nullptr, nullptr);
    if (type == PM_ERROR) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Picodrive failed to load ROM: %s", pRomName);
        return FALSE;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "ROM Loaded Successfully! Type: %d", type);

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';

    // Set draw callbacks
    PicoDrawSetOutFormat(PDF_RGB555, 0);
    PicoDrawSetCallbacks(EmuScanBegin, EmuScanEnd);

    // Reset emulator
    PicoReset();

    m_bRomLoaded = TRUE;
    return TRUE;
}

void CEmuOrchestrator::RunFrame() {
    if (!m_bRomLoaded) return;

    // Map global input pad state into PicoIn
    PicoIn.pad[0] = g_SharedState.pad1;
    PicoIn.pad[1] = g_SharedState.pad2;

    // Run emulator frame
    PicoFrame();

    // Signal Core 1 (Video) that frame is ready
    g_SharedState.video_frame_ready = TRUE;
}

extern "C" int PicoState(const char *fname, int is_save);

void CEmuOrchestrator::SaveState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Saving state to: %s", stateName);
    int ret = PicoState(stateName, 1);
    if (ret == 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State saved successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to save state! error=%d", ret);
    }
}

void CEmuOrchestrator::LoadState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading state from: %s", stateName);
    int ret = PicoState(stateName, 0);
    if (ret == 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State loaded successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load state! error=%d", ret);
    }
}
