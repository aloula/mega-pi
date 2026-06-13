#include <circle/timer.h>
#include <circle/string.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include <circle/stdarg.h>
#include <circle/synchronize.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ff.h>
#include "shared_state.h"

extern FATFS *g_pFileSystem;

struct CFileWrapper {
    FIL file;
    char title[128];
    boolean in_use;
};

static CFileWrapper s_OpenFiles[8];

extern "C" {

static char dummy_reent[1024];
struct _reent * _impure_ptr = (struct _reent *)dummy_reent;

char *strdup(const char *s) {
    if (s == nullptr) return nullptr;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (dup != nullptr) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

FILE *fopen(const char *pathname, const char *mode) {
    // Find a free wrapper slot
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (!s_OpenFiles[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return nullptr;

    BYTE flags = 0;
    if (strchr(mode, 'r')) flags |= FA_READ;
    if (strchr(mode, 'w')) flags |= FA_WRITE | FA_CREATE_ALWAYS;
    if (strchr(mode, 'a')) flags |= FA_WRITE | FA_OPEN_APPEND;
    if (strchr(mode, '+')) flags |= FA_READ | FA_WRITE;

    // Convert potential raw path with leading slash to use drive specifier SD:/
    char fullPath[256];
    if (pathname[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "SD:%s", pathname);
    } else if (strncmp(pathname, "SD:", 3) != 0) {
        snprintf(fullPath, sizeof(fullPath), "SD:/%s", pathname);
    } else {
        strncpy(fullPath, pathname, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    }

    FRESULT res = f_open(&s_OpenFiles[slot].file, fullPath, flags);
    if (res != FR_OK) {
        return nullptr;
    }

    strncpy(s_OpenFiles[slot].title, fullPath, 127);
    s_OpenFiles[slot].title[127] = '\0';
    s_OpenFiles[slot].in_use = TRUE;

    return (FILE *)&s_OpenFiles[slot];
}

int fclose(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    f_close(&w->file);
    w->in_use = FALSE;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (stream == nullptr) return 0;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return 0;

    unsigned bytesToRead = size * nmemb;
    if (bytesToRead == 0) return 0;

    UINT read = 0;
    FRESULT res = f_read(&w->file, ptr, bytesToRead, &read);
    if (res != FR_OK) {
        return 0;
    }
    return read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (stream == nullptr) return 0;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return 0;

    unsigned bytesToWrite = size * nmemb;
    if (bytesToWrite == 0) return 0;

    UINT written = 0;
    FRESULT res = f_write(&w->file, ptr, bytesToWrite, &written);
    if (res != FR_OK) {
        return 0;
    }
    return written / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    FSIZE_t target = 0;
    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        target = f_tell(&w->file) + offset;
    } else if (whence == SEEK_END) {
        target = f_size(&w->file) + offset;
    } else {
        return -1;
    }

    FRESULT res = f_lseek(&w->file, target);
    return (res == FR_OK) ? 0 : -1;
}

long ftell(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;
    return f_tell(&w->file);
}

int fseeko(FILE *stream, off_t offset, int whence) {
    return fseek(stream, (long)offset, whence);
}

off_t ftello(FILE *stream) {
    return ftell(stream);
}

int fflush(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;
    f_sync(&w->file);
    return 0;
}

int fputc(int c, FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    unsigned char ch = (unsigned char)c;
    UINT written = 0;
    FRESULT res = f_write(&w->file, &ch, 1, &written);
    if (res != FR_OK || written != 1) return -1;
    return c;
}

FILE *fdopen(int fd, const char *mode) {
    return nullptr;
}

char *strerror(int errnum) {
    return (char *)"Unknown error";
}

int sscanf(const char *str, const char *format, ...) {
    return 0;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list var;
    va_start(var, fmt);
    CString Msg;
    Msg.FormatV(fmt, var);
    va_end(var);
    strcpy(buf, (const char *)Msg);
    return Msg.GetLength();
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (size == 0) return 0;
    va_list var;
    va_start(var, fmt);
    CString Msg;
    Msg.FormatV(fmt, var);
    va_end(var);
    size_t len = Msg.GetLength();
    if (size - 1 < len) {
        len = size - 1;
    }
    memcpy(buf, (const char *)Msg, len);
    buf[len] = '\0';
    return len;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list var) {
    if (size == 0) return 0;
    CString Msg;
    Msg.FormatV(fmt, var);
    size_t len = Msg.GetLength();
    if (size - 1 < len) {
        len = size - 1;
    }
    memcpy(buf, (const char *)Msg, len);
    buf[len] = '\0';
    return len;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) {
    if (tv) {
        u64 ticks = CTimer::GetClockTicks64(); // 1 tick = 1 microsecond
        tv->tv_sec = ticks / 1000000;
        tv->tv_usec = ticks % 1000000;
    }
    return 0;
}

// Missing string/stdlib functions
char *strrchr(const char *s, int c) {
    const char *last = nullptr;
    do {
        if (*s == (char)c) {
            last = s;
        }
    } while (*s++);
    return (char *)last;
}

#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif
#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)
#endif

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc;
    int c;
    unsigned long cutoff;
    int neg = 0, any, cutlim;

    do {
        c = *s++;
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');

    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }

    cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
    cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;
    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'A' && c <= 'Z') {
            c -= 'A' - 10;
        } else if (c >= 'a' && c <= 'z') {
            c -= 'a' - 10;
        } else {
            break;
        }
        if (c >= base) {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (neg) {
        acc = -acc;
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? s - 1 : nptr);
    }
    return acc;
}

static unsigned long next_rand = 1;
int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}
void srand(unsigned int seed) {
    next_rand = seed;
}

char *fgets(char *s, int size, FILE *stream) {
    return nullptr;
}

int feof(FILE *stream) {
    return 0;
}

int abs(int x) {
    return x < 0 ? -x : x;
}

int printf(const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    extern void lprintf(const char *fmt, ...);
    lprintf("%s", buf);
    return ret;
}

void perror(const char *s) {
    extern void lprintf(const char *fmt, ...);
    lprintf("perror: %s", s);
}

static void swap_bytes(char *a, char *b, size_t size) {
    char tmp;
    while (size--) {
        tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t num, size_t size, int (*compar)(const void *, const void *)) {
    if (num <= 1) return;
    char *char_base = (char *)base;
    char *pivot = char_base + (num - 1) * size;
    size_t i = 0;
    for (size_t j = 0; j < num - 1; j++) {
        if (compar(char_base + j * size, pivot) <= 0) {
            swap_bytes(char_base + i * size, char_base + j * size, size);
            i++;
        }
    }
    swap_bytes(char_base + i * size, pivot, size);
    if (i > 1) {
        qsort(char_base, i, size, compar);
    }
    if (num - i - 1 > 1) {
        qsort(char_base + (i + 1) * size, num - i - 1, size, compar);
    }
}

// Picodrive SMS & 32x stubs
void Pico32xPrepare(void) {}
void PicoPrepareMS(void) {}
#ifndef _ASM_MEMORY_C
unsigned int PicoRead8_32x(unsigned int a) { return 0; }
unsigned int PicoRead16_32x(unsigned int a) { return 0; }
void PicoWrite8_32x(unsigned int a, unsigned int d) {}
void PicoWrite16_32x(unsigned int a, unsigned int d) {}
#endif

// Picodrive ZIP stubs
struct ZIP;
struct zipent;
ZIP* openzip(const char* path) { return nullptr; }
void closezip(ZIP* zip) {}
struct zipent* readzip(ZIP* zip) { return nullptr; }
int seekcompresszip(ZIP* zip, struct zipent* ent) { return -1; }

// Picodrive video mode change callback
void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count) {
    g_SharedState.start_line = start_line;
    g_SharedState.game_h = line_count;
}

// Picodrive Sega CD MP3 & OGG stubs
int mp3_get_bitrate(void *f, int size) { return 0; }
void mp3_start_play(void *f, int pos) {}
void mp3_update(s32 *buffer, int length, int stereo) {}
int ogg_get_length(void *f_) { return 0; }
void ogg_start_play(void *f_, int sample_offset) {}
void ogg_stop_play(void) {}
void ogg_update(s32 *buffer, int length, int stereo) {}

void cache_flush_d_inval_i(void *start_addr, void *end_addr) {
    unsigned int start = (unsigned int)start_addr;
    unsigned int end = (unsigned int)end_addr;
    if (end > start) {
        CleanAndInvalidateDataCacheRange(start, end - start);
    }
    InvalidateInstructionCache();
}

} // extern "C"
