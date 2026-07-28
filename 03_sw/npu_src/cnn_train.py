import numpy as np
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
rng = np.random.default_rng(0)

d = load_digits()
X = d.data.astype(np.float32).reshape(-1,8,8)/16.0
y = d.target
Xtr,Xte,ytr,yte = train_test_split(X,y,test_size=0.25,random_state=42,stratify=y)
print("train %d / test %d, input 8x8"%(len(Xtr),len(Xte)))

C, K = 8, 3          # 8채널, 3x3 커널
OH = 8-K+1           # 6x6
PH = OH//2           # pool 후 3x3
FLAT = C*PH*PH       # 72
NC = 10

def conv(x, W, b):           # x:(N,8,8) -> (N,C,6,6)
    N = x.shape[0]
    o = np.zeros((N,C,OH,OH), np.float32)
    for i in range(OH):
        for j in range(OH):
            patch = x[:, i:i+K, j:j+K].reshape(N,-1)      # (N,9)
            o[:,:,i,j] = patch @ W.reshape(C,-1).T + b
    return o
def relu(v): return np.maximum(v,0)
def pool(v):                  # (N,C,6,6) -> (N,C,3,3) maxpool 2x2
    N = v.shape[0]
    return v.reshape(N,C,PH,2,PH,2).max(axis=(3,5))

# 초기화
W1 = (rng.standard_normal((C,K,K))*0.5).astype(np.float32); b1 = np.zeros(C,np.float32)
W2 = (rng.standard_normal((FLAT,NC))*0.2).astype(np.float32); b2 = np.zeros(NC,np.float32)

def forward(x):
    c = conv(x,W1,b1); a = relu(c); p = pool(a)
    f = p.reshape(len(x),-1)
    return c,a,p,f, f@W2+b2
def softmax(z):
    z = z - z.max(1,keepdims=True); e = np.exp(z); return e/e.sum(1,keepdims=True)

lr, EP, BS = 0.08, 60, 32
Y1 = np.eye(NC,dtype=np.float32)[ytr]
for ep in range(EP):
    idx = rng.permutation(len(Xtr))
    for s in range(0,len(idx),BS):
        bi = idx[s:s+BS]; xb, yb = Xtr[bi], Y1[bi]
        c,a,p,f,z = forward(xb)
        g = (softmax(z)-yb)/len(bi)
        gW2 = f.T@g; gb2 = g.sum(0)
        gf = g@W2.T
        gp = gf.reshape(len(bi),C,PH,PH)
        # maxpool backward
        ga = np.zeros_like(a)
        ar = a.reshape(len(bi),C,PH,2,PH,2)
        mx = ar.max(axis=(3,5),keepdims=True)
        mask = (ar==mx)
        gar = mask * gp[:,:,:,None,:,None]
        ga = gar.reshape(a.shape)
        gc = ga*(c>0)
        gW1 = np.zeros_like(W1); gb1 = gc.sum(axis=(0,2,3))
        for i in range(OH):
            for j in range(OH):
                patch = xb[:, i:i+K, j:j+K].reshape(len(bi),-1)
                gW1 += (gc[:,:,i,j].T @ patch).reshape(C,K,K)
        W2 -= lr*gW2; b2 -= lr*gb2; W1 -= lr*gW1; b1 -= lr*gb1
    if (ep+1)%20==0:
        acc = (forward(Xte)[4].argmax(1)==yte).mean()
        print("  epoch %2d  test acc %.4f"%(ep+1,acc))

acc_f = float((forward(Xte)[4].argmax(1)==yte).mean())
print("float32 CNN test accuracy: %.4f"%acc_f)
print("params: conv %d, fc %d, total %d"%(W1.size+b1.size, W2.size+b2.size, W1.size+b1.size+W2.size+b2.size))
np.savez('/tmp/cnn_f32.npz',W1=W1,b1=b1,W2=W2,b2=b2,Xte=Xte,yte=yte,acc=acc_f)
