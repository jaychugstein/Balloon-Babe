# Balloon Babe: a confident grown-up with long flowing hair,
# hoop earrings, lashes and lipstick, a fitted tank top with a star, high-waisted shorts and high-top sneakers.
# Adult proportions (head about 1/5 of her height). About 44 px tall. Drawn at 8x then 1 bit.
import numpy as np, math
from PIL import Image, ImageDraw
S = 8
ANCHOR = []
# her top: the bust drawn the way it looks when she's falling (the boldest look), with the shading under it,
# then her waist. 12 rows, for her 2 px taller body.
TORSO_ROWS = ["....XX......XX....",
             "...X.X......X.X...",
             "..X..X......X..X..",
             ".X....X....X....X.",
             "X......X..X......X",
             "X........X.......X",
             "X...X........X...X",
             ".X...XX....XX...X.",
             "..XX..........XX..",
             "..XX..........XX..",
             "...X..........X...",
             "...X..........X..."]
def torso_variant(T, b):                    # bust moved b pixels (+ down / - up); the waist stays put
    T = list(T); chest, mid = T[:8], T[8:]
    for _ in range(abs(b)):
        if b > 0: chest = chest[:5] + [chest[5]] + chest[5:]; mid = mid[1:]          # settle: a row more bust, a row less waist
        else: chest = chest[:5] + chest[6:]; mid = [T[8]] + mid                     # lift: a row less bust, more waist
    return (chest + mid)[:12]
