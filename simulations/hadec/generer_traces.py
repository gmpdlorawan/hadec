"""
Traces de mobilité HADEC (format BonnMotion 2D : une ligne par terminal, "t x y t x y ...").

Deux familles de trajets entre le réseau A (gauche) et le réseau B (droite), frontière x = 10 km :
  - traversee : le terminal part dans A et s'installe dans B (passage en itinérance active utile) ;
  - incursion : le terminal entre dans la zone commune ou dans B, puis revient dans A
                (rester en itinérance passive est préférable).
Un fichier CSV de métadonnées accompagne chaque trace (type de trajet, vitesse, instants).

Usage : python3 generer_traces.py --graines 20 --vitesse 5 --part-incursion 0.3 --sortie traces/
"""
import argparse, os, csv
import numpy as np

P = dict(n=100, frontiere=10000.0, xA=(4000.0, 6000.0), xB=(14000.0, 16000.0),
         y=(1500.0, 8500.0), retour=(9000.0, 12500.0), depart=(0.0, 600.0), pause_fin=600.0)


def trajet(rng, kind, v):
    t0 = rng.uniform(*P["depart"])
    x0 = rng.uniform(*P["xA"]); y0 = rng.uniform(*P["y"])
    pts = [(0.0, x0, y0)]
    if t0 > 0:
        pts.append((t0, x0, y0))
    if kind == "traversee":
        x1 = rng.uniform(*P["xB"]); y1 = np.clip(y0 + rng.normal(0, 1500), *P["y"])
        d = np.hypot(x1 - x0, y1 - y0); t1 = t0 + d / v
        pts.append((t1, x1, y1))
        fin = t1
    else:
        xr = rng.uniform(*P["retour"]); yr = np.clip(y0 + rng.normal(0, 1500), *P["y"])
        d1 = np.hypot(xr - x0, yr - y0); t1 = t0 + d1 / v
        pts.append((t1, xr, yr))
        x2 = rng.uniform(*P["xA"]); y2 = np.clip(yr + rng.normal(0, 1500), *P["y"])
        t2 = t1 + np.hypot(x2 - xr, y2 - yr) / v
        pts.append((t2, x2, y2))
        fin = t2
    return pts, t0, fin


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--graines", type=int, default=20)
    ap.add_argument("--vitesse", type=float, default=5.0)
    ap.add_argument("--part-incursion", type=float, default=0.3)
    ap.add_argument("--n", type=int, default=P["n"])
    ap.add_argument("--prefixe", default="v5")
    ap.add_argument("--sortie", default="traces")
    a = ap.parse_args()
    os.makedirs(a.sortie, exist_ok=True)
    for g in range(a.graines):
        rng = np.random.default_rng(1000 + g)
        lignes, meta = [], []
        for i in range(a.n):
            kind = "incursion" if rng.uniform() < a.part_incursion else "traversee"
            pts, t0, fin = trajet(rng, kind, a.vitesse)
            lignes.append(" ".join(f"{t:.1f} {x:.1f} {y:.1f}" for t, x, y in pts))
            meta.append(dict(index=i, kind=kind, v=a.vitesse, depart=round(t0, 1), fin=round(fin, 1)))
        with open(os.path.join(a.sortie, f"{a.prefixe}_{g}.movements"), "w") as f:
            f.write("\n".join(lignes) + "\n")
        with open(os.path.join(a.sortie, f"{a.prefixe}_{g}.csv"), "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(meta[0])); w.writeheader(); w.writerows(meta)
    print("traces écrites dans", a.sortie)


if __name__ == "__main__":
    main()
