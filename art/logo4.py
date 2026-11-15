# Title logo (Nov 14 2026): BALLOON drawn like BABE (white letters, black outline, a 1 px shadow) with its O's as solid
# black balloons on strings; BABE outlined underneath. Also the sound screen's words: SOUND outlined to match, ON and OFF bold.
# Writes logo.pkl and sound.pkl for fxpack.py.
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as nd
K=np.ones((3,3),bool)
NOTO='/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc'
POP='/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf'
DJB='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
def glyph(c,cap,font=NOTO,thr=120):
    S=8; f=ImageFont.truetype(font,int(cap*S*1.42))
    im=Image.new('L',(cap*S*3,cap*S*3),0); ImageDraw.Draw(im).text((cap*S//2,0),c,font=f,fill=255)
    a=np.array(im)>127; ys,xs=np.where(a); a=a[ys.min():ys.max()+1,xs.min():xs.max()+1]
    w=max(1,round(a.shape[1]*cap/a.shape[0]))
    return np.array(Image.fromarray((a*255).astype(np.uint8)).resize((w,cap),Image.LANCZOS))>thr
def balloonO(cap,wf=0.92):
    r=cap/2-0.3; yy,xx=np.mgrid[0:cap,0:cap]; cy=(cap-1)/2-0.5; cx=(cap-1)/2
    return ((xx-cx)/(r*wf))**2+((yy-cy)/r)**2<=1
def word(t,cap,font=NOTO,gap=1,bob=None,balloons=True,puff=0):
    gs=[]
    for c in t:
        g=balloonO(cap) if (c=='O' and balloons) else glyph(c,cap,font)
        if puff and not (c=='O' and balloons):
            g=np.pad(g,puff); g=nd.binary_closing(nd.binary_dilation(g,iterations=puff),iterations=1)
            g=nd.binary_opening(g,structure=np.array([[0,1,0],[1,1,1],[0,1,0]]))
            g=np.array(Image.fromarray((g*255).astype(np.uint8)).resize((g.shape[1],cap),Image.LANCZOS))>120
        gs.append((c,g))
    W=sum(g.shape[1] for _,g in gs)+gap*(len(gs)-1); H=cap+9
    body=np.zeros((H,W),bool); knots=[]; x=0
    for i,(c,g) in enumerate(gs):
        dy=1+(bob[i] if bob else 0)
        body[dy:dy+cap,x:x+g.shape[1]]|=g
        if c=='O' and balloons: knots.append((x+g.shape[1]//2,dy+cap))
        x+=g.shape[1]+gap
    return body,knots
def style_outline(B):   # white letters, black outline, 1px shadow (like BABE)
    B=np.pad(B,3); out=nd.binary_dilation(B,K); sh=np.zeros_like(out); sh[1:,1:]=out[:-1,:-1]
    ink=(out&~B)|(sh&~out); return ink, nd.binary_dilation(out|sh,K)
def style_solid(B):     # black letters, white edge, 1px shadow
    B=np.pad(B,3); sh=np.zeros_like(B); sh[1:,1:]=B[:-1,:-1]; ink=B|sh; return ink, nd.binary_dilation(ink,K)
def style_inline(B):    # black letters with a white shine line inside, white edge
    B=np.pad(B,3); er=nd.binary_erosion(B,iterations=1); sh=er&~np.roll(er,1,0)&~np.roll(er,1,1)
    ink=B&~(sh&nd.binary_erosion(B,iterations=2)|False); hl=nd.binary_erosion(B,iterations=1)&~nd.binary_erosion(B,iterations=2)
    top=hl&np.roll(~B,2,0); ink=B&~top; return ink, nd.binary_dilation(B,K)
def add_strings(ink,mask,knots,P=3,white_O=False,n=5):
    for kx,ky in knots:
        kx+=P; ky+=P
        pts=[(ky-1,kx),(ky,kx-1),(ky,kx),(ky,kx+1)]+[(ky+i,kx+(1 if i in (2,3) else 0)-(1 if (i==5 if n==5 else i in (6,7)) else 0)) for i in range(1,n+1)]
        for y,x in pts:
            if 0<=y<ink.shape[0] and 0<=x<ink.shape[1]: ink[y,x]=True
        m2=np.zeros_like(mask)
        for y,x in pts:
            if 0<=y<ink.shape[0] and 0<=x<ink.shape[1]: m2[y,x]=True
        mask|=nd.binary_dilation(m2,K)
    return ink,mask
def balloon_shine(ink,word_body,knots,cap,P=3):
    for kx,ky in knots:
        cx=kx+P; cy=ky+P-cap//2-1; sx,sy=cx-cap//4,cy-cap//4
        ink[sy:sy+2,sx]=False; ink[sy,sx+1]=False
    return ink
def babe(cap=15,st=style_outline,font=NOTO):
    b,_=word('BABE',cap,font,gap=2,balloons=False); return st(b)
def trim(ink,mask):
    ys,xs=np.where(mask); return ink[ys.min():ys.max()+1,xs.min():xs.max()+1],mask[ys.min():ys.max()+1,xs.min():xs.max()+1]
def paste(canvas,ink,mask,x,y):
    h,w=ink.shape
    for j in range(h):
        for i in range(w):
            X,Y=x+i,y+j
            if 0<=X<128 and 0<=Y<64 and mask[j,i]: canvas[Y,X]=0 if ink[j,i] else 255
LIFT = 3                                                      # the balloons float this many pixels above the letters
def compose(t, cap, font, gap, letter_style):
    full, knots = word(t, cap, font, gap, bob=[0 if c == 'O' else LIFT for c in t])
    bal = np.zeros_like(full); x = 0
    gs = [balloonO(cap) if c == 'O' else glyph(c, cap, font) for c in t]
    for c, g in zip(t, gs):
        if c == 'O': bal[1:1 + cap, x:x + g.shape[1]] |= g
        x += g.shape[1] + gap
    # each letter outlined on its own and all the outlines kept, so every letter is closed and a black line shows
    # where one letter ends and the next begins
    li = np.zeros((full.shape[0] + 6, full.shape[1] + 6), bool); lm = li.copy(); x = 0
    for c, g in zip(t, gs):
        if c != 'O':
            one = np.zeros_like(full); one[1 + LIFT:1 + LIFT + cap, x:x + g.shape[1]] = g
            oi, om = letter_style(one); li |= oi; lm |= om          # (every letter's whole outline is kept: lines win over white)
        x += g.shape[1] + gap
    bi, bm = style_solid(bal)
    ink = li.copy(); mask = lm | bm; ink[bm] = bi[bm]
    ink, mask = add_strings(ink, mask, knots, n=5 + LIFT)
    for kx, ky in knots:                                      # a shine on each balloon
        cx = kx + 3; top = ky + 3 - cap; sx = cx - cap // 4; sy = top + cap // 4
        ink[sy:sy + 2, sx] = False; ink[sy, sx + 1] = False
    return ink, mask
import pickle
top_i, top_m = trim(*compose('BALLOON', 14, NOTO, 0, style_outline))
b_i, b_m = trim(*babe(15))
W = top_i.shape[1]; H = top_i.shape[0] - 4 + b_i.shape[0]
ink = np.zeros((H, W), bool); mask = np.zeros((H, W), bool)
def put(i, m, x, y):
    h, w = i.shape; mask[y:y + h, x:x + w] |= m; ink[y:y + h, x:x + w] &= ~m; ink[y:y + h, x:x + w] |= i
bx = (W - b_i.shape[1]) // 2
put(b_i, b_m, bx, top_i.shape[0] - 4); put(top_i, top_m, 0, 0)
ink, mask = trim(ink, mask); babe_cx = bx + b_i.shape[1] / 2
pickle.dump([('outline_shadow', 15, ink, mask, babe_cx)], open('logo.pkl', 'wb'))
print('logo', ink.shape, babe_cx)
def plain(t, cap):
    b, _ = word(t, cap, POP, gap=1, balloons=False); ys, xs = np.where(b); b = b[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    return b, nd.binary_dilation(np.pad(b, 1), K)[1:-1, 1:-1] | b
snd = trim(*style_outline(word('SOUND', 14, NOTO, gap=2, balloons=False)[0]))
on, off = plain('ON', 8), plain('OFF', 8)
pickle.dump({'SOUND': snd, 'ON': on, 'OFF': off}, open('sound.pkl', 'wb'))
print('sound', snd[0].shape, on[0].shape, off[0].shape)
Image.fromarray(np.where(ink, 0, np.where(mask, 255, 150)).astype(np.uint8)).resize((W * 4, H * 4), 0).save('logo_preview.png')
