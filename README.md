# MEGA-PI: Bare-Metal Sega Mega Drive/Genesis Emulator

![MEGA-PI Logo](res/Mega_Pi_Logo.png)

**MEGA-PI** is a token-optimized, high-performance Sega Mega Drive / Genesis emulator designed to run bare-metal on the Raspberry Pi. By bypassing heavy operating systems like Linux, it achieves near-instant boot times and ultra-low input latency. 

It leverages the **Picodrive** emulation core and runs on top of the **Circle** bare-metal C++ framework, distributing workload across the Raspberry Pi's four ARM cores.

---

## 🚀 Key Features

*   **Multi-core Architecture**:
    *   **Core 0 (Orchestrator)**: Emulates the Sega Mega Drive/Mega CD hardware, clocks the Sub-CPU (S68K) synchronously, and handles emulation events.
    *   **Core 1 (Video)**: Handles 2x integer nearest-neighbor upscaling (yielding clean 640x480 outputs) and OSD menu rendering.
    *   **Core 2 (Audio)**: Outputs 44.1 kHz stereo audio via the Pi's PWM sound engine.
    *   **Core 3 (Input)**: Dedicated USB plug-and-play thread for gamepad and keyboard inputs.
*   **Sega CD / Mega CD Support**:
    *   **High-Quality Formats**: Supports CD images in both standard `.cue` (with separate `.bin` tracks) and compressed `.chd` formats (compressed via `chdman`).
    *   **Accurate Subsystem Emulation**: Emulates the Sub-CPU, RF5C164 PCM audio chip, CDDA (Redbook) digital audio streaming, and the hardware rotation/scaling graphics coprocessor.
    *   **Region-Free BIOS Handling**: Automatically detects the CD region from the image and loads the corresponding BIOS file (`bios_CD_E.bin`, `bios_CD_U.bin`, `bios_CD_J.bin`).
*   **Scrolling OSD Menu**: Browse up to 1000 ROMs with a viewport-scrolling window and ROM counter. The OSD features a clean forest green container with high-contrast grayscale elements, and lists ROMs cleanly without extensions, displaying their size (e.g., `Sonic (512 KB)` or `Lords of Thunder (351 MB)`).
*   **Save & Load States**: Supports standard emulator save states mapped directly to the SD card.
*   **Input Masking**: In-game actions are ignored while holding control hotkey combinations to prevent accidental character movements.
*   **ROM Support**: Scans and loads `.bin`, `.md`, `.gen` files for Genesis/Mega Drive, and `.cue`, `.chd` files for Sega CD / Mega CD.

---

## 🎮 Controls & Shortcuts

The emulator supports both USB gamepads and USB keyboards out-of-the-box.

### Gamepad Configuration
During gameplay, use the following shortcuts to trigger emulator hotkeys:

| Action | Control Shortcut |
| :--- | :--- |
| **Move Up / Down** (Menu) | D-pad Up / Down |
| **Boot Selected ROM** (Menu) | Button A / Start |
| **Save State** (Slot 0) | **SELECT + D-pad Left** |
| **Load State** (Slot 0) | **SELECT + D-pad Right** |
| **Exit to Menu** | **START + SELECT** |

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

To compile the project, you need the GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`).

### 1. Configure and Build the Circle Framework
Circle must be configured for the correct Raspberry Pi hardware model and compiled first.

1. Navigate to the [circle/](file:///home/loula/src/mega-pi/circle/) directory:
   ```bash
   cd circle
   ```

2. Run the [configure](file:///home/loula/src/mega-pi/circle/configure) script with multi-core support enabled. 
   
   * For **Raspberry Pi 3** (AArch32, outputs `kernel8-32.img`):
     ```bash
     ./configure -r 3 --multicore
     ```
   * For **Raspberry Pi 4** (AArch32, outputs `kernel7l.img`):
     ```bash
     ./configure -r 4 --multicore
     ```

   *(Note: Add the `-f` flag to overwrite an existing configuration if needed)*

3. Compile the Circle framework libraries:
   ```bash
   ./makeall
   ```

### 2. Build the Emulator
Once Circle is compiled, you can build the emulator executable.

1. Navigate to the [emulator/](file:///home/loula/src/mega-pi/emulator/) directory:
   ```bash
   cd ../emulator
   ```

2. Compile the project:
   ```bash
   make
   ```
   
   Depending on the configured target, this will output a bootable image (e.g., `kernel8-32.img` for Raspberry Pi 3, or `kernel7l.img` for Raspberry Pi 4).

---

## ⚙️ How to Run on a Raspberry Pi

1. Format an SD card as **FAT32**.
2. Copy all files and directories (including the `overlays` directory) from the [emulator/boot/](file:///home/loula/src/mega-pi/emulator/boot/) directory (including firmware files like `bootcode.bin`, `start.elf`, `start4.elf`, `fixup.dat`, Device Tree `.dtb` files, `config.txt`, and `cmdline.txt`) to the root of the SD card.
3. Copy your compiled kernel image (`kernel8-32.img` for RPi 3 or `kernel7l.img` for RPi 4) from the [emulator/](file:///home/loula/src/mega-pi/emulator/) directory to the root of the SD card.
4. Create a folder named `roms` on the root of the SD card, and place your Sega Genesis ROMs (`.bin`, `.md`, `.gen`) and Sega CD games (`.cue` + `.bin` tracks, or compressed `.chd` files) inside it.
5. **Sega CD BIOS Setup**: Create a folder named `bios` on the root of the SD card and copy the official Sega CD BIOS files. They must be named exactly as follows depending on the region:
   * **US Region**: `bios_CD_U.bin`
   * **EU Region**: `bios_CD_E.bin`
   * **JP Region**: `bios_CD_J.bin`
6. Plug in a USB Gamepad and/or Keyboard, insert the SD card, and power on the Pi.
