#include <circle/timer.h>
#include <circle/string.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include <circle/stdarg.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <circle/fs/fat/fatfs.h>

extern CFATFileSystem *g_pFileSystem;

struct CFileWrapper {
    unsigned hFile;
    char title[128];
    boolean bWrite;
    unsigned int offset;
    unsigned int size;
    boolean in_use;
};

static CFileWrapper s_OpenFiles[8];

extern "C" {

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
    if (g_pFileSystem == nullptr) return nullptr;

    // Find a free wrapper slot
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (!s_OpenFiles[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return nullptr;

    boolean bWrite = FALSE;
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
        bWrite = TRUE;
    }

    unsigned hFile = 0;
    if (bWrite) {
        hFile = g_pFileSystem->FileCreate(pathname);
    } else {
        hFile = g_pFileSystem->FileOpen(pathname);
    }

    if (hFile == 0) {
        return nullptr;
    }

    s_OpenFiles[slot].hFile = hFile;
    strncpy(s_OpenFiles[slot].title, pathname, 127);
    s_OpenFiles[slot].title[127] = '\0';
    s_OpenFiles[slot].bWrite = bWrite;
    s_OpenFiles[slot].offset = 0;
    s_OpenFiles[slot].size = 0xFFFFFFFF;
    s_OpenFiles[slot].in_use = TRUE;

    return (FILE *)&s_OpenFiles[slot];
}

int fclose(FILE *stream) {
    if (g_pFileSystem == nullptr || stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    g_pFileSystem->FileClose(w->hFile);
    w->in_use = FALSE;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (g_pFileSystem == nullptr || stream == nullptr) return 0;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use || w->bWrite) return 0;

    unsigned bytesToRead = size * nmemb;
    if (bytesToRead == 0) return 0;

    unsigned read = g_pFileSystem->FileRead(w->hFile, ptr, bytesToRead);
    if (read == 0xFFFFFFFF) {
        return 0;
    }
    w->offset += read;
    return read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (g_pFileSystem == nullptr || stream == nullptr) return 0;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use || !w->bWrite) return 0;

    unsigned bytesToWrite = size * nmemb;
    if (bytesToWrite == 0) return 0;

    unsigned written = g_pFileSystem->FileWrite(w->hFile, ptr, bytesToWrite);
    if (written == 0xFFFFFFFF) {
        return 0;
    }
    w->offset += written;
    return written / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (g_pFileSystem == nullptr || stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    unsigned target = w->offset;
    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        target = w->offset + offset;
    } else {
        return -1;
    }

    if (target == w->offset) {
        return 0;
    }

    if (w->bWrite) {
        return -1;
    }

    if (target < w->offset) {
        g_pFileSystem->FileClose(w->hFile);
        w->hFile = g_pFileSystem->FileOpen(w->title);
        if (w->hFile == 0) {
            w->in_use = FALSE;
            return -1;
        }
        w->offset = 0;
    }

    char temp[256];
    while (w->offset < target) {
        unsigned diff = target - w->offset;
        unsigned to_read = diff > sizeof(temp) ? sizeof(temp) : diff;
        unsigned read = g_pFileSystem->FileRead(w->hFile, temp, to_read);
        if (read == 0 || read == 0xFFFFFFFF) {
            break;
        }
        w->offset += read;
    }

    return (w->offset == target) ? 0 : -1;
}

long ftell(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;
    return w->offset;
}

int fflush(FILE *stream) {
    return 0;
}

int fputc(int c, FILE *stream) {
    if (g_pFileSystem == nullptr || stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use || !w->bWrite) return -1;

    unsigned char ch = (unsigned char)c;
    unsigned written = g_pFileSystem->FileWrite(w->hFile, &ch, 1);
    if (written != 1) return -1;
    w->offset += 1;
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

// Picodrive SMS & 32x stubs
void Pico32xPrepare(void) {}
void PicoPrepareMS(void) {}
unsigned int PicoRead8_32x(unsigned int a) { return 0; }
unsigned int PicoRead16_32x(unsigned int a) { return 0; }
void PicoWrite8_32x(unsigned int a, unsigned int d) {}
void PicoWrite16_32x(unsigned int a, unsigned int d) {}

// Picodrive ZIP stubs
struct ZIP;
struct zipent;
ZIP* openzip(const char* path) { return nullptr; }
void closezip(ZIP* zip) {}
struct zipent* readzip(ZIP* zip) { return nullptr; }
int seekcompresszip(ZIP* zip, struct zipent* ent) { return -1; }

// Picodrive video mode change stub
void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count) {}

// Picodrive Sega CD MP3 & OGG stubs
int mp3_get_bitrate(void *f, int size) { return 0; }
void mp3_start_play(void *f, int pos) {}
void mp3_update(s32 *buffer, int length, int stereo) {}
int ogg_get_length(void *f_) { return 0; }
void ogg_start_play(void *f_, int sample_offset) {}
void ogg_stop_play(void) {}
void ogg_update(s32 *buffer, int length, int stereo) {}

} // extern "C"
