# System updates

Each directory is one update for Settings > Software Update
(docs/software-update.md). Nothing here is published until the payload is
on a GitHub release and the manifest is in the repository root.

## 2.6.1

From the published 2.6 ISO (release `XXRI-OS-Lite-2.6`, sha256
`02096b4b0c2d3816e9ee4e0a853e2d272d2549887811dceb48ff7f196c65f571`) to the
ISO built from this tree on 2026-10-18.

| File | |
|---|---|
| `xxri-update-2.6.1.tar.gz` | the payload: 82 files, no removals, no `boot/` (kernel and `/init` unchanged) |
| `manifest.json` | i686, needs 2.6, restart: reboot |
| `notes.txt` | the notes Settings shows |
| `files.txt` | every path the update writes |

Made with:

```sh
tools/update/mktree.sh XXRI-Lite-2.6.iso ptxdist/platform-xxri/images/XXRI-Lite-2.6.iso TREE
SOURCE_DATE_EPOCH=$(git log -1 --format=%ct bb49cf1) tools/update/mkupdate.sh \
    -v 2.6.1 -t TREE -o update/2.6.1 -n update/2.6.1/notes.txt -m 2.6 -r reboot -a i686 \
    -u https://github.com/jantzen1212/XXRI-OS-LITE/releases/download/XXRI-OS-Lite-2.6.1
```

The payload is reproducible: the same commands give the same sha256.

Checked: the published 2.6 root, with its own 2.6 `xxri-system-update`,
installed it (the payload served locally): all 82 files identical
afterwards, user configuration untouched, version 2.6.1, backup and history
written. Not checked: a real installed machine, or a reboot into the updated
system.

Existing users do not get what lives in their home: the login screen and
the two-display layout reach accounts created after the update (or users who
replace `~/.profile` and `~/.xsession` with the `/etc/skel` copies).

To publish: create the release `XXRI-OS-Lite-2.6.1` with the payload
attached, then copy `2.6.1/manifest.json` to `manifest.json` at the
repository root on `main`. The payload first: a device that reads the
manifest before the payload exists gets a download error and tries later.
