#ifndef OSD_H
#define OSD_H

#include <circle/fs/fat/fatfs.h>
#include "shared_state.h"

class COSDMenu {
public:
    COSDMenu(CFATFileSystem *pFileSystem);
    ~COSDMenu();

    boolean Initialize();
    void Update();

    // Input handlers (called from orchestrator)
    void MoveUp();
    void MoveDown();
    const char *GetSelectedRom();
    unsigned GetSelectedRomSize();

private:
    void ScanRoms();

private:
    CFATFileSystem *m_pFileSystem;
    char m_RomFiles[MAX_ROMS][64];
    unsigned m_RomSizes[MAX_ROMS];
    int m_RomCount;
    int m_SelectedIndex;
};

#endif
