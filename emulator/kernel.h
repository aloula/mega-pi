#ifndef KERNEL_H
#define KERNEL_H

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/cputhrottle.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/multicore.h>
#include <circle/usb/usbgamepad.h>
#include <circle/usb/usbkeyboard.h>
#include "shared_state.h"
#include "osd.h"
#include "emu_orchestrator.h"

enum TShutdownMode {
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

class CKernel;

class CEmulatorMultiCore : public CMultiCoreSupport {
public:
    CEmulatorMultiCore(CMemorySystem *pMemorySystem, CKernel *pKernel);
    virtual void Run(unsigned nCore) override;
private:
    CKernel *m_pKernel;
};

class CKernel {
public:
    CKernel(void);
    ~CKernel(void);

    boolean Initialize(void);
    TShutdownMode Run(void);

    // Core loops called by CEmulatorMultiCore
    void RunOrchestrator();
    void RunVideoDomain();
    void RunAudioDomain();
    void RunInputDomain();

    // Handlers
    static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);
    static void GamePadRemovedHandler(CDevice *pDevice, void *pContext);
    static void KeyboardStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]);
    static void KeyboardRemovedHandler(CDevice *pDevice, void *pContext);

private:
    CActLED             m_ActLED;
    CKernelOptions      m_Options;
    CDeviceNameService  m_DeviceNameService;
    CCPUThrottle        m_CPUThrottle;
    CScreenDevice       m_Screen;
    CSerialDevice       m_Serial;
    CExceptionHandler   m_ExceptionHandler;
    CInterruptSystem    m_Interrupt;
    CTimer              m_Timer;
    CLogger             m_Logger;
    CScheduler          m_Scheduler;
    CUSBHCIDevice       m_USBHCI;
    CFATFileSystem      m_FileSystem;

    CEmulatorMultiCore  m_MultiCore;

    // Devices
    CUSBGamePadDevice  *m_pGamePad[2];
    CUSBKeyboardDevice *m_pKeyboard;

    COSDMenu           *m_pOSDMenu;
    CEmuOrchestrator   *m_pEmuOrchestrator;
};

#endif
