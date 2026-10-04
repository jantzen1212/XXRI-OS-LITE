#!/usr/bin/env python3
"""glass-measure.py DESKTOP.png SHOT.png NAME=X,Y,W,H [NAME=X,Y,W,H ...]

Is a glass surface really translucent?  Measured, not judged by eye.

DESKTOP.png is the session with nothing open, SHOT.png the same session with
the surface on screen.  For each named region the screenshot's pixels are
regressed against the desktop's:  C = alpha*G + (1 - alpha)*D.  The slope is
(1 - alpha): an OPAQUE surface has slope 0 whatever is behind it, a real
translucent one follows the backdrop with the slope its alpha predicts.  The
region should be bare glass (no text or icons) over a backdrop that varies.
Prints alpha, the glass colour G, and the fit's residual; exits 1 if any
region is opaque (alpha > 0.985) or its backdrop does not vary enough to tell.
"""
import subprocess, sys

def region(path, x, y, w, h):
    """The region's pixels as a list of (r, g, b)."""
    raw = subprocess.check_output(['magick', path, '-crop', f'{w}x{h}+{x}+{y}', '+repage',
                                   '-depth', '8', 'rgb:-'])
    return [(raw[i], raw[i + 1], raw[i + 2]) for i in range(0, len(raw) - 2, 3)]

def main():
    desk, shot = sys.argv[1], sys.argv[2]
    bad = 0
    for spec in sys.argv[3:]:
        name, r = spec.split('=')
        x, y, w, h = map(int, r.split(','))
        d, c = region(desk, x, y, w, h), region(shot, x, y, w, h)
        n = len(d)
        dm = [sum(p[k] for p in d) / n for k in range(3)]
        cm = [sum(p[k] for p in c) / n for k in range(3)]
        # one slope for all three channels, an intercept per channel
        var = sum((p[k] - dm[k]) ** 2 for p in d for k in range(3))
        if var / (3 * n) < 40:
            print(f'{name:22s} backdrop too flat to measure (variance {var / (3 * n):.1f})')
            bad += 1
            continue
        cov = sum((d[i][k] - dm[k]) * (c[i][k] - cm[k]) for i in range(n) for k in range(3))
        slope = cov / var
        alpha = 1 - slope
        resid = (sum(((c[i][k] - cm[k]) - slope * (d[i][k] - dm[k])) ** 2
                     for i in range(n) for k in range(3)) / (3 * n)) ** 0.5
        g = [(cm[k] - slope * dm[k]) / alpha if alpha > 0.02 else cm[k] for k in range(3)]
        g = [min(255, max(0, int(round(v)))) for v in g]
        opaque = alpha > 0.985
        bad += opaque
        print(f'{name:22s} alpha {alpha:5.3f}  glass #{g[0]:02X}{g[1]:02X}{g[2]:02X}'
              f'  residual {resid:4.1f}  backdrop sd {(var / (3 * n)) ** 0.5:5.1f}'
              f'  {"OPAQUE" if opaque else "REAL"}')
    sys.exit(1 if bad else 0)

main()
