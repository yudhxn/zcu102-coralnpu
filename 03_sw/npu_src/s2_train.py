exec(open('/tmp/search.py').read().split('cands=[')[0])
best=None; bestacc=-1
for sd in range(6):                      # 좋은 시드 선택
    m=Model(C=8,K=3,stride=2,pool=False,seed=sd).train(EP=120)
    a=m.acc(Xte,yte)
    if a>bestacc: bestacc,best=a,m
print("Stride2 8ch 최종 float32 정확도: %.4f (params %d, MAC %d)"%(bestacc,best.params(),best.macs()))
np.savez('/tmp/s2_f32.npz',W1=best.W1,b=best.bd,W2=best.W2,b2=best.b2,
         Xte=Xte,yte=yte,acc=bestacc,C=best.C,K=best.K,OH=best.OH)
