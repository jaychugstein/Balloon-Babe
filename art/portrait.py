# Balloon Babe portrait for the instructions screen: head and chest, about twice the detail of the game sprite.
# Same look as in the game: long dark hair with side-swept bangs, big lashy eyes, full lips, hoop earrings, a white
# top with a scoop neck over a fuller bust. Painted in order at 8x, then reduced to 1 bit, then face details by hand.
import numpy as np
from PIL import Image, ImageDraw
S = 8; W, H = 52, 64; T, K, Wh = 128, 0, 255          # transparent, black, white
img = Image.new('L', (W * S, H * S), T); d = ImageDraw.Draw(img)
Z, PV, HY = 1.12, 36, 3                    # zoomed in a little (about the middle of her face/neck); head 3 px lower (shorter neck)
head = False                                 # (set while drawing head parts: they sit HY lower)
def TX(x): return (x - 26) * Z + 26
def TY(y): return (y + (HY if head else 0) - PV) * Z + PV
def P(x, y): return (TX(x) * S, TY(y) * S)
def E(cx, cy, rx, ry): return [P(cx - rx, cy - ry), P(cx + rx, cy + ry)]
def Ehead(): pass
def line(pts, wd=1.15): d.line([P(*p) for p in pts], fill=K, width=int(wd * S), joint='curve')
cx = 26
# 1. long hair behind her, down past the shoulders
head = True
d.polygon([P(cx - 14, 12), P(cx - 17, 28), P(cx - 18.5, 46), P(cx - 12.5, 48), P(cx - 10, 36), P(cx + 10, 36),
           P(cx + 13, 49), P(cx + 19.5, 47), P(cx + 18, 28), P(cx + 14, 12)], fill=K)
d.ellipse(E(cx, 17, 15.5, 15), fill=K)
head = False
# 2. shoulders and chest: white top, outlined
d.polygon([P(cx - 23, 64), P(cx - 22, 51), P(cx - 17, 45), P(cx - 6, 41), P(cx + 6, 41), P(cx + 17, 45), P(cx + 22, 51), P(cx + 23, 64)], fill=K)
d.polygon([P(cx - 21.8, 64), P(cx - 20.8, 51.6), P(cx - 16.4, 46.2), P(cx - 5.6, 42.2), P(cx + 5.6, 42.2), P(cx + 16.4, 46.2), P(cx + 20.8, 51.6), P(cx + 21.8, 64)], fill=Wh)
# 3. neck
d.polygon([P(cx - 4.2, 29), P(cx + 4.2, 29), P(cx + 5, 42.6), P(cx - 5, 42.6)], fill=K)
d.polygon([P(cx - 3.1, 29), P(cx + 3.1, 29), P(cx + 3.9, 42.6), P(cx - 3.9, 42.6)], fill=Wh)
# 4. face
head = True
d.ellipse(E(cx, 20, 11.2, 13.2), fill=Wh)
d.ellipse(E(cx, 7.5, 12.5, 3.5), fill=K)                                  # hairline
# 5. bangs sweeping across from her right
d.polygon([P(cx - 12.5, 8), P(cx + 6, 5), P(cx + 6.5, 9.5), P(cx + 3, 11), P(cx - 2, 12.8), P(cx - 7, 15.2), P(cx - 11.5, 18.5), P(cx - 12.5, 18.5)], fill=K)   # bangs: parted on her left, sweeping across and down to her right, like the little her
head = False
# 6. her top: thin straps down to a low sweetheart neckline over a full bust: the round tops, deep cleavage, full
#    outer curves and the undersides curving back in at the bottom of the picture
for s_ in (-1, 1): line([(cx + s_ * 10, 42.8), (cx + s_ * 12.5, 46.5)])
for s_ in (-1, 1):
    line([(cx + s_ * 12.5, 46.5), (cx + s_ * 8.5, 45.5), (cx + s_ * 4.5, 47), (cx + s_ * 1.6, 50), (cx + s_ * 0.6, 52.5)])   # round tops
    line([(cx + s_ * 18.5, 46.5), (cx + s_ * 21, 52), (cx + s_ * 20.5, 57), (cx + s_ * 17, 60.5), (cx + s_ * 11, 62.3)])  # full outer curves
    line([(cx + s_ * 2, 60.6), (cx + s_ * 6, 62.3)], 0.9)                                                               # undersides meeting
line([(cx, 51.5), (cx, 60)])                                               # cleavage
a = np.array(img).reshape(H, S, W, S).transpose(0, 2, 1, 3).reshape(H, W, S * S)
nk = (a == K).sum(-1); nw = (a == Wh).sum(-1); nt = (a == T).sum(-1)
sol = nt < 40; m = sol & (nk >= 22)
def px(x, y, v=True): m[y, x] = v; sol[y, x] = True
def R(x, y):                                  # a head point (source units) -> picture pixel
    return int(round(TX(x))), int(round((y + HY - PV) * Z + PV))
fx, fy = R(cx, 20)
for y in range(H):                            # a clean face to draw on
    for x in range(W):
        if y >= R(cx, 16.6)[1] and sol[y, x] and (x - fx) ** 2 / (11.2 * Z) ** 2 + (y - fy) ** 2 / (13.2 * Z) ** 2 < 0.62: m[y, x] = False   # (below the bangs)
# hand-placed face, a little bigger than before: eyes with lashes and catchlights, brows, nose, full smiling lips
EYE = ["..XXXXX.",
       ".XX...XX",
       "XX.XX..X",
       "X.XXXX..",
       "..XXX..."]
for ax, flip in ((-6.2, False), (6.2, True)):
    ex, ey = R(cx + ax, 18.2); ex -= 4
    for j, r in enumerate(EYE):
        r = r[::-1] if flip else r
        for i, c in enumerate(r):
            if c == 'X': px(ex + i, ey + j)
    if not flip: px(ex, ey - 1); px(ex - 1, ey - 2)                      # lash flicks out at the corners
    else: px(ex + 7, ey - 1); px(ex + 8, ey - 2)
    px(ex + (4 if not flip else 3), ey + 2, False)                       # a sparkle in each eye
    bx, by = R(cx + ax, 14.8)
    for i in range(6): px(bx - 3 + i, by - (1 <= i <= 4) + (0 if not flip else 0))   # brows, gently arched
nx, ny = R(cx, 24.2); px(nx, ny); px(nx + 1, ny + 1)                    # nose
LIPS = ["X.........X",                                                   # a big happy smile, full lips
        ".XXXXXXXXX.",
        "..XXXXXXX..",
        "...XXXXX..."]
lx, ly = R(cx, 27.5); lx -= 5
for j, r in enumerate(LIPS):
    for i, c in enumerate(r):
        if c == 'X': px(lx + i, ly + j)
for ax in (-12, 12):                                                     # hoops: white rings on her dark hair
    ex, ey = R(cx + ax, 26)
    for (dx, dy) in ((0, 0), (-1, 1), (-1, 2), (-1, 3), (0, 4), (1, 3), (1, 2), (1, 1)): px(ex + dx, ey + dy, False)
np.save('portrait.npy', np.stack([m, sol]))
prev = np.where(m, 0, np.where(sol, 255, 170)).astype(np.uint8)
Image.fromarray(prev).resize((W * 8, H * 8), Image.NEAREST).save('portrait_preview.png')
