#ifndef EMU_ORCHESTRATOR_H
#define EMU_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CEmuOrchestrator {
public:
    CEmuOrchestrator(FATFS *pFileSystem);
    ~CEmuOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];
};

#endif
