import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as nd
F2='/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc'
def glyph(c, cap, font=F2):
    S=8; f=ImageFont.truetype(font, int(cap*S*1.42))
    im=Image.new('L',(cap*S*3, cap*S*3),0); ImageDraw.Draw(im).text((cap*S//2,0),c,font=f,fill=255)
    a=np.array(im)>127; ys,xs=np.where(a); a=a[ys.min():ys.max()+1, xs.min():xs.max()+1]
    w=max(1,round(a.shape[1]*cap/a.shape[0]))
    return np.array(Image.fromarray((a*255).astype(np.uint8)).resize((w,cap),Image.LANCZOS))>120
def balloonO(cap):
    r=cap/2-0.3; h=cap; w=cap
    yy,xx=np.mgrid[0:h,0:w]; cy=(h-1)/2-0.5; cx=(w-1)/2
    m=((xx-cx)/(r*0.92))**2+((yy-cy)/r)**2<=1
    return m
def word(t, cap, gap=1, balloons=True):
    gs=[]
    for c in t:
        g=balloonO(cap) if (c=='O' and balloons) else glyph(c,cap)
        gs.append((c,g))
    W=sum(g.shape[1] for _,g in gs)+gap*(len(gs)-1); H=cap+7
    body=np.zeros((H,W),bool); x=0; extra=[]
    for c,g in gs:
        body[0:cap, x:x+g.shape[1]] |= g
        if c=='O' and balloons: extra.append((x+g.shape[1]//2, cap))
        x+=g.shape[1]+gap
    return body, extra
def render(t, cap, gap=1):
    body, knots = word(t, cap, gap)
    H,W=body.shape; P=2
    B=np.zeros((H+2*P,W+2*P),bool); B[P:P+H,P:P+W]=body
    ink=B.copy(); white=np.zeros_like(B)
    # balloon O details: shine and knot + wiggly string
    for (kx,ky) in knots:
        kx+=P; ky+=P
        ink[ky-1,kx]=True; ink[ky,kx-1:kx+2]=True
        for i in range(1,6):
            ink[ky+i, kx+(1 if i in (2,3) else 0) - (1 if i==5 else 0)]=True
        cx=kx; cy=P+cap//2
        sx, sy = cx-cap//4, cy-cap//4
        white[sy:sy+2, sx:sx+1]=True; white[sy, sx+1]=True
    out=nd.binary_dilation(ink, structure=np.ones((3,3)))
    mask=out
    ink=ink & ~white
    return ink, mask
# (helpers for logo3.py, which builds the logo that goes in the FX data)
