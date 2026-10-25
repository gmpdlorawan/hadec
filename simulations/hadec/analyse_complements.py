"""Analyses complémentaires (cas A, HADEC sans dernière chance), comparaisons appariées par graine :
période des messages 30 / 120 s, ombrage 5 / 10 dB et corrélation de l'ombrage entre les liens 0,5 / 0,8 :
HADEC contre hystérésis réglée, réactive et trajectoire (2 passerelles par opérateur, graines 0 à 9).
Sorties : compl_metriques.csv, compl_stats.csv.
"""
import os as _os, tempfile as _tf
_HADEC = _os.path.dirname(_os.path.abspath(__file__))
import glob, csv, math, collections, os, sys, pickle
import numpy as np
from scipy.stats import wilcoxon
from analyse import metriques

METR = ["DER", "ack", "interruption_max_traversee_s", "t_app_int_s", "HDR", "incursion_transfert_inutile",
        "forcerejoin_par_terminal", "allers_retours"]
CACHE = _os.path.join(_tf.gettempdir(), "hadec_compl_cache.pkl")
cache = pickle.load(open(CACHE, "rb")) if os.path.exists(CACHE) else {}
def M(f):
    st = os.path.getmtime(f)
    if f not in cache or cache[f][0] != st: cache[f] = (st, metriques(f))
    return cache[f][1]

rows = []
def ajoute(etude, cond, gw, regle, f):
    m = M(f); rep = int(os.path.basename(f).split("-r")[-1][:-4])
    rows.append(dict(etude=etude, cond=cond, gw=gw, regle=regle, rep=rep, **{k: m.get(k, float("nan")) for k in METR}))
def gwde(cfg): return 8 if "_Gw8" in cfg else 4 if "_Gw4" in cfg else 2
NOM = {"Hadec2": "Hadec2", "HysteresisBest": "HysteresisBest", "ReactiveLC": "ReactiveLC", "FallGeo": "FallGeo"}

for et, tags in (("periode", ("p30", "p120")), ("ombrage", ("s5", "s10")), ("correlation", ("r05", "r08"))):
    for t in tags:
        for f in glob.glob(f"results3/*_{t}-cas=A,prefixe=v5-r*.sca"):
            cfg = os.path.basename(f).split("-")[0].replace(f"_{t}", "").replace("_g101", "")
            if cfg in NOM: ajoute(et, t[1:], 2, NOM[cfg], f)
# références de la campagne principale (graines 0 à 9, 5 m/s, 60 s, 7,8 dB, liens indépendants)
for r in csv.DictReader(open("metriques4.csv")):
    if r["cas"] != "A" or r["regle"] not in ("Hadec2", "HysteresisBest", "ReactiveLC", "FallGeo"): continue
    vals = {k: float(r[k]) if r.get(k) not in (None, "", "nan") else float("nan") for k in METR}
    g = int(r["gw"])
    if g == 2:
        for et, c in (("periode", "60"), ("ombrage", "7.8"), ("correlation", "0")):
            if int(r["rep"]) <= 9: rows.append(dict(etude=et, cond=c, gw=2, regle=r["regle"], rep=int(r["rep"]), **vals))
pickle.dump(cache, open(CACHE, "wb"))
with open("compl_metriques.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)

D = collections.defaultdict(dict)
for r in rows: D[(r["etude"], r["cond"], r["gw"], r["regle"])][r["rep"]] = r
def holm(p):
    idx = sorted(range(len(p)), key=lambda i: p[i]); out = [0.0] * len(p); run = 0.0
    for k, i in enumerate(idx):
        run = max(run, min(1.0, (len(p) - k) * p[i])); out[i] = run
    return out
def desc(key, m):
    v = [x[m] for x in D.get(key, {}).values() if not math.isnan(x[m])]
    return (len(v), np.median(v), np.percentile(v, 25), np.percentile(v, 75)) if v else (0, float("nan"), float("nan"), float("nan"))
out = []
def famille(ref, autres, label):
    for m in METR:
        fam = []
        for a in autres:
            x, y = D.get(ref, {}), D.get(a, {})
            reps = sorted(set(x) & set(y))
            d = [x[k][m] - y[k][m] for k in reps if not (math.isnan(x[k][m]) or math.isnan(y[k][m]))]
            try: p = wilcoxon(d).pvalue if any(abs(v) > 1e-12 for v in d) else 1.0
            except ValueError: p = 1.0
            n, med, q1, q3 = desc(a, m)
            fam.append(dict(famille=label, ref="/".join(map(str, ref)), autre="/".join(map(str, a)), metrique=m, n=n,
                            mediane_autre=med, mediane_ref=desc(ref, m)[1], ecart_median=np.median(d) if d else float("nan"), p=p))
        for f_, ph in zip(fam, holm([f_["p"] for f_ in fam])): f_["p_holm"] = ph
        out.extend(fam)
for et, conds in (("periode", ("30", "60", "120")), ("ombrage", ("5", "7.8", "10")), ("correlation", ("0", "05", "08"))):
    for c in conds:
        famille((et, c, 2, "Hadec2"), [(et, c, 2, x) for x in ("HysteresisBest", "ReactiveLC", "FallGeo")], f"{et}_{c}")
with open("compl_stats.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(out[0].keys())); w.writeheader(); w.writerows(out)
print(len(rows), "lignes ;", len(out), "comparaisons -> compl_stats.csv")