def make(pose, W=40, H=52, bust=0):
    ink = Image.new('L', (W * S, H * S), 0); d = ImageDraw.Draw(ink)
    wht = Image.new('L', (W * S, H * S), 0); w = ImageDraw.Draw(wht)
    sol = Image.new('L', (W * S, H * S), 0); o = ImageDraw.Draw(sol)
    def P(x, y): return (x * S, y * S)
    def both(fn, *a, **k): getattr(d, fn)(*a, **k); getattr(o, fn)(*a, **k)
    def solid(fn, *a, **k): getattr(o, fn)(*a, **k)
    cx = W / 2; feet = H - 1.5
    sq = {'crouch': 5, 'land': 6}.get(pose, 0)
    hip = feet - 15 + sq
    spread = {'rise': 1.6, 'top': 4.5, 'fall': 3, 'land': 6, 'crouch': 4.5}.get(pose, 2.6)
    tuck = {'top': 4, 'fall': 2}.get(pose, 0)
    # legs: long, outlined white (bare legs) with black high-tops
    for s in (-1, 1):
        knee = (cx + s * (spread + (2.4 if sq else 0.6) + tuck * 0.3), hip + (15 - sq) * 0.5 - tuck)
        foot = (cx + s * spread, feet - 4 - tuck * 1.3)
        for (a, b, wd) in (((cx + s * 2.2, hip), knee, 4.2), (knee, foot, 3.6)):
            both('line', [P(*a), P(*b)], fill=255, width=int((wd + 0.5) * S))
            w.line([P(*a), P(*b)], fill=255, width=int((wd - 2.2) * S))
        fx = foot[0] + s * 1.2
        both('rounded_rectangle', [P(fx - 2.6, foot[1] - 1.6), P(fx + 3, foot[1] + 3.4)], radius=1.3 * S, fill=255)    # high-top
        w.rectangle([P(fx - 2.6, foot[1] + 2.2), P(fx + 3, foot[1] + 2.8)], fill=255)                                    # sole
        w.line([P(fx - 1.2, foot[1] - 1), P(fx + 1.4, foot[1] - 1)], fill=255, width=int(0.6 * S))                      # lace
    # (skirt: see SKIRT below)
    # (no shorts: she wears a flared skirt, drawn by the game on top so it can flip with her jumps)
    # torso: fitted white tank top with a star, narrow waist
    top = hip - 13                                   # two rows taller in the body (midriff)
    # tank top over a fuller bust: wider and rounded at the chest, nipped in at the waist
    both('polygon', [P(cx - 6, top), P(cx + 6, top), P(cx + 7.8, top + 3.5), P(cx + 7.6, top + 6.5), P(cx + 4.8, hip - 6), P(cx + 5.4, hip - 3.5),
                     P(cx - 5.4, hip - 3.5), P(cx - 4.8, hip - 6), P(cx - 7.6, top + 6.5), P(cx - 7.8, top + 3.5)], fill=255)
    w.polygon([P(cx - 4.9, top + 1.2), P(cx + 4.9, top + 1.2), P(cx + 6.6, top + 3.8), P(cx + 6.4, top + 6), P(cx + 3.8, hip - 6), P(cx + 4.2, hip - 4.6),
               P(cx - 4.2, hip - 4.6), P(cx - 3.8, hip - 6), P(cx - 6.4, top + 6), P(cx - 6.6, top + 3.8)], fill=255)
    star = (cx, top + 9)
    # arms: slim, outlined white
    arms = {'stand': ((-9.5, top + 11), (9.5, top + 11)), 'crouch': ((-11, top + 8), (11, top + 8)), 'rise': ((-10, top - 9), (10, top - 9)),
            'top': ((-14, top - 2), (14, top - 2)), 'fall': ((-13, top - 8), (13, top - 8)), 'land': ((-15, top + 4), (15, top + 4))}[pose]
    for (ax, ay), s in zip(arms, (-1, 1)):
        sh = (cx + s * 6.4, top + 2.5)
        both('line', [P(*sh), P(cx + ax, ay)], fill=255, width=int(3.2 * S)); w.line([P(*sh), P(cx + ax, ay)], fill=255, width=int(1.2 * S))
        both('ellipse', [P(cx + ax - 1.5, ay - 1.5), P(cx + ax + 1.5, ay + 1.5)], fill=255); w.ellipse([P(cx + ax - 0.6, ay - 0.6), P(cx + ax + 0.6, ay + 0.6)], fill=255)
    # neck and head
    hcy = top - 11.5
    both('rectangle', [P(cx - 1.8, hcy + 7), P(cx + 1.8, top + 0.5)], fill=255); w.rectangle([P(cx - 0.8, hcy + 7), P(cx + 0.8, top)], fill=255)
    # long hair flowing behind her (behind the pixel head), swinging with the pose
    swing = {'rise': [(5, 4), (9, 7), (10, 10)], 'top': [(5, 2), (11, 0), (15, 2)], 'fall': [(5, 0), (9, -6), (9, -11)],
             'land': [(5, 2), (12, 2), (16, 5)], 'crouch': [(5, 4), (9, 6), (10, 9)]}.get(pose, [(5, 4), (8, 7), (9, 10)])   # hair flowing out behind her head
    pts = [(cx + a_, hcy + b_) for a_, b_ in swing]
    both('line', [P(*p) for p in pts], fill=255, width=int(5.4 * S), joint='curve')
    both('ellipse', [P(pts[-1][0] - 2, pts[-1][1] - 2), P(pts[-1][0] + 2, pts[-1][1] + 2)], fill=255)
    w.line([P(*pts[0]), P(*pts[1])], fill=255, width=int(0.6 * S))
    I = np.array(ink).reshape(H, S, W, S).mean(axis=(1, 3)) > 110
    Wt = np.array(wht).reshape(H, S, W, S).mean(axis=(1, 3)) > 110
    So = np.array(sol).reshape(H, S, W, S).mean(axis=(1, 3)) > 110
    m = I & ~Wt
    # star on the tank top
    TORSO = TORSO_ROWS
    T2 = list(TORSO)
    T2 = torso_variant(TORSO, bust)
    tx0 = int(round(cx)) - 9; ty0 = int(round(top)) - 1
    for j, r in enumerate(T2):
        r2 = ' ' * (len(r) - len(r.lstrip('.'))) + r.strip('.') + ' ' * (len(r) - len(r.rstrip('.')))
        for i, c in enumerate(r2):
            if c == 'X': m[ty0 + j, tx0 + i] = True; So[ty0 + j, tx0 + i] = True
            elif c == '.': m[ty0 + j, tx0 + i] = False; So[ty0 + j, tx0 + i] = True
    # the head: hand-placed pixels (14 wide), long hair framing her face, side-swept bangs, big lashy eyes, full lips
    OPEN = [
    "......XXXXXX......",
    "....XXXXXXXXXX....",
    "...XXXXXXXXXXXX...",
    "..XXXXXXXXXXXXXX..",
    ".XXXXXXXXXXXXXXXX.",
    ".XXXXXXXXXX...XXXX",
    "XXXXXXXX.......XXX",
    "XXXX...........XXX",
    "XX.XXX......XXX.XX",
    "XX..XX......XX..XX",
    "XX..X.......X...XX",
    "XX..XX......XX..XX",
    "XX..............XX",
    "XX........X.....XX",
    "XX..............XX",
    "XX......XXXX....XX",
    "XXX.....XXXX...XXX",
    ".XXX.....XX...XXX.",
    ".XXXXX......XXXXX.",
    "..XXXXXX..XXXXXX..",
    "..XXXX......XXXX..",
    ]
    
    SHUT = OPEN[:8] + ["XX..............XX", "XX.XXXX....XXXX.XX", "XX..............XX", "XX..............XX"] + OPEN[12:]
    GRIN = OPEN[:15] + ["XX.....XXXXXX...XX", "XXX....X....X..XXX", ".XXX....XXXX..XXX."] + OPEN[18:]
    HEADS = {'open': OPEN, 'shut': SHUT, 'grin': GRIN}
    hd = HEADS['shut' if pose == 'land' else 'grin' if pose in ('top', 'rise') else 'open']
    x0 = int(round(cx)) - 9; y0 = int(round(hcy)) - 9
    for j, r in enumerate(hd):
        r = r.strip('.').center(len(r)) if False else (' ' * (len(r) - len(r.lstrip('.'))) + r.strip('.') + ' ' * (len(r) - len(r.rstrip('.'))))   # dots outside the hair: see-through
        for i, c in enumerate(r):
            if c == 'X': m[y0 + j, x0 + i] = True; So[y0 + j, x0 + i] = True
            elif c == '.': m[y0 + j, x0 + i] = False; So[y0 + j, x0 + i] = True
    for xx in (x0 + 1, x0 + 16):                                                    # hoop earrings peeking out
        m[y0 + 13, xx] = m[y0 + 14, xx] = False
    ANCHOR.append((tx0, ty0))
    return m, So | m
