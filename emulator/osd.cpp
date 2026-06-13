#include "osd.h"
#include <circle/string.h>
#include <circle/logger.h>
#include <string.h>
#include <stdio.h>

static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

COSDMenu::COSDMenu(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_RomCount(0),
      m_SelectedIndex(0)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
    }
}

COSDMenu::~COSDMenu() {}

boolean COSDMenu::Initialize() {
    ScanRoms();
    Update();
    return TRUE;
}

void COSDMenu::ScanRoms() {
    m_RomCount = 0;
    DIR dir;
    FILINFO fileInfo;
    FRESULT res = f_findfirst(&dir, &fileInfo, "SD:/roms", "*");
    if (res != FR_OK) {
        CLogger::Get()->Write("OSD", LogError, "f_findfirst failed on SD:/roms: %d", res);
        return;
    }

    while (res == FR_OK && fileInfo.fname[0] != '\0' && m_RomCount < MAX_ROMS) {
        if (!(fileInfo.fattrib & AM_DIR) && !(fileInfo.fattrib & (AM_HID | AM_SYS))) {
            const char *pDot = strrchr(fileInfo.fname, '.');
            if (pDot != 0) {
                pDot++;
                if (my_strcasecmp(pDot, "bin") == 0 ||
                    my_strcasecmp(pDot, "md") == 0 ||
                    my_strcasecmp(pDot, "gen") == 0 ||
                    my_strcasecmp(pDot, "iso") == 0 ||
                    my_strcasecmp(pDot, "cue") == 0 ||
                    my_strcasecmp(pDot, "chd") == 0) {
                    strncpy(m_RomFiles[m_RomCount], fileInfo.fname, sizeof(m_RomFiles[m_RomCount]) - 1);
                    m_RomFiles[m_RomCount][sizeof(m_RomFiles[m_RomCount]) - 1] = '\0';
                    m_RomSizes[m_RomCount] = fileInfo.fsize;
                    m_RomCount++;
                }
            }
        }
        res = f_findnext(&dir, &fileInfo);
    }
    f_closedir(&dir);
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_RomCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;

    for (int i = 0; i < m_RomCount; i++) {
        char temp[80];
        strncpy(temp, m_RomFiles[i], sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        
        char *pDot = strrchr(temp, '.');
        if (pDot != nullptr) {
            *pDot = '\0';
        }
        
        unsigned size_kb = m_RomSizes[i] / 1024;
        if (size_kb >= 1024) {
            snprintf(g_SharedState.menu_lines[i], 80, "%s (%u MB)", temp, size_kb / 1024);
        } else {
            snprintf(g_SharedState.menu_lines[i], 80, "%s (%u KB)", temp, size_kb);
        }
    }
    g_SharedState.menu_needs_redraw = TRUE;
}

void COSDMenu::MoveUp() {
    if (m_RomCount == 0) return;
    if (m_SelectedIndex > 0) {
        m_SelectedIndex--;
    } else {
        m_SelectedIndex = m_RomCount - 1; // wrap around
    }
    Update();
}

void COSDMenu::MoveDown() {
    if (m_RomCount == 0) return;
    if (m_SelectedIndex < m_RomCount - 1) {
        m_SelectedIndex++;
    } else {
        m_SelectedIndex = 0; // wrap around
    }
    Update();
}

const char *COSDMenu::GetSelectedRom() {
    if (m_RomCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_RomCount) {
        return nullptr;
    }
    return m_RomFiles[m_SelectedIndex];
}

unsigned COSDMenu::GetSelectedRomSize() {
    if (m_RomCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_RomCount) {
        return 0;
    }
    return m_RomSizes[m_SelectedIndex];
}
