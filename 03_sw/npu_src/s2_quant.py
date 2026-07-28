import numpy as np
m=np.load('/tmp/s2_f32.npz')
W1,b1,W2,b2 = m['W1'],m['b'],m['W2'],m['b2']     # W1:(8,9) b1:(8) W2:(72,10)
Xte,yte,acc_f = m['Xte'],m['yte'],float(m['acc'])
C,K,OH = int(m['C']),int(m['K']),int(m['OH'])    # 8,3,3
def relu(v): return np.maximum(v,0)
def patches(x,K,st):
    N=x.shape[0]; O=(8-K)//st+1
    out=np.empty((N,O*O,K*K),np.float32); t=0
    for i in range(0,8-K+1,st):
        for j in range(0,8-K+1,st):
            out[:,t]=x[:,i:i+K,j:j+K].reshape(N,-1); t+=1
    return out
def fwd_f(x):
    p=patches(x,K,2); a=relu(p@W1.T+b1)
    return a.reshape(len(x),-1)@W2+b2

s_x=1.0/127.0
s_w1=float(np.abs(W1).max())/127.0
s_w2=float(np.abs(W2).max())/127.0
q=lambda a,s: np.clip(np.round(a/s),-127,127).astype(np.int32)
qW1,qW2=q(W1,s_w1),q(W2,s_w2)
qb1=np.round(b1/(s_x*s_w1)).astype(np.int32)
h_max=float(relu(patches(Xte,K,2)@W1.T+b1).max()); s_h=h_max/127.0
qb2=np.round(b2/(s_h*s_w2)).astype(np.int32)
M1=(s_x*s_w1)/s_h; SHIFT=20; MULT=int(round(M1*(1<<SHIFT)))
print("MULT=%d SHIFT=%d (M1=%.6g)"%(MULT,SHIFT,M1))
qX=np.clip(np.round(Xte/s_x),-127,127).astype(np.int32)

def fwd_i(x):
    N=x.shape[0]; O=OH
    pt=np.empty((N,O*O,K*K),np.int64); t=0
    for i in range(0,8-K+1,2):
        for j in range(0,8-K+1,2):
            pt[:,t]=x[:,i:i+K,j:j+K].reshape(N,-1); t+=1
    a=pt@qW1.T+qb1
    a=np.maximum(a,0); a=(a*MULT)>>SHIFT; a=np.clip(a,0,127)
    return a.reshape(N,-1)@qW2+qb2
acc_q=float((fwd_i(qX).argmax(1)==yte).mean())
acc_f2=float((fwd_f(Xte).argmax(1)==yte).mean())
print("정확도: float32 %.4f -> int8 %.4f (차이 %+.4f)"%(acc_f2,acc_q,acc_q-acc_f2))
wf=(W1.size+W2.size)*4+(b1.size+b2.size)*4; wq=(qW1.size+qW2.size)+(qb1.size+qb2.size)*4
print("가중치: float32 %dB -> int8 %dB"%(wf,wq))
np.savez('/tmp/s2_q.npz',qW1=qW1,qb1=qb1,qW2=qW2,qb2=qb2,MULT=MULT,SHIFT=SHIFT,
         qX=qX,yte=yte,acc_q=acc_q,acc_f=acc_f2,Xraw=(Xte*16).astype(np.int32))
