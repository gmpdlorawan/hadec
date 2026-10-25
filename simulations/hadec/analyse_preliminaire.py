"""Étude en une dimension (sans simulateur) : terminal traversant la frontière entre deux réseaux,
écart de signal moyen selon le modèle d'Oulu, ombrage de 7,8 dB par lien.
Affiche le nombre de changements de réseau et la position du premier passage pour des seuils H = 0, 6 et 12 dB,
la largeur de la zone où l'écart moyen est inférieur à son écart-type, et le rapport signal sur bruit de la pente
estimée par régression selon le nombre de points."""
import numpy as np
rng=np.random.default_rng(1)
n,s=3,7.8*np.sqrt(2)
def mean(x): return 10*n*(np.log10(np.abs(x-2500)+1)-np.log10(np.abs(7500-x)+1))
for H in (0,6,12):
  sw=[];first=[]
  for _ in range(4000):
    x=np.arange(2600+rng.uniform(0,300),7400,300.0)
    d=mean(x)+rng.normal(0,s,len(x))
    srv=0;k=0;f=None
    for xi,di in zip(x,d):
      if srv==0 and di>=H: srv=1;k+=1; f=xi if f is None else f
      elif srv==1 and -di>=H: srv=0;k+=1
    sw.append(k); first.append(f-5000 if f is not None else np.nan)
  print(H,"dB: changements %.1f, premier passage %+.0f m"%(np.mean(sw),np.nanmedian(first)))
x=np.linspace(5000,7000,20001); print("zone |E[Delta]|<sigma_Delta: +-%.0f m"%(x[np.argmax(mean(x)>=s)]-5000))
for N in (3,5,8,10,15):
  print(N, "SNR pente (1,6 dB/msg): %.2f"%(1.6*np.sqrt(N*(N*N-1)/12)/s))
