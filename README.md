### 🌿 XXRI OS Lite – Lightweight, Fast, and Minimal

**XXRI OS Lite** is the most lightweight edition of the xxri OS series, designed for low-spec machines, older hardware, or users who prefer a clean and ultra-efficient system. it includes only essential components to deliver speed, simplicity, and stability — without unnecessary bloat.

#### ⚡ Key Features:

* Lightweight ISO (239MB, including a full web browser)
* Fast boot and low memory footprint
* Clean, minimal user interface
* Pre-installed basic tools: file manager, text editor, terminal, and more
* Perfect for reviving old PCs or running in lightweight virtual environments

#### 🖥️ Minimum System Requirements:

* **CPU:** Intel Pentium II / AMD K6-2 or newer (32-bit)
* **RAM:** 256 MB minimum
* **Storage:** 700 MB free disk space

#### 📁 XXRI File

The bundled file manager is **XXRI File** — a source-level fork of PCManFM
1.3.2, rebuilt against GTK3 and reworked to the XXRI design: a translucent
Places rail, the shared `△ □ X` window chrome, the XXRI wordmark and location
pill, and system directories (`/proc`, `/sys`, `/dev`) kept out of ordinary
listings.

Source, build instructions and the full list of changes: [`src/xxri-file/`](src/xxri-file/).

#### 🌐 XXRI Browser

The bundled web browser is **XXRI Browser** — a source-level fork of Qt
WebEngine's Simple Browser, running on Qt WebEngine 5.15.10 / Chromium 87 built
for i686. It replaces the stock menu bar, tool bar and title bar with the XXRI
shell: a translucent sidebar carrying the shared `◀ ■ ●` window chrome, the XXRI
wordmark, shortcut tiles, bookmarks and history, and a slim tool bar with a
centred address pill. HTTPS certificate validation, JavaScript, HTML5 video and
audio all work.

Source, build instructions and the measured layout: [`src/xxri-browser/`](src/xxri-browser/).

**Website:** xxri.flows.best
**Maintained by:** xxri OS Team

---

