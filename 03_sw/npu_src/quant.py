import numpy as np
m = np.load('/tmp/model_f32.npz')
W1,b1,W2,b2 = m['W1'],m['b1'],m['W2'],m['b2']
Xte,yte,acc_f = m['Xte'],m['yte'],float(m['acc'])

# ---- 대칭 양자화: q = round(r/scale), scale = max|r|/127 ----
def qparam(a): return float(np.abs(a).max())/127.0
def q(a,s):    return np.clip(np.round(a/s),-127,127).astype(np.int32)

s_x  = 1.0/127.0                 # 입력 0~1 -> 0~127
s_w1 = qparam(W1); s_w2 = qparam(W2)
qW1 = q(W1,s_w1); qW2 = q(W2,s_w2)
# bias는 누적 스케일(s_x*s_w)로 양자화 -> int32
qb1 = np.round(b1/(s_x*s_w1)).astype(np.int32)

# 은닉층 출력 재양자화: h_int32 * (s_x*s_w1) = 실수 h. 이를 다시 0..127 스케일로.
# s_h 를 데이터로 캘리브레이션(학습셋 대신 테스트 입력 일부 사용은 부적절 -> 가중치/입력 범위로 추정)
# 여기서는 간단히: h 실수 최대치를 추정해 s_h 결정
def relu(v): return np.maximum(v,0)
h_real_max = float(relu(Xte@W1 + b1).max())     # 캘리브레이션
s_h = h_real_max/127.0
qb2 = np.round(b2/(s_h*s_w2)).astype(np.int32)

M1 = (s_x*s_w1)/s_h    # 1층 누적 -> 은닉 int8 로 되돌리는 배율
# 고정소수점화: M1 ≈ mult1 / 2^shift1
shift1 = 20
mult1 = int(round(M1 * (1<<shift1)))
print("양자화 파라미터:")
print("  s_x=%.6g  s_w1=%.6g  s_h=%.6g  s_w2=%.6g" % (s_x,s_w1,s_h,s_w2))
print("  M1=%.6g -> mult1=%d, shift1=%d (오차 %.3e)" % (M1, mult1, shift1, abs(M1-mult1/(1<<shift1))))

qX = np.clip(np.round(Xte/s_x),-127,127).astype(np.int32)

# ---- int8 추론 (정수 연산만) ----
def infer_int(x):
    a1 = x@qW1 + qb1                      # int32 누적
    a1 = np.maximum(a1,0)                 # ReLU
    h  = (a1*mult1) >> shift1             # 재양자화 -> 0..127
    h  = np.clip(h,0,127)
    a2 = h@qW2 + qb2
    return a2

pred_q = infer_int(qX).argmax(1)
acc_q  = float((pred_q==yte).mean())
pred_f = (relu(Xte@W1+b1)@W2+b2).argmax(1)
acc_f2 = float((pred_f==yte).mean())

print("\n[정확도 비교]")
print("  float32 : %.4f" % acc_f2)
print("  int8    : %.4f" % acc_q)
print("  차이    : %+.4f (%d/%d 샘플)" % (acc_q-acc_f2, int((pred_q==yte).sum()), len(yte)))
print("\n[메모리]")
print("  float32 가중치: %d bytes" % ((W1.size+W2.size)*4 + (b1.size+b2.size)*4))
print("  int8    가중치: %d bytes (+bias int32 %d)" % (qW1.size+qW2.size, (qb1.size+qb2.size)*4))

np.savez('/tmp/model_q.npz', qW1=qW1,qb1=qb1,qW2=qW2,qb2=qb2,
         mult1=mult1, shift1=shift1, qX=qX, yte=yte, acc_q=acc_q, acc_f=acc_f2,
         Xte_raw=(Xte*16).astype(np.int32))
