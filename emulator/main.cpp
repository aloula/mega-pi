#include "kernel.h"
#include <circle/startup.h>

int main(void) {
    CKernel* pKernel = new CKernel();
    if (!pKernel) {
        halt();
        return EXIT_HALT;
    }
    if (!pKernel->Initialize()) {
        delete pKernel;
        halt();
        return EXIT_HALT;
    }

    TShutdownMode ShutdownMode = pKernel->Run();

    delete pKernel;

    switch (ShutdownMode) {
        case ShutdownReboot:
            reboot();
            return EXIT_REBOOT;

        case ShutdownHalt:
        default:
            halt();
            return EXIT_HALT;
    }
}
