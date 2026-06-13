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
      m_SelectedIndex(0),
      m_GenesisCount(0),
      m_MegaCDCount(0),
      m_FilteredCount(0),
      m_ActiveTab(0)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_GenesisIndices[i] = -1;
        m_MegaCDIndices[i] = -1;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 5; t++) {
        m_TabLabels[t][0] = '\0';
    }
}

COSDMenu::~COSDMenu() {}

boolean COSDMenu::Initialize() {
    ScanRoms();
    CalculateTabLabels();
    BuildFilteredList();
    Update();
    return TRUE;
}

void COSDMenu::ScanRoms() {
    m_RomCount = 0;
    m_GenesisCount = 0;
    m_MegaCDCount = 0;

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

    // Sort ROMs alphabetically
    for (int i = 0; i < m_RomCount - 1; i++) {
        for (int j = i + 1; j < m_RomCount; j++) {
            if (my_strcasecmp(m_RomFiles[i], m_RomFiles[j]) > 0) {
                // Swap files
                char tempFile[128];
                strcpy(tempFile, m_RomFiles[i]);
                strcpy(m_RomFiles[i], m_RomFiles[j]);
                strcpy(m_RomFiles[j], tempFile);
                // Swap sizes
                unsigned tempSize = m_RomSizes[i];
                m_RomSizes[i] = m_RomSizes[j];
                m_RomSizes[j] = tempSize;
            }
        }
    }

    // Categorize ROMs into Genesis vs Mega CD
    auto is_mega_cd = [](const char *filename) -> boolean {
        const char *pDot = strrchr(filename, '.');
        if (pDot != nullptr) {
            pDot++;
            if (my_strcasecmp(pDot, "cue") == 0 ||
                my_strcasecmp(pDot, "chd") == 0 ||
                my_strcasecmp(pDot, "iso") == 0) {
                return TRUE;
            }
        }
        return FALSE;
    };

    for (int i = 0; i < m_RomCount; i++) {
        if (is_mega_cd(m_RomFiles[i])) {
            m_MegaCDIndices[m_MegaCDCount++] = i;
        } else {
            m_GenesisIndices[m_GenesisCount++] = i;
        }
    }
}

void COSDMenu::CalculateTabLabels() {
    // Tab 0: "ALL"
    strcpy(m_TabLabels[0], "ALL");

    // Tab 4: "Mega CD"
    strcpy(m_TabLabels[4], "Mega CD");

    // Helper to get uppercase starting character or '#' for numbers/symbols
    auto get_char = [this](int genesis_idx) -> char {
        if (genesis_idx < 0 || genesis_idx >= m_GenesisCount) return '?';
        const char *name = m_RomFiles[m_GenesisIndices[genesis_idx]];
        if (name == nullptr || *name == '\0') return '?';
        char c = *name;
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
        return '#';
    };

    // Calculate equal partitions of the sorted Genesis games
    int size1 = m_GenesisCount / 3;
    int size2 = m_GenesisCount / 3;
    int size3 = m_GenesisCount - (size1 + size2);

    // Tab 1: Alpha 1
    if (size1 > 0) {
        char c_start = get_char(0);
        char c_end = get_char(size1 - 1);
        if (c_start == c_end) {
            snprintf(m_TabLabels[1], sizeof(m_TabLabels[1]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[1], sizeof(m_TabLabels[1]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[1], "A-G");
    }

    // Tab 2: Alpha 2
    if (size2 > 0) {
        char c_start = get_char(size1);
        char c_end = get_char(size1 + size2 - 1);
        if (c_start == c_end) {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[2], "H-P");
    }

    // Tab 3: Alpha 3
    if (size3 > 0) {
        char c_start = get_char(size1 + size2);
        char c_end = get_char(m_GenesisCount - 1);
        if (c_start == c_end) {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[3], "Q-Z");
    }
}

void COSDMenu::BuildFilteredList() {
    m_FilteredCount = 0;
    if (m_ActiveTab == 0) {
        // ALL tab: include all scanned roms
        for (int i = 0; i < m_RomCount; i++) {
            m_FilteredIndices[m_FilteredCount++] = i;
        }
    }
    else if (m_ActiveTab >= 1 && m_ActiveTab <= 3) {
        // Alphabetical Genesis tabs
        if (m_GenesisCount > 0) {
            int part = m_ActiveTab - 1;
            int size1 = m_GenesisCount / 3;
            int size2 = m_GenesisCount / 3;
            int start_idx = 0;
            int end_idx = 0;

            if (part == 0) {
                start_idx = 0;
                end_idx = size1 - 1;
            } else if (part == 1) {
                start_idx = size1;
                end_idx = size1 + size2 - 1;
            } else if (part == 2) {
                start_idx = size1 + size2;
                end_idx = m_GenesisCount - 1;
            }

            for (int i = start_idx; i <= end_idx && i < m_GenesisCount; i++) {
                m_FilteredIndices[m_FilteredCount++] = m_GenesisIndices[i];
            }
        }
    }
    else if (m_ActiveTab == 4) {
        // Mega CD tab: include only Mega CD indices
        for (int i = 0; i < m_MegaCDCount; i++) {
            m_FilteredIndices[m_FilteredCount++] = m_MegaCDIndices[i];
        }
    }
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_FilteredCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;
    g_SharedState.menu_active_tab = m_ActiveTab;

    // Copy tab titles to shared state
    for (int t = 0; t < 5; t++) {
        strncpy(g_SharedState.menu_tab_names[t], m_TabLabels[t], sizeof(g_SharedState.menu_tab_names[t]) - 1);
        g_SharedState.menu_tab_names[t][sizeof(g_SharedState.menu_tab_names[t]) - 1] = '\0';
    }

    for (int i = 0; i < m_FilteredCount; i++) {
        int orig_idx = m_FilteredIndices[i];
        char temp[80];
        strncpy(temp, m_RomFiles[orig_idx], sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *pDot = strrchr(temp, '.');
        if (pDot != nullptr) {
            *pDot = '\0';
        }

        unsigned size_kb = m_RomSizes[orig_idx] / 1024;
        if (size_kb >= 1024) {
            snprintf(g_SharedState.menu_lines[i], 80, "%s (%u MB)", temp, size_kb / 1024);
        } else {
            snprintf(g_SharedState.menu_lines[i], 80, "%s (%u KB)", temp, size_kb);
        }
    }

    DataMemBarrier();
    g_SharedState.menu_needs_redraw = TRUE;
}

void COSDMenu::MoveUp() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex > 0) {
        m_SelectedIndex--;
    } else {
        m_SelectedIndex = m_FilteredCount - 1; // wrap around
    }
    Update();
}

void COSDMenu::MoveDown() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex < m_FilteredCount - 1) {
        m_SelectedIndex++;
    } else {
        m_SelectedIndex = 0; // wrap around
    }
    Update();
}

void COSDMenu::MoveLeft() {
    if (m_ActiveTab > 0) {
        m_ActiveTab--;
    } else {
        m_ActiveTab = 4;
    }
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::MoveRight() {
    if (m_ActiveTab < 4) {
        m_ActiveTab++;
    } else {
        m_ActiveTab = 0;
    }
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

const char *COSDMenu::GetSelectedRom() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return nullptr;
    }
    return m_RomFiles[m_FilteredIndices[m_SelectedIndex]];
}

unsigned COSDMenu::GetSelectedRomSize() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return 0;
    }
    return m_RomSizes[m_FilteredIndices[m_SelectedIndex]];
}
