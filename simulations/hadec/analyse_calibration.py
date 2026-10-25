"""Calibration du score Conf_B à partir des fichiers de calib.sh (results_calib/<config>-r<graine>.csv).
Chaque ligne : Conf_B prédit, ..., issue observée au message suivant (0/1), nombre de points de la régression.
Affiche, par densité et pour les graines 0 à 9 puis 20 à 29 : nombre de prédictions, score de Brier, et la fréquence
observée pour Conf_B entre 0,4 et 0,6 selon le nombre de points (3 à 5, 10 ou plus)."""
import glob, os
import numpy as np

for cfg, gw in (("Hadec2", 2), ("Hadec2_Gw4", 4), ("Hadec2_Gw8", 8)):
    for nom, pat in (("graines 0-9", "r[0-9]"), ("graines 20-29", "r2[0-9]")):
        fs = [f for f in glob.glob(f"results_calib/{cfg}-{pat}.csv") if os.path.getsize(f)]
        if not fs: continue
        X = np.vstack([np.loadtxt(f, delimiter=",", ndmin=2) for f in fs])
        p, y = X[:, 0], X[:, 3]
        s = f"{gw} passerelles, {nom} : n = {len(p)}, Brier = {np.mean((p - y) ** 2):.3f}"
        if X.shape[1] >= 5:
            k = X[:, 4]; m = (p >= 0.4) & (p < 0.6)
            s += f", Conf_B 0,4-0,6 : {y[m & (k <= 5)].mean():.2f} (3-5 points), {y[m & (k >= 10)].mean():.2f} (10 points ou plus)"
        print(s)
