import numpy as np
m = np.load('/tmp/cnn_f32.npz')
W1,b1,W2,b2 = m['W1'],m['b1'],m['W2'],m['b2']
Xte,yte,acc_f = m['Xte'],m['yte'],float(m['acc'])
C,K,OH,PH,FLAT,NC = 8,3,6,3,72,10

def relu(v): return np.maximum(v,0)
def conv_f(x,W,b):
    N=x.shape[0]; o=np.zeros((N,C,OH,OH),np.float32)
    for i in range(OH):
        for j in range(OH):
            o[:,:,i,j]=x[:,i:i+K,j:j+K].reshape(N,-1)@W.reshape(C,-1).T+b
    return o
def pool_f(v):
    N=v.shape[0]; return v.reshape(N,C,PH,2,PH,2).max(axis=(3,5))

# ---- 양자화 파라미터 ----
s_x  = 1.0/127.0
s_w1 = float(np.abs(W1).max())/127.0
s_w2 = float(np.abs(W2).max())/127.0
q = lambda a,s: np.clip(np.round(a/s),-127,127).astype(np.int32)
qW1, qW2 = q(W1,s_w1), q(W2,s_w2)
qb1 = np.round(b1/(s_x*s_w1)).astype(np.int32)

# 은닉(pool 출력) 스케일 캘리브레이션
h_max = float(pool_f(relu(conv_f(Xte,W1,b1))).max())
s_h = h_max/127.0
qb2 = np.round(b2/(s_h*s_w2)).astype(np.int32)
M1 = (s_x*s_w1)/s_h
SHIFT=20; MULT=int(round(M1*(1<<SHIFT)))
print("s_x=%.6g s_w1=%.6g s_h=%.6g s_w2=%.6g"%(s_x,s_w1,s_h,s_w2))
print("M1=%.6g -> MULT=%d SHIFT=%d"%(M1,MULT,SHIFT))

qX = np.clip(np.round(Xte/s_x),-127,127).astype(np.int32)

def infer_int(x):          # x:(N,8,8) int
    N=x.shape[0]
    acc=np.zeros((N,C,OH,OH),np.int64)
    Wf=qW1.reshape(C,-1)
    for i in range(OH):
        for j in range(OH):
            patch=x[:,i:i+K,j:j+K].reshape(N,-1)
            acc[:,:,i,j]=patch@Wf.T+qb1
    acc=np.maximum(acc,0)
    h=(acc*MULT)>>SHIFT
    h=np.clip(h,0,127)
    p=h.reshape(N,C,PH,2,PH,2).max(axis=(3,5))
    f=p.reshape(N,-1)
    return f@qW2+qb2

pred_q=infer_int(qX).argmax(1)
acc_q=float((pred_q==yte).mean())
pred_f=(pool_f(relu(conv_f(Xte,W1,b1))).reshape(len(Xte),-1)@W2+b2).argmax(1)
acc_f2=float((pred_f==yte).mean())
print("\n[정확도] float32 %.4f  ->  int8 %.4f  (차이 %+.4f)"%(acc_f2,acc_q,acc_q-acc_f2))
wf=(W1.size+W2.size)*4+(b1.size+b2.size)*4
wq=(qW1.size+qW2.size)+(qb1.size+qb2.size)*4
print("[메모리] float32 %dB -> int8 %dB"%(wf,wq))
np.savez('/tmp/cnn_q.npz',qW1=qW1,qb1=qb1,qW2=qW2,qb2=qb2,MULT=MULT,SHIFT=SHIFT,
         qX=qX,yte=yte,acc_q=acc_q,acc_f=acc_f2,Xraw=(Xte*16).astype(np.int32))
