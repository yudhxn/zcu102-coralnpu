import numpy as np
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
from sklearn.neural_network import MLPClassifier

d = load_digits()                      # 8x8 손글씨, 0~9
X = d.data.astype(np.float32) / 16.0   # 0~16 -> 0~1
y = d.target
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.25, random_state=42, stratify=y)
print("학습 %d개 / 테스트 %d개, 입력 %d차원" % (len(Xtr), len(Xte), X.shape[1]))

clf = MLPClassifier(hidden_layer_sizes=(32,), activation='relu', max_iter=800,
                    random_state=1, alpha=1e-3)
clf.fit(Xtr, ytr)
acc_f = clf.score(Xte, yte)
print("float32 테스트 정확도: %.4f" % acc_f)

W1 = clf.coefs_[0];  b1 = clf.intercepts_[0]    # 64x32
W2 = clf.coefs_[1];  b2 = clf.intercepts_[1]    # 32x10
np.savez('/tmp/model_f32.npz', W1=W1,b1=b1,W2=W2,b2=b2, Xte=Xte, yte=yte, acc=acc_f)
print("구조: 64 -> 32(ReLU) -> 10   파라미터 %d개" % (W1.size+b1.size+W2.size+b2.size))
print("가중치 범위: W1[%.3f,%.3f] W2[%.3f,%.3f]" % (W1.min(),W1.max(),W2.min(),W2.max()))
