# xxri Hardware Enablement Layer — API reference (api version 1)

The single hardware abstraction of xxri OS Lite. Frontends (Settings, the
Control Center, the Store, updates) call these backends, never
ALSA/ifconfig/mount/etc. directly.

Conventions:
- every backend supports `--json`; without it, output is human-readable
- every JSON document is one line: `{"api":1,"backend":"<name>", ...}`
- schemas are additive-only: fields are never renamed or removed
- string fields are `""` and arrays `[]` when a fact is unavailable;
  graceful-fallback objects carry `"available":false` + `"reason"`, and a
  `"hint"` whenever the user can fix it (e.g. install a package)
- shared code: `/usr/local/lib/xxri/hw-lib.sh`
- persistent settings: `/usr/local/etc/xxri/*.conf` (plain `KEY=value`)

## xxri-hardware — detection library
```
xxri-hardware summary|cpu|memory|gpu|usb|camera|monitor|printer|firmware|all [--json]
```
- `summary --json` → `"summary":{os,kernel,arch,cpu,cores,memory_mb,gpu,
  sound_cards,network_interfaces,cameras,bluetooth_adapters,battery}`
- `cpu` → `"cpu":{model,cores,mhz,arch}` · `memory` → `{total_kb,free_kb,available_kb}`
- `gpu` → `"gpu":[{name,pci,driver}]` (names via pciutils/hwdata)
- `usb` → `"usb":[{vendor_id,product_id,product,manufacturer,bus}]`
- `camera` → `"camera":{devices:[{device,name}],driver_support,hint}` (V4L2 sysfs)
- `monitor` → `"monitor":{connectors:[...],kms,note}` (EDID only with KMS)
- `printer` → `"printer":{devices:[...],hint}` (USB printer class)
- `firmware` → `"firmware":{loaded:[],missing:[],firmware_dir,hint}`
- `all --json` → one combined document with every section above

## xxri-audio — ALSA
```
xxri-audio list|volume [get|set N|up|down]|mute|unmute|toggle|
           default [get|set CARD]|save|restore|init  [--json] [--card N]
```
- `list --json` → `"cards":[{id,name,type(analog|hdmi|usb),playback,capture,default}]`
- `volume --json` → `"volume":{card,control,volume,muted}`
- `default set N` writes /etc/asound.conf; `save`/`restore` persist the
  mixer (alsactl state in /usr/local/etc/xxri/alsa.state)
- `init` (boot): loads drivers by PCI/USB modalias, auto-selects the first
  analog playback card (saved choice wins), restores or unmutes to 70%

## xxri-network
```
xxri-network status|list|wifi [on|off]|scan [IF]|info [IF]|
             connect SSID [PSK] [IF]|connect --hidden SSID [PSK]|
             connect --profile NAME|dhcp IF|static IF IP/PFX GW [DNS]|
             disconnect [IF]|profiles|forget NAME|SSID|country [CC]|
             reconnect|auto [--json]
```
- `status --json` → `"status":{interfaces:[{interface,type,state,carrier,
  mac,ip,netmask,ssid?,wpa_state?}],gateway,dns}`
- `wifi --json` → `"wifi":{present,enabled,hard_blocked,soft_blocked,
  interface,ssid,country}`; `on`/`off` switch the radio (rfkill) and are kept
  across boots (network.conf WIFI=)
- `scan --json` → `"scan":{interface,networks:[{ssid,bssid,signal,freq,
  security,band,bars,saved,current}]}` - one entry per name (its strongest
  access point), strongest first; security is open|wep|wpa|wpa3|eap
- `info --json` → `"info":{interface,state,connected,ssid,bssid,signal,bars,
  freq,band,link_mbps,security,ip,netmask,gateway,dns,mac,online,saved}`
- `connect --json` → `"connect":{connected,ssid,interface?,ip?,error?,
  reason?}`; error is `auth` (the key was refused) or `notfound`. A saved
  network connects without its password again.
- `country CC` sets the regulatory domain (iw reg set, kept in network.conf)
- profiles live in /usr/local/etc/xxri/network/NAME.conf as KEY='value'
  lines (read with sed, never sourced); `auto` (boot) hands every saved Wi-Fi
  network to wpa_supplicant at once, which joins the one in range; wired
  DHCP boot path unchanged
- Wi-Fi = wpa_supplicant/wpa_cli (WPA/WPA2/WPA3-SAE) + udhcpc (renewing lease)

