#include "osd.h"
#include <circle/string.h>
#include <string.h>

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

COSDMenu::COSDMenu(CFATFileSystem *pFileSystem)
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
    TDirentry Direntry;
    TFindCurrentEntry CurrentEntry;
    unsigned nEntry = m_pFileSystem->RootFindFirst(&Direntry, &CurrentEntry);
    while (nEntry != 0 && m_RomCount < MAX_ROMS) {
        if (!(Direntry.nAttributes & FS_ATTRIB_SYSTEM)) {
            // Check extension (case insensitive)
            const char *pDot = strrchr(Direntry.chTitle, '.');
            if (pDot != 0) {
                pDot++;
                if (my_strcasecmp(pDot, "bin") == 0 ||
                    my_strcasecmp(pDot, "md") == 0 ||
                    my_strcasecmp(pDot, "gen") == 0) {
                    strncpy(m_RomFiles[m_RomCount], Direntry.chTitle, 63);
                    m_RomFiles[m_RomCount][63] = '\0';
                    m_RomSizes[m_RomCount] = Direntry.nSize;
                    m_RomCount++;
                }
            }
        }
        nEntry = m_pFileSystem->RootFindNext(&Direntry, &CurrentEntry);
    }
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_RomCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;

    for (int i = 0; i < m_RomCount; i++) {
        strncpy(g_SharedState.menu_lines[i], m_RomFiles[i], 79);
        g_SharedState.menu_lines[i][79] = '\0';
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