POSES = ['stand', 'crouch', 'rise', 'top', 'fall', 'land']
fr = [make(p) for p in POSES]
np.save('babe2.npy', np.stack([np.stack(f) for f in fr]))
np.save('babe2_anchor.npy', np.array(ANCHOR[:6]))
# the chest as its own small sprite, in 5 positions (-2..+2), drawn over her body by the game for a springy bounce
TV = []
for b in (-2, -1, 0, 1, 2):
    T = torso_variant(TORSO_ROWS, b)
    T = [' ' * (len(r) - len(r.lstrip('.'))) + r.strip('.') + ' ' * (len(r) - len(r.rstrip('.'))) for r in T]   # outside dots: see-through
    ink = np.array([[c == 'X' for c in r] for r in T]); sol = np.array([[c in 'X.' for c in r] for r in T])
    TV.append(np.stack([ink, sol]))
np.save('babe2_torso.npy', np.stack(TV))
SKIRT = [["....XXXXXXXXXXXX....",
          "...XXXXXXXXXXXXXX...",
          "...XXXXXXXXXXXXXX...",
          "..XXXXXXXXXXXXXXXX..",
          "..XXXXXX.XX.XXXXXX..",
          ".XXXX.XXXXXXXX.XXXX.",
          ".XXXXXXXXXXXXXXXXXX."],
         ["....XXXXXXXXXXXX....",
          "...XXXXXXXXXXXXXX...",
          "..XXXXXXXXXXXXXXXX..",
          ".XXXXXX.XX.XX.XXXXX.",
          "XXXXX.XXXXXXXX.XXXXX",
          "XXX..X.XX..XX.X..XXX",
          "                    "],
         ["....XXXXXXXXXXXX....",
          "..XXXXXXXXXXXXXXXX..",
          "XXXXXXX.XX.XX.XXXXXX",
          "XXX.XX.X..X..X.XX.XX",
          "X..X..X......X..X..X",
          "                    ",
          "                    "]]
SK = []
for rows in SKIRT:
    rows = [' ' * (len(r) - len(r.lstrip('.'))) + r.strip('.') + ' ' * (len(r) - len(r.rstrip('.'))) for r in rows]
    SK.append(np.stack([np.array([[c == 'X' for c in r] for r in rows]), np.array([[c in 'X.' for c in r] for r in rows])]))
np.save('babe2_skirt.npy', np.stack(SK))
row = np.hstack([np.pad(np.where(ink, 0, np.where(sol, 255, 215)).astype(np.uint8), 3, constant_values=150) for ink, sol in fr[:6]])
Image.fromarray(row).resize((row.shape[1] * 6, row.shape[0] * 6), Image.NEAREST).save('babe2_preview.png')
