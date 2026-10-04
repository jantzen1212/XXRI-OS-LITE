# XXRI OS — Technical Documentation

## 🧱 Overview

XXRI OS is a Linux derivative built on top of Tiny Core Linux.

Tiny Core provides the lightweight base system, kernel, package format, and a number of low-level components. XXRI OS builds on that foundation by replacing, extending, combining from other distro, and adding components to create a complete desktop-oriented operating system with a modern graphical interface.

Components that already work well and fit the lightweight design are retained, while components that needed different behavior, hardware support, or user experience are modified or replaced.

---

# 🏗️ Base System

XXRI OS makes several changes to the base system for modern usage 

The provided item:

- Core system utilities
- Lightweight boot and initialization components
- Tiny Core libraries and system packages
- The `/bin`, `/sbin`, `/lib` style filesystem layout
- Tiny Core extension loading and dependency mechanisms

XXRI migrate to:

✏️ Writable System

this is One of the major changes made by XXRI is that the installed system uses a writable root filesystem instead of using Tiny Core's traditional immutable/loop-mounted core workflow.
This allows XXRI to behave more like a conventional installed Linux system:
System files can be modified normally
Applications can write persistent configuration
System configuration can be changed without rebuilding the base image
Updates can modify installed system files directly
The system is easier to maintain as a complete desktop OS
The goal is to keep the low resource usage of Tiny Core while providing a conventional and maintainable desktop experience..

---

# 🖥️ Desktop Environment

The default Tiny Core desktop experience has been heavily customized for XXRI OS.

XXRI adds its own:

- Desktop layout
- Application launcher
- Dock
- Control Center
- Window styling
- System dialogs
- Power menu
- Settings application
- Wallpapers
- Icons
- Themes
- Translucent interface elements
- Window animations
- Keyboard shortcuts
- Desktop integration

The desktop is designed to be usable without requiring the user to understand Tiny Core's underlying command-line-oriented workflow.

---

# 🪟 Window Manager and Compositor

XXRI retains a lightweight window-management, not heavy D.E.

The window manager is based on a modified version of FLWM with XXRI-specific changes.

XXRI also includes a lightweight compositor responsible for features such as:

- Window transparency
- Translucent UI surfaces
- Window shadows
- Window animations
- Desktop visual effects

The compositor is intentionally kept lightweight so that these effects can still be used on older hardware.

---

# 🎨 User Interface

One of the largest differences is the GUI.

XXRI used a unified visual system across the operating system.

This includes:

- GTK3-based applications
- Consistent icons
- Rounded dialogs and controls
- Light and dark themes
- Blue accent colors
- Translucent surfaces
- Custom window controls
- Shared application styling
- Custom wallpapers
- Custom system dialogs

The interface is designed around the idea that a lightweight operating system does not have to look like an old minimal Linux desktop.

---

# ⚙️ XXRI Settings

XXRI replaces the need to configure many parts of the system through individual Tiny Core utilities.

The Settings application provides a centralized graphical interface for system configuration.

Current areas include:

- Display
- Resolution
- Refresh rate
- Brightness
- Network
- Wi-Fi
- Bluetooth
- Appearance
- Dark mode
- System information
- Power and session controls
- Other system settings

The Settings application communicates with XXRI's backend utilities instead of directly putting all system logic into the GUI.

---

# 🔄 XXRI System Update

One of the major additions in XXRI OS is its built-in system update system, designed to make updating the operating system feel more like updating a modern desktop or mobile OS.

Instead of requiring users to download and reinstall a complete ISO for every update, XXRI can deliver small incremental update packages containing only the files that changed.

✨ How it works

1. Check for updates
XXRI checks the official update at /update.


2. Download the update
Only the required update package is downloaded.


3. Verify the package
The updater verifies the package using its SHA-256 checksum before installation.


4. Stage the update
Files are prepared safely before being activated.


5. Update the system
Normal system files are replaced while protected files such as the kernel and core.gz are handled separately.


6. Reboot when required
Kernel and boot-related updates can be activated during reboot.


🛡️ Designed for safer updates

The updater includes protections against:

Corrupted downloads

Incorrect checksums

Tampered update packages

Malicious archive paths

Unsafe symbolic links

Interrupted update operations


This allows XXRI to move away from the traditional "download a new ISO and reinstall" workflow. 

An incremental update can work more like:

XXRI OS 2.6
     ↓
Check update manifest
     ↓
Download changed files
     ↓
Verify SHA-256
     ↓
Stage update
     ↓
Reboot if required
     ↓
XXRI OS 2.6.x

The system is designed so that future updates can remain relatively small while still being capable of updating core system components, applications, and even the kernel.


# 🖥️ Display System

XXRI doesn't use Tiny Core's original Xvesa-based display.

The current display stack uses **XLibre**, an Xorg-compatible X server.

XXRI supports:

- XLibre modesetting
- XLibre Xfbdev
- VESA fallback on supported BIOS systems
- RandR-based resolution management
- Automatic display mode detection
- Manual resolution selection
- Refresh-rate detection
- Framebuffer fallback

The system selects an appropriate display path depending on the available hardware.

