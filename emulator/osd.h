#ifndef OSD_H
#define OSD_H

#include <ff.h>
#include "shared_state.h"

class COSDMenu {
public:
    COSDMenu(FATFS *pFileSystem);
    ~COSDMenu();

    boolean Initialize();
    void Update();

    // Input handlers (called from orchestrator)
    void MoveUp();
    void MoveDown();
    void MoveLeft();
    void MoveRight();
    
    const char *GetSelectedRom();
    unsigned GetSelectedRomSize();

private:
    void ScanRoms();
    void BuildFilteredList();
    void CalculateTabLabels();

private:
    FATFS *m_pFileSystem;
    char m_RomFiles[MAX_ROMS][128];
    unsigned m_RomSizes[MAX_ROMS];
    int m_RomCount;
    int m_SelectedIndex;

    // Filtered lists for tabs
    int m_GenesisIndices[MAX_ROMS];
    int m_GenesisCount;
    int m_MegaCDIndices[MAX_ROMS];
    int m_MegaCDCount;

    int m_FilteredIndices[MAX_ROMS];
    int m_FilteredCount;

    int m_ActiveTab; // 0 = ALL, 1 = Split 1, 2 = Split 2, 3 = Split 3, 4 = Mega CD
    char m_TabLabels[5][16];
};

#endif
