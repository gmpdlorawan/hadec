"""Analyses de sensibilité (cas A, HADEC sans dernière chance) :
- vitesse 2, 5, 10, 20 m/s (2 passerelles par opérateur) : HADEC, hystérésis réglée, réactive LinkCheck, trajectoire géoloc. ;
- seuil beta 0,3 / 0,5 / 0,7 (5 m/s, 2, 4 et 8 passerelles par opérateur).
Les points à 5 m/s et beta = 0,5 viennent de la campagne principale (metriques4.csv, graines 0 à 19).
Sorties : sensib_metriques.csv (une ligne par exécution), sensib_stats.csv (médianes, quartiles, écarts appariés, Wilcoxon + Holm).
Usage : python3 analyse_sensibilite.py
"""
import glob, csv, math, collections, os
import numpy as np
from scipy.stats import wilcoxon
from analyse import metriques

METR = ["DER", "ack", "interruption_max_traversee_s", "t_app_int_s", "HDR", "incursion_transfert_inutile",
        "forcerejoin_par_terminal", "allers_retours"]
NOMS = {"Hadec2_g101": "Hadec2", "HysteresisBest": "HysteresisBest", "ReactiveLC": "ReactiveLC", "FallGeo": "FallGeo"}

rows = []
# 1) vitesse : nouvelles exécutions
for f in glob.glob("results3/*-cas=A,prefixe=v*-r*.sca"):
    base = os.path.basename(f)
    cfg = base.split("-")[0]
    pref = base.split("prefixe=")[1].split("-")[0]
    if pref == "v5" or cfg not in NOMS: continue
    m = metriques(f)
    rows.append(dict(etude="vitesse", v=int(pref[1:]), gw=2, beta=0.5, regle=NOMS[cfg], rep=int(m["rep"]) if "rep" in m else int(base.split("-r")[-1][:-4]),
                     **{k: m.get(k, float("nan")) for k in METR}))
# 2) beta : nouvelles exécutions
for f in glob.glob("results3/Hadec2*_g101_b0*-cas=A,prefixe=v5-r*.sca"):
    base = os.path.basename(f); cfg = base.split("-")[0]
    gw = 8 if "_Gw8" in cfg else 4 if "_Gw4" in cfg else 2
    b = 0.3 if cfg.endswith("_b03") else 0.7
    m = metriques(f)
    rows.append(dict(etude="beta", v=5, gw=gw, beta=b, regle="Hadec2", rep=int(base.split("-r")[-1][:-4]),
                     **{k: m.get(k, float("nan")) for k in METR}))
# 3) points de référence : campagne principale
for r in csv.DictReader(open("metriques4.csv")):
    if r["cas"] != "A": continue
    vals = {k: float(r[k]) if r.get(k) not in (None, "", "nan") else float("nan") for k in METR}
    if r["regle"] in ("Hadec2", "HysteresisBest", "ReactiveLC", "FallGeo") and r["gw"] == "2":
        rows.append(dict(etude="vitesse", v=5, gw=2, beta=0.5, regle=r["regle"], rep=int(r["rep"]), **vals))
    if r["regle"] == "Hadec2":
        rows.append(dict(etude="beta", v=5, gw=int(r["gw"]), beta=0.5, regle="Hadec2", rep=int(r["rep"]), **vals))

with open("sensib_metriques.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)

def holm(p):
    idx = sorted(range(len(p)), key=lambda i: p[i]); out = [0.0] * len(p); run = 0.0
    for k, i in enumerate(idx):
        run = max(run, min(1.0, (len(p) - k) * p[i])); out[i] = run
    return out
D = collections.defaultdict(dict)
for r in rows:
    D[(r["etude"], r["v"], r["gw"], r["beta"], r["regle"])][r["rep"]] = r

out = []
def compare(etude, groupes, ref, autres):
    """groupes : clés de famille ; ref/autres : fonctions clé -> (v, gw, beta, regle)."""
    for g in groupes:
        for m in METR:
            fam = []
            for a in autres(g):
                x, y = D.get((etude,) + ref(g), {}), D.get((etude,) + a, {})
                reps = sorted(set(x) & set(y))
                d = [x[k][m] - y[k][m] for k in reps if not (math.isnan(x[k][m]) or math.isnan(y[k][m]))]
                try: p = wilcoxon(d).pvalue if any(abs(v) > 1e-12 for v in d) else 1.0
                except ValueError: p = 1.0
                vy = [y[k][m] for k in y if not math.isnan(y[k][m])]
                fam.append(dict(etude=etude, v=a[0], gw=a[1], beta=a[2], regle=a[3], metrique=m, n=len(vy),
                                mediane=np.median(vy) if vy else float("nan"),
                                q1=np.percentile(vy, 25) if vy else float("nan"), q3=np.percentile(vy, 75) if vy else float("nan"),
                                ecart_median=np.median(d) if d else float("nan"), p=p))
            vx = [v[m] for v in D.get((etude,) + ref(g), {}).values() if not math.isnan(v[m])]
            for f_, ph in zip(fam, holm([f_["p"] for f_ in fam])): f_["p_holm"] = ph
            r0 = ref(g)
            out.append(dict(etude=etude, v=r0[0], gw=r0[1], beta=r0[2], regle=r0[3], metrique=m, n=len(vx),
                            mediane=np.median(vx) if vx else float("nan"), q1=np.percentile(vx, 25) if vx else float("nan"),
                            q3=np.percentile(vx, 75) if vx else float("nan"), ecart_median="", p="", p_holm=""))
            out.extend(fam)
# vitesse : famille = (vitesse, métrique) ; HADEC contre les 3 autres règles
compare("vitesse", [2, 5, 10, 20], lambda v: (v, 2, 0.5, "Hadec2"),
        lambda v: [(v, 2, 0.5, c) for c in ("HysteresisBest", "ReactiveLC", "FallGeo")])
# beta : famille = (densité, métrique) ; beta = 0,5 contre 0,3 et 0,7
compare("beta", [2, 4, 8], lambda g: (5, g, 0.5, "Hadec2"), lambda g: [(5, g, 0.3, "Hadec2"), (5, g, 0.7, "Hadec2")])
with open("sensib_stats.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(out[0].keys())); w.writeheader(); w.writerows(out)
print(len(rows), "exécutions ;", len(out), "lignes de statistiques -> sensib_stats.csv")
