# XXRI OS Lite system updates

Settings > Software Update installs new XXRI OS Lite releases on an installed
system. The mechanism is intentionally simple. It downloads one payload,
verifies it against a SHA-256 checksum, stages it, and applies it, keeping the
previous kernel and initrd as a fallback. There is no A/B partition scheme.

| Part | Where |
|---|---|
| Backend | `/usr/local/bin/xxri-system-update` (`ptxdist/projectroot`) |
| Front end | Settings > Software Update (`src/xxri-settings`) |
| Publishing tool | `tools/update/mkupdate.sh` |
| Update source | `manifest.json` in the root of the `main` branch of github.com/jantzen1212/XXRI-OS-LITE, read from `https://raw.githubusercontent.com/jantzen1212/XXRI-OS-LITE/main/manifest.json` |
| Override | `MANIFEST_URL=` in `/usr/local/etc/xxri/update.conf` (or `XXRI_UPDATE_MANIFEST` in the environment, for testing) |

## What happens on the device

1. **Check** (as the desktop user, when the page opens or on *Check for
   updates*). The manifest is fetched with GNU wget and the system CA bundle,
   the same client the Store uses; busybox wget has no TLS here. It is newer
   when `version` is numerically greater than `IMAGE_VERSION` in
   `/etc/os-release`.
2. **Install** (*Download and install*). The backend re-runs itself as root
   through `sudo` and writes its progress to `/tmp/xxri-update/state`, which
   the page polls. The steps are:
   * the manifest is read again, and the install is refused unless the
     version is newer, `min_version` is met, the payload URL and a 64-hex
     SHA-256 are present, and the disk has room for about 3× the payload;
   * the payload is downloaded to `/var/cache/xxri-update/<version>/`;
   * the payload is **verified** with `openssl dgst -sha256` (busybox has no
     `sha256sum`). On a mismatch the file is deleted and nothing changes;
   * the payload is **staged**. Every member must be `files/…`, `boot/…`,
     `remove.list` or `UPDATE`. Absolute paths, `..` and writes through an
     archive-made symlink are refused. `boot/vmlinuz` must carry the Linux
     `HdrS` magic and `boot/core.gz` must be gzip;
   * the update is **applied**:
     * `files/…` are laid over `/`, each file copied beside its target and
       renamed into place, so a running program keeps its old copy;
     * `remove.list` deletes files;
     * everything replaced or deleted is first copied to
       `/var/lib/xxri-update/backup/`, and put back if applying fails part
       way;
     * `boot/…` files replace those in `/boot`, the current ones being kept as
       `*.prev`. `/boot/grub/grub.cfg` gets an entry, *XXRI OS Lite (previous
       system)*, that boots `vmlinuz.prev`/`core.gz.prev`;
   * `/etc/os-release` is updated (`IMAGE_VERSION`, `VERSION`,
     `PRETTY_NAME`, `BUILD_ID`), and the update is recorded in
     `/usr/local/etc/xxri/update-history`.
3. **Restart.** The page offers *Restart now* when the manifest asks for a
   `reboot` (or *Restart desktop* for `session`).

A **live session** can check for updates but cannot install them. Its root is
in RAM and the medium is read-only, so the page says so. That test uses the
root filesystem type: installed systems run from ext4.

## manifest.json

The manifest is a flat JSON object (the device reads it with awk):

```json
{
  "schema": 1,
  "product": "xxri-os-lite",
  "channel": "stable",
  "version": "2.6.1",
  "build": "20261015",
  "released": "2026-10-15",
  "title": "XXRI OS Lite 2.6.1",
  "notes": "What changed, one item per line.",
  "min_version": "2.6",
  "payload_url": "https://github.com/jantzen1212/XXRI-OS-LITE/releases/download/v2.6.1/xxri-update-2.6.1.tar.gz",
  "payload_sha256": "<64 lowercase hex>",
  "payload_size": 1234567,
  "restart": "reboot"
}
```

`restart` is `reboot`, `session` or `none`. A manifest whose `version` is not
newer than the device, or that has no payload, means "up to date". Until the
first `manifest.json` is published, devices show *No updates published yet*.

## Publishing an update

```sh
# 1. put what the release changes in a tree
mkdir -p upd/files/usr/local/bin upd/boot
cp …/xxri-settings upd/files/usr/local/bin/        # normal files, as under /
cp …/vmlinuz …/core.gz upd/boot/                   # system files (optional)
# a new kernel needs its modules as upd/files/lib/modules/<release>/…

# 2. build the payload and the manifest
tools/update/mkupdate.sh -v 2.6.1 -t upd -o out \
    -u https://github.com/jantzen1212/XXRI-OS-LITE/releases/download/v2.6.1 \
    -n notes.txt -m 2.6 -r reboot

# 3. publish
#    out/xxri-update-2.6.1.tar.gz -> a GitHub release tagged v2.6.1
#    out/manifest.json            -> commit to main at the repository root
```

The payload is reproducible: it is sorted, root-owned, stamped with
`SOURCE_DATE_EPOCH` and written with `gzip -n`. Publish the payload before the
manifest, because a device that reads the manifest first gets a download error
and simply tries again later.

## Trust model and limits

* The manifest comes over HTTPS from GitHub, and the payload must match its
  SHA-256. A compromised GitHub account or repository can therefore publish a
  malicious update. Payload signatures (for example minisign with a key
  shipped in the image) are the next step if that matters.
* There is no automatic rollback. If the new kernel does not boot, choose
  *XXRI OS Lite (previous system)* in GRUB. If a file update breaks
  something, `/var/lib/xxri-update/backup/` holds what the last update
  replaced.
* Extensions (`/tce/optional`, Tiny Core's mirror) stay with the separate
  *Extension Packages* section and `xxri-updates`.
