#!/usr/bin/env python3
"""28x28 MNIST — FC 784-32-10 학습 + 대칭 int8 양자화 (8x8 mnist 방식 확장)

출력:
  weights28.h      : NPU 커널/로더용 양자화 가중치 (바이트 패킹)
  demo28.h         : 데모용 샘플 10장 (0~9 각 1장)
  t10k_x.bin/y.bin : 보드 전수 평가용 테스트셋 (양자화된 입력)
  expected.json    : CI 검증용 기대 정확도
DTCM 바이트 오프셋: IN 0x0000 / W1 0x0320 / B1 0x6520 / W2 0x65A0 /
                    B2 0x66E0 / OUT 0x6708 / PRED 0x6730 / DONE 0x6734
"""
import numpy as np, gzip, struct, json, sys, os

D = sys.argv[1] if len(sys.argv) > 1 else "/tmp/m28/mnist"

def idx(fn):
    with gzip.open(os.path.join(D, fn), "rb") as f:
        data = f.read()
    n_dims = data[3]
    shape = struct.unpack(">" + "I"*n_dims, data[4:4+4*n_dims])
    return np.frombuffer(data, np.uint8, offset=4+4*n_dims).reshape(shape)

Xtr = idx("train-images-idx3-ubyte.gz").reshape(-1, 784).astype(np.float32)/255.0
Ytr = idx("train-labels-idx1-ubyte.gz").astype(np.int64)
Xte = idx("t10k-images-idx3-ubyte.gz").reshape(-1, 784).astype(np.float32)/255.0
Yte = idx("t10k-labels-idx1-ubyte.gz").astype(np.int64)
print(f"train {Xtr.shape}  test {Xte.shape}")

# ---- 학습 (numpy, Adam) ----
rng = np.random.default_rng(0)
W1 = rng.normal(0, np.sqrt(2/784), (784, 32)).astype(np.float32); b1 = np.zeros(32, np.float32)
W2 = rng.normal(0, np.sqrt(2/32),  (32, 10)).astype(np.float32);  b2 = np.zeros(10, np.float32)
mw = [np.zeros_like(p) for p in (W1, b1, W2, b2)]
vw = [np.zeros_like(p) for p in (W1, b1, W2, b2)]
lr, B, EP = 1e-3, 128, 12
t = 0
for ep in range(EP):
    per = rng.permutation(len(Xtr))
    for s in range(0, len(Xtr), B):
        idxs = per[s:s+B]; x, y = Xtr[idxs], Ytr[idxs]
        h_pre = x @ W1 + b1; h = np.maximum(h_pre, 0)
        z = h @ W2 + b2
        z -= z.max(1, keepdims=True); e = np.exp(z); p = e/e.sum(1, keepdims=True)
        g = p; g[np.arange(len(y)), y] -= 1; g /= len(y)
        gW2 = h.T @ g; gb2 = g.sum(0)
        gh = g @ W2.T; gh[h_pre <= 0] = 0
        gW1 = x.T @ gh; gb1 = gh.sum(0)
        t += 1
        for i, (p_, g_) in enumerate(zip((W1, b1, W2, b2), (gW1, gb1, gW2, gb2))):
            mw[i] = 0.9*mw[i] + 0.1*g_
            vw[i] = 0.999*vw[i] + 0.001*g_*g_
            p_ -= lr * (mw[i]/(1-0.9**t)) / (np.sqrt(vw[i]/(1-0.999**t)) + 1e-8)
    acc = ((np.maximum(Xte@W1+b1, 0)@W2+b2).argmax(1) == Yte).mean()
    print(f"epoch {ep+1:2d}  test acc {acc:.4f}")

facc = float(acc)

# ---- 양자화 (대칭 int8, 8x8 mnist와 동일 방식) ----
s_x  = 1.0/127.0
s_w1 = float(np.abs(W1).max())/127.0
W1q  = np.clip(np.round(W1/s_w1), -127, 127).astype(np.int8)
b1q  = np.round(b1/(s_x*s_w1)).astype(np.int64)
h_tr = np.maximum(Xtr[:10000]@W1+b1, 0)
s_h  = float(h_tr.max())/127.0
M1   = int(round((s_x*s_w1/s_h)*(1 << 20)))
s_w2 = float(np.abs(W2).max())/127.0
W2q  = np.clip(np.round(W2/s_w2), -127, 127).astype(np.int8)
b2q  = np.round(b2/(s_h*s_w2)).astype(np.int64)

def int_infer(xq):                      # 커널과 비트 단위 동일한 정수 시뮬
    a = xq.astype(np.int64) @ W1q.astype(np.int64) + b1q
    a = np.maximum(a, 0)
    hq = np.clip((a*M1) >> 20, 0, 127)
    z  = hq @ W2q.astype(np.int64) + b2q
    return z.argmax(1), z

Xte_q = np.round(Xte*127).astype(np.int64)
pred, _ = int_infer(Xte_q)
iacc = float((pred == Yte).mean())
print(f"\nfloat32 acc {facc:.4f}  ->  int8 acc {iacc:.4f}   (M1={M1})")

# ---- 산출물 ----
out = os.path.dirname(os.path.abspath(__file__))
# 데모 10장: 0~9 첫 등장
demo_i = [int(np.argmax(Yte == d)) for d in range(10)]
demo_x = Xte_q[demo_i].astype(np.uint8)
demo_pred = [int(p) for p in pred[demo_i]]

def carr(name, typ, vals, per=16):
    s = f"static const {typ} {name}[{len(vals)}] = {{\n"
    for i in range(0, len(vals), per):
        s += "    " + ",".join(str(int(v)) for v in vals[i:i+per]) + ",\n"
    return s + "};\n"

# W1: 뉴런별 행 우선 W1r[i*784+j] (워드 정렬), W2: 출력별 W2r[o*32+m]
W1r = W1q.T.reshape(-1)          # (32,784) -> flat
W2r = W2q.T.reshape(-1)          # (10,32)  -> flat
with open(f"{out}/weights28.h", "w") as f:
    f.write("/* train28.py 자동 생성 — FC 784-32-10 int8 */\n")
    f.write(f"#define M1_28 {M1}\n#define SHIFT_28 20\n")
    f.write(f"/* float acc {facc:.4f} / int8 acc {iacc:.4f} */\n")
    f.write(carr("qW1_28", "signed char", W1r))
    f.write(carr("qb1_28", "int", b1q))
    f.write(carr("qW2_28", "signed char", W2r))
    f.write(carr("qb2_28", "int", b2q))
with open(f"{out}/demo28.h", "w") as f:
    f.write("/* 데모 샘플 10장 (t10k 첫 등장 0~9), 값 0..127 */\n")
    f.write(carr("demo_x28", "unsigned char", demo_x.reshape(-1), per=28))
    f.write(carr("demo_y28", "int", list(range(10))))
    f.write(carr("demo_expect28", "int", demo_pred))
Xte_q.astype(np.uint8).tofile(f"{out}/t10k_x.bin")
Yte.astype(np.uint8).tofile(f"{out}/t10k_y.bin")
json.dump({"float_acc": facc, "int8_acc": iacc, "M1": M1,
           "n_test": len(Yte), "n_correct": int((pred == Yte).sum()),
           "demo_expect": demo_pred},
          open(f"{out}/expected.json", "w"), indent=1)
print("saved: weights28.h demo28.h t10k_x.bin t10k_y.bin expected.json")
