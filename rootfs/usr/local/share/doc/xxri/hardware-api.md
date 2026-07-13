# xxri Hardware Enablement Layer — API reference (api version 1)

The single hardware abstraction of xxri OS Lite. Frontends (Control
Center, Software Center, updates, AI tooling) must call these backends and
never ALSA/ifconfig/mount/etc. directly.

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
xxri-network status|list|scan [IF]|connect SSID [PSK] [IF]|
             connect --profile NAME|dhcp IF|static IF IP/PFX GW [DNS]|
             disconnect [IF]|profiles|forget NAME|reconnect|auto [--json]
```
- `status --json` → `"status":{interfaces:[{interface,type,state,carrier,
  mac,ip,netmask,ssid?,wpa_state?}],gateway,dns}`
- `scan --json` → `"scan":{interface,networks:[{ssid,bssid,signal,freq,security}]}`
- profiles live in /usr/local/etc/xxri/network/NAME.conf; `auto` (boot)
  reconnects every AUTOCONNECT=1 profile; wired DHCP boot path unchanged
- WiFi = wpa_supplicant/wpa_cli (WPA/WPA2) + udhcpc

## xxri-display  (VESA backend; API is driver-agnostic)
```
xxri-display list|current|set-resolution WxH[xBPP] [--apply]|edid|cache-modes [--json]
```
- `list --json` → `"list":{modes:[{mode,width,height,depth,refresh,current}],driver,note}`
- `set-resolution` validates against the BIOS mode list, persists, updates
  the session; `--apply` restarts the desktop session immediately
- `edid --json` → `"edid":{available,reason?}` (false on VESA)
- mode list is cached at boot by `cache-modes` (X and VBE can't co-exist)

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

## xxri-bluetooth  (graceful-fallback backend)
```
xxri-bluetooth status|scan|pair MAC|connect MAC|disconnect MAC [--json]
```
- `status --json` → `"status":{available,adapters,stack_installed,
  rfkill_blocked,reason?,hint?}`
- BlueZ is not shipped (15 MB dep chain); once `xxri-pkg -wi bluez5` is
  installed the same commands drive bluetoothctl unchanged.

## xxri-camera
`xxri-camera list [--json]` — alias of `xxri-hardware camera`.

## Boot / session integration
- `/etc/init.d/xxri-hw-init` (from /opt/bootlocal.sh, backgrounded):
  caches VESA modes, `xxri-audio init`, restores governor/brightness,
  `xxri-network auto`
- `~/.X.d/xxri-hw-session`: `xxri-input apply`, `xxri-storage watch &`
