import numpy as np
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
rng0=np.random.default_rng(0)
d=load_digits(); X=d.data.astype(np.float32).reshape(-1,8,8)/16.0; y=d.target
Xtr,Xte,ytr,yte=train_test_split(X,y,test_size=0.25,random_state=42,stratify=y)
NC=10; Y1=np.eye(NC,dtype=np.float32)[ytr]
def relu(v): return np.maximum(v,0)
def softmax(z):
    z=z-z.max(1,keepdims=True); e=np.exp(z); return e/e.sum(1,keepdims=True)

def patches(x,K,stride):
    """(N,8,8) -> (N, OH*OH, K*K), OH=(8-K)//stride+1"""
    N=x.shape[0]; OH=(8-K)//stride+1
    out=np.empty((N,OH*OH,K*K),np.float32)
    t=0
    for i in range(0,8-K+1,stride):
        for j in range(0,8-K+1,stride):
            out[:,t]=x[:,i:i+K,j:j+K].reshape(N,-1); t+=1
    return out,OH

class Model:
    """구조: [conv(K,stride,C) (+dw/pw 여부)] -> relu -> (pool?) -> fc"""
    def __init__(s,C=8,K=3,stride=1,pool=True,sep=False,hidden=0,seed=0):
        s.C,s.K,s.stride,s.pool,s.sep,s.hidden=C,K,stride,pool,sep,hidden
        r=np.random.default_rng(seed)
        s.OH=(8-K)//stride+1
        s.PH=s.OH//2 if pool else s.OH
        if sep:
            s.Wd=(r.standard_normal((1,K*K))*0.5).astype(np.float32)   # depthwise(입력 1채널)
            s.Wp=(r.standard_normal((1,C))*0.5).astype(np.float32)     # pointwise 1->C
            s.bd=np.zeros(C,np.float32)
        else:
            s.W1=(r.standard_normal((C,K*K))*0.5).astype(np.float32)
            s.bd=np.zeros(C,np.float32)
        F=C*s.PH*s.PH
        if hidden:
            s.Wh=(r.standard_normal((F,hidden))*0.2).astype(np.float32); s.bh=np.zeros(hidden,np.float32)
            s.W2=(r.standard_normal((hidden,NC))*0.2).astype(np.float32)
        else:
            s.W2=(r.standard_normal((F,NC))*0.2).astype(np.float32)
        s.b2=np.zeros(NC,np.float32); s.F=F
    def params(s):
        n=(s.Wd.size+s.Wp.size if s.sep else s.W1.size)+s.bd.size+s.W2.size+s.b2.size
        if s.hidden: n+=s.Wh.size+s.bh.size
        return n
    def macs(s):
        P=s.OH*s.OH
        conv = P*s.K*s.K*(1+s.C) if s.sep else P*s.C*s.K*s.K
        fc = (s.F*s.hidden + s.hidden*NC) if s.hidden else s.F*NC
        return conv+fc
    def fwd(s,x):
        N=len(x); pt,OH=patches(x,s.K,s.stride)          # (N,P,KK)
        if s.sep:
            dw=pt@s.Wd.T                                  # (N,P,1)
            c=dw*s.Wp + s.bd                              # (N,P,C)
        else:
            c=pt@s.W1.T + s.bd                            # (N,P,C)
        a=relu(c)
        am=a.reshape(N,OH,OH,s.C)
        if s.pool:
            p=am.reshape(N,s.PH,2,s.PH,2,s.C).max(axis=(2,4))
        else:
            p=am
        f=p.reshape(N,-1)
        if s.hidden:
            h=relu(f@s.Wh+s.bh); z=h@s.W2+s.b2
            return dict(pt=pt,c=c,a=a,am=am,p=p,f=f,h=h,z=z)
        z=f@s.W2+s.b2
        return dict(pt=pt,c=c,a=a,am=am,p=p,f=f,z=z)
    def train(s,EP=70,lr=0.08,BS=32):
        r=np.random.default_rng(1)
        for ep in range(EP):
            idx=r.permutation(len(Xtr))
            for st in range(0,len(idx),BS):
                bi=idx[st:st+BS]; xb,yb=Xtr[bi],Y1[bi]; N=len(bi)
                o=s.fwd(xb); g=(softmax(o['z'])-yb)/N
                if s.hidden:
                    gW2=o['h'].T@g; gb2=g.sum(0); gh=(g@s.W2.T)*(o['h']>0)
                    gWh=o['f'].T@gh; gbh=gh.sum(0); gf=gh@s.Wh.T
                    s.Wh-=lr*gWh; s.bh-=lr*gbh
                else:
                    gW2=o['f'].T@g; gb2=g.sum(0); gf=g@s.W2.T
                s.W2-=lr*gW2; s.b2-=lr*gb2
                gp=gf.reshape(o['p'].shape)
                if s.pool:
                    ar=o['am'].reshape(N,s.PH,2,s.PH,2,s.C)
                    mx=ar.max(axis=(2,4),keepdims=True)
                    ga=((ar==mx)*gp[:,:,None,:,None,:]).reshape(o['am'].shape)
                else:
                    ga=gp
                gc=ga.reshape(N,-1,s.C)*(o['c']>0)
                s.bd-=lr*gc.sum(axis=(0,1))
                if s.sep:
                    dw=o['pt']@s.Wd.T
                    gWp=(dw*gc).sum(axis=(0,1)).reshape(1,-1)
                    gdw=(gc*s.Wp).sum(2,keepdims=True)
                    gWd=np.einsum('npk,npo->ok',o['pt'],gdw)
                    s.Wp-=lr*gWp; s.Wd-=lr*gWd
                else:
                    gW1=np.einsum('npc,npk->ck',gc,o['pt'])
                    s.W1-=lr*gW1
        return s
    def acc(s,X_,y_): return float((s.fwd(X_)['z'].argmax(1)==y_).mean())

cands=[
 ("FC 64-32-10 (기준)",      None),
 ("CNN 8ch 3x3 +pool",       dict(C=8,K=3,stride=1,pool=True,sep=False)),
 ("Stride2 8ch 3x3",         dict(C=8,K=3,stride=2,pool=False,sep=False)),
 ("DSConv 8ch 3x3 +pool",    dict(C=8,K=3,stride=1,pool=True,sep=True)),
 ("DSConv 8ch stride2",      dict(C=8,K=3,stride=2,pool=False,sep=True)),
 ("DSConv 12ch stride2",     dict(C=12,K=3,stride=2,pool=False,sep=True)),
 ("DSConv 16ch stride2",     dict(C=16,K=3,stride=2,pool=False,sep=True)),
 ("DSConv 12ch st2 +h16",    dict(C=12,K=3,stride=2,pool=False,sep=True,hidden=16)),
]
print("%-24s %8s %8s %8s"%("구조","정확도","파라미터","MAC"))
print("-"*54)
# FC 기준값(이전 실험)
print("%-24s %8.4f %8d %8d"%("FC 64-32-10 (기준)",0.9800,2410,2368))
res=[]
for name,cfg in cands[1:]:
    m=Model(**cfg,seed=0).train()
    a=m.acc(Xte,yte)
    print("%-24s %8.4f %8d %8d"%(name,a,m.params(),m.macs()))
    res.append((name,a,m.params(),m.macs(),cfg))
import pickle; pickle.dump(res,open('/tmp/search.pkl','wb'))
