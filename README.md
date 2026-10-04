# XXRI OS

### 🖥️ A modern Linux desktop for old computers.

XXRI OS is a lightweight Linux distribution made for old and low-spec PCs.

The idea is simple:

> **Old hardware shouldn't have to feel old.**

XXRI gives older computers a modern, graphical desktop that is easy to use without requiring the terminal for everyday tasks.

---

## ✨ Why XXRI?

Modern Linux distributions can be surprisingly heavy on old hardware.

XXRI takes a different approach:

- 🪶 Lightweight
- 🎨 Modern desktop UI
- 🖱️ GUI-first experience
- 📦 Built-in App Store
- ⚙️ Graphical Settings
- 📶 Wi-Fi & Bluetooth support
- 🖥️ Display and resolution controls
- 🔄 Built-in system updates
- 💾 Designed for old hardware
- 🚀 Fast startup and low resource usage

You don't need a powerful PC just to have a nice desktop.

---

## 🖥️ XXRI OS Lite

The lightweight edition.

Built for older x86 computers with limited RAM and storage.

**Target hardware:**

- Old desktops and laptops
- Pentium-class systems
- Older Intel / AMD / NVIDIA hardware
- Low-RAM machines
- 32-bit x86 systems

# 🎨 The XXRI Desktop

XXRI is designed to feel modern without requiring modern hardware.

The desktop uses:

- Translucent surfaces
- Rounded dialogs
- Simple controls
- Light and dark appearance
- Modern icons
- Smooth window effects
- Automatic display detection

The goal isn't to copy another operating system.

It's to make an old computer feel **clean, modern and pleasant to use**.

---

# 📦 Built-in App Store

Installing software shouldn't require a terminal.

XXRI includes a graphical **App Store** where supported applications can be discovered and installed.

The Store is designed around the same simple workflow as the rest of XXRI.

More applications can be added to the repository over time.

---

# ⚙️ Settings

XXRI includes graphical tools for common system tasks.

You can manage things such as:

- 🖥️ Display resolution
- 🔆 Brightness
- 📶 Wi-Fi
- 🟦 Bluetooth
- 🔊 Audio
- 🔋 Power
- 💾 Storage
- 🎨 Appearance
- 🔄 System updates

The goal is simple:

**If something can be configured normally, you shouldn't have to open a terminal to do it.**

---

# 🌐 XXRI Browser

The bundled web browser is XXRI Browser, a source-level fork of Qt WebEngine's Simple Browser, running on Qt WebEngine 5.15.10 / Chromium 87 built for i686. It replaces the stock menu bar, tool bar and title bar with the XXRI shell: a translucent sidebar carrying the shared ◀ ■ ● window chrome, the XXRI wordmark, shortcut tiles, bookmarks and history, and a slim tool bar with a centred address pill. HTTPS certificate validation, JavaScript, HTML5 video and audio all work.


# 📶 Connectivity

XXRI Lite includes support for a wide range of older wireless hardware.

### Wi-Fi

Supported driver families include hardware using:

- Intel iwlwifi
- Atheros ath5k / ath9k / ath10k
- Broadcom b43 / brcmsmac
- Realtek rtlwifi / rtl8xxxu / rtw88 / rtw89
- Ralink / MediaTek
- Marvell
- ZD1211
- and other hardware supported by the included kernel

### Bluetooth

Bluetooth is powered by BlueZ and can be managed from the graphical Settings interface.

You can:

- Scan for devices
- Pair devices
- Connect / disconnect
- Trust devices
- Remove devices
- Rename devices

Some hardware requires firmware that cannot legally be redistributed. Such devices may require additional firmware from the hardware manufacturer.

---

# 🖥️ Hardware Support

XXRI Lite is specifically designed around older PC hardware.

The project includes support for older:

- Intel graphics
- AMD Radeon graphics
- NVIDIA graphics through Nouveau
- Intel Atom / Celeron platforms
- Older Wi-Fi chipsets
- Older Bluetooth hardware

XXRI Lite is currently a **32-bit x86 system**.

64-bit x86 support is planned for a future Lite release in 2.7.

---

# 🪶 Lightweight by Design

XXRI Lite is built around the idea that a lightweight OS doesn't have to look lightweight.

It is designed to work on machines with:

- Low RAM
- Old CPUs
- Older graphics
- Small storage devices

At the same time, it still provides a complete graphical desktop.

For example, XXRI Lite has been tested with extremely old CPU generations including Pentium II and Pentium III-class systems.

> **Small hardware. Modern experience.**

---

# 🔄 System Updates

XXRI includes a built-in update system.

Instead of downloading a completely new ISO for every small update, system updates can contain only the files that changed.

The updater:

1. Checks for updates
2. Downloads the update
3. Verifies the checksum
4. Stages the update
5. Applies the update
6. Updates boot components when required

This allows XXRI to evolve without forcing users to reinstall the entire operating system.

---

# 💿 Installation

XXRI can be tested directly from USB before installing.

### USB

You can create a bootable USB using tools such as:

- Rufus
- Ventoy
- Other standard ISO-writing tools

XXRI Lite supports:

- BIOS / Legacy
- MBR
- GPT
- 32-bit UEFI / IA32 UEFI

> 64-bit UEFI requires the future x86-64 version of XXRI Lite.

Always back up important files before installing an operating system.

---

# 🧪 Hardware Compatibility

XXRI is tested using both virtual machines and real hardware.

Testing includes:

- BIOS systems
- IA32 UEFI systems
- Low-memory configurations
- Pentium-class CPUs
- Intel graphics
- AMD graphics
- NVIDIA graphics
- Older laptops
- Rufus
- Ventoy
- VirtualBox
- QEMU

Real hardware testing is especially important because there are thousands of different combinations of hardware, BIOS firmware and peripherals.

---

# 🐛 Found a Problem?

We want to know.

When reporting a hardware issue, please include:

- 💻 Computer model
- 🧠 CPU
- 🎮 GPU
- 📶 Wi-Fi / Bluetooth chipset
- 💾 RAM
- 🔧 BIOS or UEFI
- 💿 Rufus / Ventoy / other boot method
- 📸 Screenshot or exact error message

send it to our discord server or email dev.xxri@gmail.com
for technical documentation see the documentation.md
xxri.flows.best