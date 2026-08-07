| Application | Found? | Download URL | Arch | Packaging | Version | Evidence / reason if unavailable |
|---|---|---|---|---|---|---|
| **7-Zip** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/p7zip.tcz` | i686 | tcz | native | Native 32-bit build. Upstream `7-zip.org/a/7z2409-linux-x86.tar.xz` also exists, but the extension integrates with the OS. |
| **AbiWord** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/abiword.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Audacity** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/audacity.tcz` | i686 | tcz | native | Native 32-bit build (5.0 MB). `audacity/audacity` releases are x86_64 AppImage only; there has never been a 32-bit Linux Audacity binary. |
| **Blender** | **YES** | `https://download.blender.org/release/Blender2.79/blender-2.79-linux-glibc219-i686.tar.bz2` | i686 | tar.bz2 | 2.79 | ELF32 EM_386 confirmed; archive root `blender-2.79-linux-glibc219-i686/`. 2.79 is the last 32-bit release. |
| **Bluefish** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/bluefish.tcz` | i686 | tcz | native | Native 32-bit build in the Tiny Core i686 repository. |
| **Double Commander** | **YES** | `https://github.com/doublecmd/doublecmd/releases/download/v1.2.7/doublecmd-1.2.7.gtk2.i386.tar.xz` | i686 | tar.xz | 1.2.7 | ELF32 EM_386 confirmed; archive root `doublecmd/`. Actively maintained i386 builds. |
| **FFmpeg** | **YES** | `https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-i686-static.tar.xz` | i686 | tar.xz | release | Officially recommended static build; ELF32 EM_386 confirmed (18.0 MB). |
| **Geany** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/geany.tcz` | i686 | tcz | native | Native 32-bit build in the Tiny Core i686 repository. |
| **GIMP** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gimp.tcz` | i686 | tcz | native | Native 32-bit build (21.5 MB). `download.gimp.org/pub/gimp/v2.10/linux/` and `aferrero2707/gimp-appimage` are x86_64-only. |
| **Gnumeric** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gnumeric.tcz` | i686 | tcz | native | Native 32-bit build. |
| **GParted** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gparted.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Inkscape** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/inkscape.tcz` | i686 | tcz | native | Native 32-bit build (11.3 MB). Upstream's `inkscape.org/release/inkscape-0.92.4/gnulinux/appimage/` is x86_64-only, so the native extension is the correct source. |
| **LibreOffice** | **YES** | `https://downloadarchive.documentfoundation.org/libreoffice/old/5.4.7.2/deb/x86/LibreOffice_5.4.7.2_Linux_x86_deb.tar.gz` | i686 | tar.gz of .deb | 5.4.7.2 | Confirmed by unpacking the nested `libobasis5.4-core_5.4.7.2-2_i386.deb` -> `opt/libreoffice5.4/program/libcuilo.so` is ELF32 EM_386. 6.4.7.2 x86 directory exists but is empty; 5.4.7.2 is the last complete 32-bit release. |
| **LibreSprite** | **YES** | — | i686 | AppImage | latest | i686 AppImage confirmed by ELF header over HTTP Range (72.2 MB). |
| **Mozilla Firefox** | **YES** | `https://ftp.mozilla.org/pub/firefox/releases/115.14.0esr/linux-i686/en-US/firefox-115.14.0esr.tar.bz2` | i686 | tar.bz2 | 115.14.0esr | ELF32 EM_386 confirmed. `download.mozilla.org/?product=firefox-latest-ssl&os=linux` returns 404 because current Firefox has no 32-bit Linux target; 115 ESR is the final supported line. |
| **Mozilla Thunderbird** | **YES** | `https://ftp.mozilla.org/pub/thunderbird/releases/115.14.0/linux-i686/en-US/thunderbird-115.14.0.tar.bz2` | i686 | tar.bz2 | 115.14.0 | ELF32 EM_386 confirmed. |
| **mtPaint** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/mtpaint.tcz` | i686 | tcz | native | Native 32-bit build. |
| **QEMU** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/qemu.tcz` | i686 | tcz | native | Native 32-bit build (15.1 MB). |
| **Remmina** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/remmina.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Standard Notes** | **YES** | — | i686 | AppImage | 3.22.11 | i386 AppImage, ELF32 EM_386 confirmed. Already in the catalog. |
| **Telegram Desktop** | **YES** | `https://telegram.org/dl/desktop/linux32` | i686 | tar.xz | linux32 | ELF32 EM_386 confirmed (35.9 MB). Officially maintained 32-bit build. **Installed and launched successfully.** |
| **Tor Browser** | **YES** | `https://archive.torproject.org/tor-package-archive/torbrowser/10.5.10/tor-browser-linux32-10.5.10_en-US.tar.xz` | i686 | tar.xz | 10.5.10 | ELF32 EM_386 confirmed; archive root `tor-browser_en-US/`. 10.5.x is the last 32-bit Linux series. |
| **Visual Studio Code** | **YES** | `https://update.code.visualstudio.com/1.35.1/linux-ia32/stable` | i686 | tar.gz | 1.35.1 | Confirmed: archive root `VSCode-linux-ia32/`, first ELF member is EM_386. Last 32-bit Linux release Microsoft shipped. |
| **VLC media player** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/vlc.tcz` | i686 | tcz | native | Native 32-bit build (6.6 MB). `download.videolan.org/pub/videolan/vlc/last/` ships source + x86_64 only. |
| **Ardour** | no | — | - | - | - | `community.ardour.org/download` offers x86_64 only; 32-bit demo builds were retired at Ardour 6. |
| **Avidemux** | no | — | - | - | - | SourceForge 2.7.6 directory + `mean00/avidemux2` releases: x86_64 AppImage only. |
| **balenaEtcher** | no | — | - | - | - | `balena-io/etcher`: Electron, x64 only since v1.5. |
| **BleachBit** | no | — | - | - | - | `bleachbit/bleachbit` + `bleachbit.org/download/linux`: distro packages are `all`/amd64; no standalone 32-bit binary. |
| **Brave Browser** | no | — | - | - | - | `brave/brave-browser`: Chromium-based, x86_64/arm64 only. |
| **CherryTree** | no | — | - | - | - | `giuspen/cherrytree`: Linux assets are x86_64 AppImage only. |
| **Chromium** | no | — | - | - | - | `commondatastorage.googleapis.com/chromium-browser-snapshots/` has no `Linux_i686` prefix - Google removed 32-bit Linux Chromium builds in 2016 (M48 was last). |
| **Discord** | no | `https://discord.com/api/download?platform=linux&format=tar.gz` | x86_64 | tar.gz | - | The only Linux tarball is x86_64 (confirmed from the ELF header inside). Discord has never shipped 32-bit Linux. |
| **Element** | no | — | - | - | - | `element-hq/element-desktop`: Electron, x86_64 only. |
| **FileZilla** | no | `https://dl2.cdn.filezilla-project.org/client/FileZilla_3.46.3_i686-linux-gnu.tar.bz2` | - | - | - | All three mirrors (`dl1.cdn`, `dl2.cdn`, `download.`) answer **301/307 -> https://filezilla-project.org/** - the i686 objects have been purged. The pass-1 'HTTP 200' was the homepage after redirect; a plain GET returns 0 bytes. |
| **FreeCAD** | no | — | - | - | - | `FreeCAD/FreeCAD`: x86_64 AppImage only. |
| **HandBrake** | no | — | - | - | - | `HandBrake/HandBrake`: Linux assets are x86_64 flatpak/AppImage only. |
| **HexChat** | no | — | - | - | - | Not in the Tiny Core 16.x i686 repository; upstream ships source only. |
| **Joplin** | no | — | - | - | - | `laurent22/joplin`: Electron, x86_64 AppImage only. |
| **Kate** | no | — | - | - | - | `cdn.kde.org/ci-builds/utilities/kate/master/linux/` publishes x86_64 AppImages only. |
| **Kdenlive** | no | — | - | - | - | `download.kde.org/stable/kdenlive/` publishes x86_64 AppImages only. |
| **KeePassXC** | no | — | - | - | - | `keepassxreboot/keepassxc`: x86_64 AppImage only since 2.4. |
| **Krita** | no | `https://download.kde.org/stable/krita/3.3.3/krita-3.3.3-x86.zip` | Windows i386 | zip | 3.3.3 | The `x86` zip on download.kde.org is the **Windows** build - it contains `krita-3.3.3-x86/bin/audio/qtaudio_windows.dll`. KDE has never published a 32-bit Linux Krita AppImage. |
| **LMMS** | no | — | - | - | - | `LMMS/lmms`: x86_64 AppImage only. |
| **mpv** | no | — | - | - | - | `probonopd/mpv-AppImage` publishes x86_64 only; no 32-bit tag in the archive. |
| **MuseScore** | no | — | - | - | - | `musescore/MuseScore`: x86_64 AppImage only since MuseScore 3. |
| **Notepadqq** | no | — | - | - | - | No 32-bit asset across the 6 most recent releases of `notepadqq/notepadqq`. |
| **OBS Studio** | no | — | - | - | - | `obsproject/obs-studio`: no 32-bit Linux asset; OBS requires OpenGL 3.3 and is x86_64-only on Linux. |
| **ONLYOFFICE Desktop Editors** | no | `https://github.com/ONLYOFFICE/DesktopEditors/releases/download/v9.1.0/DesktopEditors_x86.zip` | Windows i386 | zip | 9.1.0 | The only asset named 'x86' is a **Windows** build - the zip's first member is `app.ico` and the tree is `editors/sdkjs-plugins/...` with `.dll` files, no ELF. `download.onlyoffice.com/install/desktop/editors/linux/onlyoffice-desktopeditors_i386.deb` -> HTTP 404. ONLYOFFICE has published x86_64-only Linux builds since v4. |
| **Opera** | no | — | - | - | - | `get.geo.opera.com/pub/opera/desktop/` current tree lists amd64 only; 32-bit dropped at Opera 48. |
| **Pale Moon** | no | `https://rm-eu.palemoon.org/release/palemoon-33.4.0.linux-i686-gtk3.tar.xz` | - | - | - | Both 33.4.0 and 32.5.2 i686 URLs return HTTP 404 - the release mirror no longer carries the linux-i686 objects. |
| **PeaZip** | no | — | - | - | - | `peazip/PeaZip`: Linux packages are x86_64 only (32-bit remains Windows-only). |
| **Pidgin** | no | — | - | - | - | Not published as `pidgin.tcz` in the Tiny Core 16.x i686 repository. |
| **Pinta** | no | — | - | - | - | No 32-bit asset across the 6 most recent `PintaProject/Pinta` releases (.NET x64 only). |
| **Shotcut** | no | — | - | - | - | `mltframework/shotcut`: no 32-bit asset in the 6 most recent releases. |
| **Skype** | no | `https://go.skype.com/skypeforlinux-32.deb` | - | - | - | The URL resolves but serves a 0.1 MB stub, not a package (a real skypeforlinux deb is ~100 MB) - the '-32' route is retired. Skype for Linux has been x86_64-only since 2016, and the desktop client was discontinued in May 2025. |
| **VirtualBox** | no | — | - | - | - | `download.virtualbox.org/virtualbox/`: Linux hosts are amd64 only since VirtualBox 6.0 (2018). |
| **VSCodium** | no | — | - | - | - | Swept `github.com/VSCodium/vscodium/releases` + the 6 most recent tags via `/releases/expanded_assets/<tag>`: zero assets matching i386/i686/ia32/32bit. VSCodium follows upstream Code, which dropped ia32 after 1.35. |
| **Xournal++** | no | — | - | - | - | `xournalpp/xournalpp`: no 32-bit asset in the 6 most recent releases. |
| **Zoom** | no | `https://zoom.us/client/latest/zoom_i686.tar.xz` | - | - | - | HTTP 403 on the i686 path (and on `zoom.us/download` scraping). Zoom's Linux client has required x86_64 since 2019 - the i686 path is a dead route, not an access problem. |

**24 of 59 priority applications have a genuine 32-bit Linux build.**