XLibre was chosen to provide a more modern display stack while keeping compatibility with low-resource hardware.

---

# 🚀 Boot System

XXRI has moved away from the older tce bootloader configuration.

The current boot system uses **GNU GRUB 2**.

The boot configuration supports:

- BIOS/Legacy boot
- MBR boot
- GPT systems where supported by firmware
- 32-bit UEFI / IA32 UEFI
- USB boot
- Optical-media boot
- Virtual-machine boot

The current Lite architecture uses an **i686 / 32-bit Linux kernel**, therefore standard 64-bit UEFI boot is not supported by the current Lite kernel.

The planned x86-64 Lite generation will remove this limitation.

---

# 📦 Package System

XXRI adds its own package-management layer and graphical Store using .appimage cuz tce-load is very very outdated.

The XXRI Store provides a user-friendly UI for:

- Browsing applications
- Installing packages
- Removing packages
- Viewing application information
- Managing installed software

The goal is to make software installation possible without requiring users to manually work with Tiny Core extension commands.
with 🛍️ XXRI Store, The Store is an XXRI application store that uses appimage support from xxri-repo 

---

# 📁 XXRI File

XXRI File is based on a source-level fork of **PCManFM**.

It has been rebuilt and customized for the XXRI desktop.

Changes include:

- XXRI visual styling
- Custom window chrome
- Translucent interface elements
- Custom Places sidebar
- XXRI branding
- Desktop integration
- Cleaner presentation of system directories
- Integration with the XXRI theme

The underlying file-management functionality remains based on the mature PCManFM codebase instead of being rewritten from scratch.

---

# 🌐 Networking

XXRI adds its own networking layer around the underlying Linux networking tools.

`xxri-network` provides a consistent backend for the graphical networking interface.

The system supports:

- Wi-Fi scanning
- Wi-Fi connection management
- Saved networks
- WPA authentication
- DHCP
- Reconnection
- Forgetting networks
- RF-kill handling
- UTF-8 network names
- Network status information

The GUI communicates with the backend rather than implementing networking directly.

---

# 📶 Bluetooth

XXRI includes its own Bluetooth management utility, `xxri-bluetooth`.

It provides a simple interface to the underlying BlueZ stack.

Supported operations include:

- Bluetooth power control
- Device scanning
- Pairing
- Passkey/PIN handling
- Trusting devices
- Connecting and disconnecting devices
- Removing devices
- Renaming devices
- RF-kill handling

BlueZ remains the underlying Bluetooth implementation.

---

# 🔧 Hardware and Firmware

XXRI includes additional hardware firmware compared with the minimal Tiny Core base.

The Lite edition includes selected firmware for:

### Intel

Support is included for a range of Intel graphics generations and newer low-power Intel platforms.

### AMD

Selected Radeon and AMDGPU firmware is included

### NVIDIA

Nouveau support and selected firmware are included for older NVIDIA hardware, particularly generations around the Maxwell era.

Newer GPUs may require firmware or platform features outside the scope of the current 32-bit Lite release.

---

# 📡 Wireless Firmware

XXRI also includes a selected wireless firmware set covering a range of common Wi-Fi hardware.

The distribution includes:

- Wireless firmware
- Wireless regulatory database
- Wi-Fi utilities
- Bluetooth firmware where applicable

The firmware selection is curated to balance hardware compatibility against ISO size.

Some hardware, especially certain proprietary Broadcom devices, may require firmware that cannot be redistributed with the operating system.

---

# 🧠 CPU Microcode

XXRI includes CPU microcode support for supported Intel and AMD processors.

Microcode is loaded during early boot when applicable.

This provides processor-level fixes and compatibility improvements without requiring the user to manually install microcode packages after boot.

---

# 🧩 XXRI Applications

XXRI includes a collection of custom applications and system utilities.

These include components such as:

- XXRI Settings
- XXRI Store
- XXRI File
- XXRI Network
- XXRI Bluetooth
- XXRI Display
- XXRI Power
- XXRI Update
- Control Center
- Launcher
- Media tools
- System utilities
- Installer

Some are original XXRI applications, while others are modified forks of existing open-source projects.

---

# 💿 Installer

XXRI includes a graphical installer designed specifically for the distribution.

The installer provides:

- Graphical disk selection
- Installation configuration
- Partitioning workflow
- Bootloader installation
- Progress information
- Error feedback
- BIOS/UEFI installation support where supported
- Installation to internal and removable storage

The installer integrates with the XXRI desktop instead of exposing the user directly to the underlying Tiny Core installation process.

---

# 🛠️ Build System

The original XXRI development process used a custom Tiny Core-style build system based around a prepared root filesystem and ISO assembly scripts.

The production build system has migrated to **PTXdist**.

PTXdist is now responsible for:

- Toolchain management
- Package builds
- Dependency management
- Root filesystem assembly
- Reproducible package inputs
- Firmware packaging
- Extension packaging
- Kernel integration
- Image generation
- ISO generation

The Tiny Core base remains part of the resulting system, It is the build and integration system used to assemble the distribution in a reproducible way.

---
