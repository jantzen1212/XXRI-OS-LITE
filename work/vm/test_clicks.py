#!/usr/bin/env python3
from PIL import Image, ImageChops
import subprocess, sys, os, time
B='/home/jantzen/xxri-build'
SHOTS=os.path.join(B,'work','vm','shots')
vminput=os.path.join(B,'work','vm-input.py')
vmshot=os.path.join(B,'work','vm-shot.sh')
Q=os.path.join(B,'work','vm','qmp.sock')

def bbox_nonbg(img):
    # convert to RGB, find bbox of non-background by comparing to blurred background
    im = img.convert('RGB')
    # assume background is wallpaper largely; find bounding box of non-uniform area
    gray = im.convert('L')
    bw = gray.point(lambda p: 0 if p>240 else 255) # threshold to pick bright white window
    bbox = bw.getbbox()
    return bbox

before = Image.open(os.path.join(SHOTS,'settings-before.png'))
after = Image.open(os.path.join(SHOTS,'settings-after.png'))
bb_before = bbox_nonbg(before)
bb_after = bbox_nonbg(after)
print('before bbox', bb_before)
print('after bbox', bb_after)
if not bb_before or not bb_after:
    print('Could not find window bbox in one of images; aborting')
    sys.exit(1)

# use top-left corners
tl_before = (bb_before[0], bb_before[1])
tl_after = (bb_after[0], bb_after[1])
dx = tl_after[0]-tl_before[0]
dy = tl_after[1]-tl_before[1]
print('top-left before', tl_before, 'after', tl_after, 'delta', (dx,dy))

# candidate control offsets from top-left (empirical)
controls_offsets = {'close':(30,18),'min':(90,18),'max':(60,18)}

results = []

def click_and_check(x,y,tag):
    # click and take shot
    subprocess.run(['python3', vminput, Q, 'click', str(x), str(y)])
    time.sleep(1.0)
    subprocess.run([vmshot, tag])
    img = Image.open(os.path.join(SHOTS, tag + '.png'))
    # determine if window is gone by checking bbox
    b = bbox_nonbg(img)
    gone = (b is None)
    return gone

# First test: click controls at NEW visual position
print('\nTesting NEW visual control positions')
for name,off in controls_offsets.items():
    x = tl_after[0]+off[0]
    y = tl_after[1]+off[1]
    print(f"Clicking NEW {name} at ({x},{y})")
    gone = click_and_check(x,y, f'click-new-{name}')
    print(' -> closed?', gone)
    results.append(('new',name,x,y,gone))
    if gone:
        # reopen Settings
        subprocess.run(['python3', vminput, Q, 'click', '440', '719'])
        time.sleep(2); subprocess.run([vmshot, 'reopened-after-new'])

# Second test: click controls at OLD visual position
print('\nTesting OLD control positions')
for name,off in controls_offsets.items():
    x = tl_before[0]+off[0]
    y = tl_before[1]+off[1]
    print(f"Clicking OLD {name} at ({x},{y})")
    gone = click_and_check(x,y, f'click-old-{name}')
    print(' -> closed?', gone)
    results.append(('old',name,x,y,gone))
    if gone:
        subprocess.run(['python3', vminput, Q, 'click', '440', '719'])
        time.sleep(2); subprocess.run([vmshot, 'reopened-after-old'])

# save results
out='/home/jantzen/xxri-build/work/vm/control-click-results.txt'
with open(out,'w') as f:
    for r in results:
        f.write('%s %s %d %d closed=%s\n' % r)
print('results written to', out)