## xxri-display  (XLibre backend; API is driver-agnostic)
```
xxri-display list|current|set-resolution WxH[xBPP] [--apply]|edid|cache-modes [--json]
```
- `driver` is `kms` (XLibre Xorg + modesetting on a KMS/DRM device) or `fbdev`
  (XLibre Xfbdev on the firmware framebuffer, when there is no KMS device)
- `list --json` → `"list":{modes:[{mode,width,height,depth,refresh,current}],driver,note}`;
  kms: the connector's modes (DRM sysfs before X starts, RandR while it
  runs); fbdev: the single framebuffer mode
- `set-resolution` validates against that list and persists it (applied by
  `~/.xsession` at every session start; an `xres=` boot code wins);
  `--apply` switches the running display through RandR (kms).  On fbdev the
  mode belongs to the boot loader and cannot be changed
- `edid --json` → `"edid":{available,reason?}` (from /sys/class/drm)
- `cache-modes` records the boot-time mode list (run by xxri-hw-init)

## xxri-input
```
xxri-input devices|status|set-repeat DELAY RATE|set-mouse-speed A [T]|
           natural-scroll on|off|layouts|set-layout CODE|apply [--json]
```
- `devices --json` → `"devices":[{name,type(keyboard|mouse|touchpad|other),handlers}]`
- `status --json` → `"status":{repeat_delay_ms,repeat_rate_hz,mouse_accel,
  mouse_threshold,natural_scroll,keyboard_layout,note}`
- settings persist in xxri config and re-apply each session (`apply`)

## xxri-storage
```
xxri-storage list|info DEV|mount DEV|unmount DEV|health DEV|watch [--json]
```
- `list --json` → `"disks":[{device,model,size_mb,usb,removable,
  partitions:[{device,size_mb,fs,label,mountpoint,mounted,used_kb?,free_kb?}]}]`
- `mount` → /media/<label-or-device>; `unmount` syncs first and reports
  busy devices instead of forcing
- `health --json` → `"health":{available,device,status(passed|failed|unsupported)}`
- `watch` (session daemon) automounts new USB partitions

## xxri-power
```
xxri-power info|battery|ac|brightness [set N|up|down]|governor [set G|list]|
           capabilities|suspend|shutdown|restart [--json]
```
- `battery --json` → `"battery":{batteries:[{name,capacity,status,technology}],present}`
- `capabilities --json` → `"capabilities":{suspend,hibernate,hibernate_kernel,
  swap,battery,brightness,cpufreq}`
- governor/brightness persist and restore at boot (xxri-hw-init)

## xxri-bluetooth
```
xxri-bluetooth status|power [on|off]|start|auto|devices|scan [SECONDS]|
               pair MAC|connect MAC|disconnect MAC|trust MAC|remove MAC|
               name [NAME] [--json]
```
- `status --json` → `"status":{available,adapters,stack_installed,
  rfkill_blocked,hard_blocked,running,powered,enabled,switched_off,address,
  name,reason?,hint?}`
- `devices --json` → `"devices":[{mac,name,named,icon,paired,trusted,
  connected,rssi}]` (also as `"paired"`, the 2.6 RC name); `scan N --json`
  → `"scan":[...]` the same objects for unpaired, named devices nearby
- `pair MAC` runs one interactive bluetoothctl with a KeyboardDisplay agent
  and prints, while it runs, `confirm NNNNNN` (answer yes/no on stdin),
  `passkey NNNNNN` or `pin NNNN` (type it on the device); the last line is
  `"pair":{...device...}` or `"pair":{paired:false,error,reason}`. Then it
  trusts and connects the device.
- `power on|off` uses rfkill and bluetoothctl and is kept across boots
  (bluetooth.conf POWER=); `name` sets the name other devices see
- BlueZ 5.72 (bluez5.tcz) ships; `start` brings up the system D-Bus and
  bluetoothd (installed systems never run the extension's install hook)

## xxri-camera
`xxri-camera list [--json]` — alias of `xxri-hardware camera`.

## Boot / session integration
- `/etc/init.d/xxri-hw-init` (from /opt/bootlocal.sh, backgrounded):
  caches VESA modes, `xxri-audio init`, restores governor/brightness,
  `xxri-network auto &` and `xxri-bluetooth auto &` (in the background:
  saved networks out of range must not hold up the rest)
- `~/.X.d/xxri-hw-session`: `xxri-input apply`, `xxri-storage watch &`
