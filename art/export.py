# Writes game/sprites.h: Balloon Babe's 6 poses, her chest in 5 bounce positions, and where the chest goes on each pose.
import numpy as np
import os
ART = os.path.dirname(os.path.abspath(__file__))
a = np.load(os.path.join(ART, 'babe2.npy'))            # [pose][ink, solid]
anc = np.load(os.path.join(ART, 'babe2_anchor.npy'))
tv = np.load(os.path.join(ART, 'babe2_torso.npy'))
sk = np.load(os.path.join(ART, 'babe2_skirt.npy'))
ys, xs = np.nonzero(a[:, 1].any(axis=0)); y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
a = a[:, :, y0:y1, x0:x1]
def plusmask(frames):
    n, _, H, W = frames.shape; Hp = (H + 7) // 8 * 8; out = [W, Hp]
    for f in range(n):
        ink, sol = frames[f, 0], frames[f, 1]
        img = np.zeros((Hp, W), bool); msk = np.zeros((Hp, W), bool); img[:H] = sol & ~ink; msk[:H] = sol
        for p in range(Hp // 8):
            for x in range(W):
                out.append(sum(int(img[p * 8 + k, x]) << k for k in range(8))); out.append(sum(int(msk[p * 8 + k, x]) << k for k in range(8)))
    return out
def arr(name, v): return 'const uint8_t %s[] PROGMEM = {\n' % name + ',\n'.join(', '.join(str(x) for x in v[i:i + 24]) for i in range(0, len(v), 24)) + '\n};\n'
t = '// Balloon Babe: stand, crouch, rise, top, fall, land (drawPlusMask); her chest in 5 bounce positions (-2..+2)\n'
t += 'constexpr uint8_t BABE_W = %d, BABE_H = %d;\n' % (x1 - x0, y1 - y0)
# (the bitmaps themselves are in the FX data: art/fxpack.py)

t += 'const int8_t CHEST_AT[6][2] PROGMEM = { %s };   // where the chest sprite sits on each pose\n' % ', '.join('{%d, %d}' % (x - x0, y - y0) for x, y in anc)
open(os.path.join(ART, '..', 'BalloonBabe', 'sprites.h'), 'w').write(t)
