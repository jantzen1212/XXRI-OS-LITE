#!/bin/bash
# wait for the catalog generator, then rebuild all three artifacts
cd /home/jantzen/xxri-build
while pgrep -f gen-catalog.py >/dev/null; do sleep 20; done
echo "=== catalog ready ==="
python3 - <<'PY'
import json
R='rootfs/usr/local/share/xxri-store/repository'
rows=[l.rstrip('\n').split('\t') for l in open(R+'/index.tsv')]
v=[r for r in rows if len(r)>15 and r[15]=='1']
i6=[r for r in v if 'i686' in r[3].split()]
print('apps:',len(rows),'| verified:',len(v),'| i686 installable:',len(i6))
s=json.load(open(R+'/sections.json'))
print('featured:', s['featured'])
PY
chmod u+r rootfs/usr/bin/sudo rootfs/usr/sbin/visudo 2>/dev/null
./build-core.sh >/tmp/claude-1000/bc95.log 2>&1 && echo "core.gz+ISO rebuilt"
./build-writable-image.sh >/tmp/claude-1000/bw95.log 2>&1 && echo "disk image rebuilt"
ls -l output/core.gz output/XXRI-Lite.iso output/xxri-disk.img | awk '{printf "  %-26s %s\n",$9,$5}'
echo FINISH95_DONE
