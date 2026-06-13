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
      m_ActiveTab(0),
      m_TabSplitK1(8),
      m_TabSplitK2(16)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_RomFavorites[i] = FALSE;
        m_GenesisIndices[i] = -1;
        m_MegaCDIndices[i] = -1;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 6; t++) {
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
    LoadFavorites();
}

void COSDMenu::CalculateTabLabels() {
    // Tab 0: "ALL"
    strcpy(m_TabLabels[0], "ALL");

    // Tab 1: "FAV"
    strcpy(m_TabLabels[1], "FAV");

    // Tab 5: "Mega CD"
    strcpy(m_TabLabels[5], "Mega CD");

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

    // Helper to get letter index (0 to 26)
    auto get_letter_idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 1; // 1 to 26
        return 0; // '#' or others
    };

    // Count games per letter
    int letter_counts[27] = {0};
    for (int i = 0; i < m_GenesisCount; i++) {
        char c = get_char(i);
        int idx = get_letter_idx(c);
        if (idx >= 0 && idx < 27) {
            letter_counts[idx]++;
        }
    }

    // Optimize k1 and k2 to divide games as equally as possible
    m_TabSplitK1 = 8;  // Default: #-H
    m_TabSplitK2 = 16; // Default: I-P, Q-Z
    int min_diff = 1000000;
    
    if (m_GenesisCount > 0) {
        for (int k1 = 0; k1 < 25; k1++) {
            for (int k2 = k1 + 1; k2 < 26; k2++) {
                int size0 = 0;
                for (int i = 0; i <= k1; i++) size0 += letter_counts[i];
                
                int size1 = 0;
                for (int i = k1 + 1; i <= k2; i++) size1 += letter_counts[i];
                
                int size2 = 0;
                for (int i = k2 + 1; i < 27; i++) size2 += letter_counts[i];
                
                int ideal = m_GenesisCount / 3;
                int d0 = size0 - ideal; if (d0 < 0) d0 = -d0;
                int d1 = size1 - ideal; if (d1 < 0) d1 = -d1;
                int d2 = size2 - ideal; if (d2 < 0) d2 = -d2;
                int diff = d0 + d1 + d2;
                
                if (diff < min_diff) {
                    min_diff = diff;
                    m_TabSplitK1 = k1;
                    m_TabSplitK2 = k2;
                }
            }
        }
    }

    // Now generate labels based on the actual games present in each split
    // Split 0 (Tab 1): letters <= m_TabSplitK1
    int start0 = -1, end0 = -1;
    for (int i = 0; i < m_GenesisCount; i++) {
        char c = get_char(i);
        int idx = get_letter_idx(c);
        if (idx <= m_TabSplitK1) {
            if (start0 == -1) start0 = i;
            end0 = i;
        }
    }
    if (start0 != -1 && end0 != -1) {
        char c_start = get_char(start0);
        char c_end = get_char(end0);
        if (c_start == c_end) {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[2], "A-G");
    }

    // Split 1 (Tab 3): letters > m_TabSplitK1 && <= m_TabSplitK2
    int start1 = -1, end1 = -1;
    for (int i = 0; i < m_GenesisCount; i++) {
        char c = get_char(i);
        int idx = get_letter_idx(c);
        if (idx > m_TabSplitK1 && idx <= m_TabSplitK2) {
            if (start1 == -1) start1 = i;
            end1 = i;
        }
    }
    if (start1 != -1 && end1 != -1) {
        char c_start = get_char(start1);
        char c_end = get_char(end1);
        if (c_start == c_end) {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[3], "H-P");
    }

    // Split 2 (Tab 4): letters > m_TabSplitK2
    int start2 = -1, end2 = -1;
    for (int i = 0; i < m_GenesisCount; i++) {
        char c = get_char(i);
        int idx = get_letter_idx(c);
        if (idx > m_TabSplitK2) {
            if (start2 == -1) start2 = i;
            end2 = i;
        }
    }
    if (start2 != -1 && end2 != -1) {
        char c_start = get_char(start2);
        char c_end = get_char(end2);
        if (c_start == c_end) {
            snprintf(m_TabLabels[4], sizeof(m_TabLabels[4]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[4], sizeof(m_TabLabels[4]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[4], "Q-Z");
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
    else if (m_ActiveTab == 1) {
        // FAV tab: include only favorited roms
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomFavorites[i]) {
                m_FilteredIndices[m_FilteredCount++] = i;
            }
        }
    }
    else if (m_ActiveTab >= 2 && m_ActiveTab <= 4) {
        // Alphabetical Genesis tabs
        if (m_GenesisCount > 0) {
            auto get_char = [this](int genesis_idx) -> char {
                if (genesis_idx < 0 || genesis_idx >= m_GenesisCount) return '?';
                const char *name = m_RomFiles[m_GenesisIndices[genesis_idx]];
                if (name == nullptr || *name == '\0') return '?';
                char c = *name;
                if (c >= 'a' && c <= 'z') c -= 32;
                if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
                return '#';
            };

            auto get_letter_idx = [](char c) -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A' + 1; // 1 to 26
                return 0; // '#' or others
            };

            int part = m_ActiveTab - 2;
            for (int i = 0; i < m_GenesisCount; i++) {
                char c = get_char(i);
                int idx = get_letter_idx(c);
                
                if (part == 0) {
                    if (idx <= m_TabSplitK1) {
                        m_FilteredIndices[m_FilteredCount++] = m_GenesisIndices[i];
                    }
                } else if (part == 1) {
                    if (idx > m_TabSplitK1 && idx <= m_TabSplitK2) {
                        m_FilteredIndices[m_FilteredCount++] = m_GenesisIndices[i];
                    }
                } else if (part == 2) {
                    if (idx > m_TabSplitK2) {
                        m_FilteredIndices[m_FilteredCount++] = m_GenesisIndices[i];
                    }
                }
            }
        }
    }
    else if (m_ActiveTab == 5) {
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
    for (int t = 0; t < 6; t++) {
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
        const char *prefix = m_RomFavorites[orig_idx] ? "* " : "  ";
        if (size_kb >= 1024) {
            snprintf(g_SharedState.menu_lines[i], 80, "%s%s (%u MB)", prefix, temp, size_kb / 1024);
        } else {
            snprintf(g_SharedState.menu_lines[i], 80, "%s%s (%u KB)", prefix, temp, size_kb);
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
        m_ActiveTab = 5;
    }
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::MoveRight() {
    if (m_ActiveTab < 5) {
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

void COSDMenu::LoadFavorites() {
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFavorites[i] = FALSE;
    }

    FIL file;
    FRESULT res = f_open(&file, "SD:/roms/favorites.txt", FA_READ);
    if (res != FR_OK) {
        return;
    }

    char line[128];
    while (f_gets(line, sizeof(line), &file) != nullptr) {
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }
        if (len == 0) continue;

        for (int i = 0; i < m_RomCount; i++) {
            if (strcmp(m_RomFiles[i], line) == 0) {
                m_RomFavorites[i] = TRUE;
                break;
            }
        }
    }
    f_close(&file);
}

void COSDMenu::SaveFavorites() {
    FIL file;
    FRESULT res = f_open(&file, "SD:/roms/favorites.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        CLogger::Get()->Write("OSD", LogError, "Failed to open favorites.txt for writing: %d", res);
        return;
    }

    for (int i = 0; i < m_RomCount; i++) {
        if (m_RomFavorites[i]) {
            char line[160];
            snprintf(line, sizeof(line), "%s\n", m_RomFiles[i]);
            UINT written = 0;
            f_write(&file, line, strlen(line), &written);
        }
    }
    f_close(&file);
}

void COSDMenu::FavoriteCurrent() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return;
    }
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = TRUE;
    SaveFavorites();
    Update();
}

void COSDMenu::UnfavoriteCurrent() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return;
    }
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = FALSE;
    SaveFavorites();
    Update();
}
