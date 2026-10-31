import numpy as np, sys
from PIL import Image
from scipy import ndimage as nd
sys.argv=['x']; exec(open('logo2.py').read())
K=np.ones((3,3),bool)
def solidword(t,cap,gap):
    gs=[glyph(c,cap) for c in t]; W=sum(g.shape[1] for g in gs)+gap*(len(gs)-1)
    b=np.zeros((cap,W),bool); x=0
    for g in gs: b[:,x:x+g.shape[1]]|=g; x+=g.shape[1]+gap
    return b
def pad(b,P=3):
    B=np.zeros((b.shape[0]+2*P,b.shape[1]+2*P),bool); B[P:-P,P:-P]=b; return B
def style(v,cap):
    B=pad(solidword('BABE',cap,2))
    if v=='outline_shadow':
        out=nd.binary_dilation(B,K); sh=np.zeros_like(out); sh[1:,1:]=out[:-1,:-1]
        ink=(out&~B)|(sh&~out); mask=nd.binary_dilation(out|sh,K)
    elif v=='solid_inline':   # black letter, white line, black outer line
        o1=nd.binary_dilation(B,K); o2=nd.binary_dilation(o1,K)
        ink=B|(o2&~o1); mask=nd.binary_dilation(o2,K)
    elif v=='solid_3d':       # black letter with a hatched 3D extrusion down-right
        ext=B.copy()
        for k in (1,2): s=np.zeros_like(B); s[k:,k:]=B[:-k,:-k]; ext|=s
        ink=B|(ext&~B&((np.indices(B.shape).sum(0)%2)==0)); edge=nd.binary_dilation(ext,K)&~ext; ink|=False
        ink=B|(ext&~B); hl=nd.binary_erosion(B)&~np.roll(nd.binary_erosion(B),1,0)
        ink=ink&~(hl&np.roll(B,2,0)); mask=nd.binary_dilation(ext,K)
    return ink,mask
a_ink,a_mask=render('BALLOON',13)
res=[]
for v in ('outline_shadow','solid_inline','solid_3d'):
    for cap in (14,15):
        b_ink,b_mask=style(v,cap)
        W=max(a_ink.shape[1], b_ink.shape[1]+8); H=a_ink.shape[0]-5+b_ink.shape[0]
        ink=np.zeros((H,W),bool); mask=np.zeros((H,W),bool)
        def put(i,m,x,y):
            h,w=i.shape; mask[y:y+h,x:x+w]|=m; ink[y:y+h,x:x+w]&=~m; ink[y:y+h,x:x+w]|=i
        ax=np.where(a_ink.any(0))[0]; bx=np.where(b_ink.any(0))[0]          # centre BABE's letters under BALLOON's letters
        acx=(W-a_ink.shape[1])+(ax[0]+ax[-1])/2; bcx=(bx[0]+bx[-1]-1)/2       # (-1: ignore the 1 px drop shadow)
        xb=int(round(acx-bcx)); put(b_ink,b_mask,xb,a_ink.shape[0]-5); put(a_ink,a_mask,W-a_ink.shape[1],0)
        x0=np.where(mask.any(0))[0][0]; babe_cx=xb+bcx-x0
        ys,xs=np.where(mask); ink=ink[ys.min():ys.max()+1,xs.min():xs.max()+1]; mask=mask[ys.min():ys.max()+1,xs.min():xs.max()+1]
        res.append((v,cap,ink,mask,babe_cx)); print(v,cap,ink.shape,babe_cx)
W=Image.new('L',(2*100,3*50),128)
for i,(v,c,ink,mask,_) in enumerate(res):
    p=np.full(ink.shape,200,np.uint8); p[mask]=255; p[ink]=0
    W.paste(Image.fromarray(p),((i%2)*100,(i//2)*50))
W.resize((W.width*4,W.height*4),0).save('logo_preview.png')
import pickle; pickle.dump(res,open('logo.pkl','wb'))
