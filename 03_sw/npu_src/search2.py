exec(open('/tmp/search.py').read().split('cands=[')[0])   # 클래스/데이터 재사용
cands=[
 ("CNN 8ch 3x3 +pool",     dict(C=8,K=3,stride=1,pool=True)),
 ("Stride2 8ch 3x3",       dict(C=8,K=3,stride=2,pool=False)),
 ("Stride2 12ch 3x3",      dict(C=12,K=3,stride=2,pool=False)),
 ("Stride2 16ch 3x3",      dict(C=16,K=3,stride=2,pool=False)),
 ("Stride2 12ch 4x4",      dict(C=12,K=4,stride=2,pool=False)),
 ("Stride2 16ch 4x4",      dict(C=16,K=4,stride=2,pool=False)),
 ("Stride3 16ch 3x3",      dict(C=16,K=3,stride=3,pool=False)),
 ("Stride2 12ch 3x3 +h24", dict(C=12,K=3,stride=2,pool=False,hidden=24)),
]
print("%-24s %8s %8s %8s  %s"%("구조","정확도","파라미터","MAC","비고"))
print("-"*66)
print("%-24s %8.4f %8d %8d  %s"%("FC 64-32-10",0.9800,2410,2368,"기준(이전)"))
best=None
for name,cfg in cands:
    accs=[]
    for sd in (0,1,2):                      # 시드 3개 평균 (안정성)
        m=Model(**cfg,seed=sd).train()
        accs.append(m.acc(Xte,yte))
    a=float(np.mean(accs)); sd_=float(np.std(accs))
    m0=Model(**cfg,seed=0)
    print("%-24s %8.4f %8d %8d  ±%.3f"%(name,a,m0.params(),m0.macs(),sd_))
