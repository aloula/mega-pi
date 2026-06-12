# MEGA-PI: Bare-Metal Sega Mega Drive/Genesis Emulator

**MEGA-PI** is a token-optimized, high-performance Sega Mega Drive / Genesis emulator designed to run bare-metal on the Raspberry Pi. By bypassing heavy operating systems like Linux, it achieves near-instant boot times and ultra-low input latency. 

It leverages the **Picodrive** emulation core and runs on top of the **Circle** bare-metal C++ framework, distributing workload across the Raspberry Pi's four ARM cores.

---

## 🚀 Key Features

*   **Multi-core Architecture**:
    *   **Core 0 (Orchestrator)**: Emulates the Mega Drive hardware and synchronizes sub-systems.
    *   **Core 1 (Video)**: Handles 2x integer nearest-neighbor upscaling (yielding clean 640x480 outputs) and OSD menu rendering.
    *   **Core 2 (Audio)**: Outputs 44.1 kHz stereo audio via the Pi's PWM sound engine.
    *   **Core 3 (Input)**: Dedicated USB plug-and-play thread for gamepad and keyboard inputs.
*   **Scrolling OSD Menu**: Browse up to 100 ROMs with a viewport-scrolling window and ROM counter.
*   **Save & Load States**: Supports standard emulator save states mapped directly to the SD card.
*   **Input Masking**: In-game actions are ignored while holding control hotkey combinations to prevent accidental character movements.
*   **ROM Support**: Scans and loads `.bin`, `.md`, and `.gen` files.

---

## 🎮 Controls & Shortcuts

The emulator supports both USB gamepads and USB keyboards out-of-the-box.

### Gamepad Configuration
During gameplay, hold **L1 + R1** to trigger emulator hotkeys:

| Action | Control Shortcut |
| :--- | :--- |
| **Move Up / Down** (Menu) | D-pad Up / Down |
| **Boot Selected ROM** (Menu) | Button A / Start |
| **Save State** (Slot 0) | **L1 + R1 + D-pad Right** |
| **Load State** (Slot 0) | **L1 + R1 + D-pad Left** |
| **Exit to Menu** | **L1 + R1 + D-pad Down** |

### Keyboard Configuration
In the menu or during gameplay, use the following keys:

| Sega Mega Drive | Keyboard Key |
| :--- | :--- |
| **D-pad (Up/Down/Left/Right)** | Arrow Keys |
| **Button A** | `Z` |
| **Button B** | `X` |
| **Button C** | `C` |
| **Button X** | `A` |
| **Button Y** | `S` |
| **Button Z** | `D` |
| **Start Button** | `Enter` |
| **Mode Button** | `Space` |
| **Save State** (Slot 0) | **`F5`** |
| **Load State** (Slot 0) | **`F8`** |
| **Exit to Menu** | **`Escape`** |

---

## 📁 Project Structure

*   [emulator/](file:///home/loula/src/mega-pi/emulator/): Bare-metal integration code.
    *   [kernel.cpp](file:///home/loula/src/mega-pi/emulator/kernel.cpp) / [kernel.h](file:///home/loula/src/mega-pi/emulator/kernel.h): Entrypoint, multicore scheduling, rendering, and input handling.
    *   [emu_orchestrator.cpp](file:///home/loula/src/mega-pi/emulator/emu_orchestrator.cpp) / [emu_orchestrator.h](file:///home/loula/src/mega-pi/emulator/emu_orchestrator.h): Picodrive core setup, ROM loading, and save state actions.
    *   [clib_stubs.cpp](file:///home/loula/src/mega-pi/emulator/clib_stubs.cpp): Standard C library stubs and custom file system wrapper mapping standard file functions to Circle's FAT FS.
    *   [osd.cpp](file:///home/loula/src/mega-pi/emulator/osd.cpp) / [osd.h](file:///home/loula/src/mega-pi/emulator/osd.h): SD card ROM scanning and OSD menu state.
    *   [audio_ring_buffer.h](file:///home/loula/src/mega-pi/emulator/audio_ring_buffer.h): Interlocked circular buffer for audio samples between Core 0 and Core 2.
    *   [shared_state.h](file:///home/loula/src/mega-pi/emulator/shared_state.h): Memory shared across the 4 ARM cores.
*   [picodrive/](file:///home/loula/src/mega-pi/picodrive/): Picodrive emulator core.
*   [circle/](file:///home/loula/src/mega-pi/circle/): Circle bare-metal C++ framework.

---

## 🛠️ How to Compile

To compile the project, you need an ARM cross-compiler (`arm-none-eabi-gcc`).

1. Navigate to the `emulator` directory:
   ```bash
   cd emulator
   ```
2. Build the project:
   ```bash
   make
   ```
   This will output the bootable image `kernel8-32.img`.

---

## ⚙️ How to Run on a Raspberry Pi

1. Format an SD card as **FAT32**.
2. Copy standard Raspberry Pi bootloader files (`bootcode.bin`, `fixup.dat`, `start.elf`, `config.txt`) to the SD card.
3. Copy the compiled `kernel8-32.img` to the SD card.
4. Put your Sega Genesis ROMs (`.bin`, `.md`, `.gen`) directly in the root directory of the SD card.
5. Plug in a USB Gamepad and/or Keyboard, insert the SD card, and power on the Pi.
