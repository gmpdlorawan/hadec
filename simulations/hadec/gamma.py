"""
Balayage de gamma (règle de dernière chance de HADEC), cas A, 2/4/8 passerelles par opérateur, graines 0 à 19.
Exécutions produites par reglages.sh (results3/Hadec2*_gXX-*) ; gamma = 1,01 : pas de dernière chance (version retenue).
Critère de choix fixé à l'avance, le même que pour l'hystérésis : médiane de la livraison montante la plus haute.
Écrit gamma.csv (médianes) et gamma_tests.csv (Wilcoxon apparié contre gamma = 0,5 et contre l'hystérésis réglée, Holm).
"""
import os as _os, tempfile as _tf
_HADEC = _os.path.dirname(_os.path.abspath(__file__))
import glob, os, re, pickle, csv, collections, math
import numpy as np
from scipy.stats import wilcoxon
from analyse import metriques

CACHE = _os.path.join(_tf.gettempdir(), "hadec_cache.pkl")
cache = pickle.load(open(CACHE, "rb")) if os.path.exists(CACHE) else {}
def m_(f):
    st = os.path.getmtime(f)
    if f not in cache or cache[f][0] != st: cache[f] = (st, metriques(f))
    return cache[f][1]

GW = {"": 2, "_Gw4": 4, "_Gw8": 8}
d = collections.defaultdict(dict)          # (gw, regle) -> {rep: metriques}
for f in glob.glob("results3/*cas=A,*.sca"):
    b = os.path.basename(f)
    m = re.match(r"(Hadec2|HysteresisBest)(_Gw4|_Gw8)?(?:_g(\d+))?-", b)
    if not m: continue
    g = GW[m.group(2) or ""]
    regle = m.group(1) if m.group(1) == "HysteresisBest" else "g" + (m.group(3) or "05")
    if m.group(1) == "HysteresisBest" and m.group(3): continue
    rep = int(re.search(r"-r(\d+)\.sca$", b).group(1))
    d[(g, regle)][rep] = m_(f)
pickle.dump(cache, open(CACHE, "wb"))

GAM = [("g03", 0.3), ("g05", 0.5), ("g07", 0.7), ("g09", 0.9), ("g101", None)]
MET = ["DER", "ack", "incursion_transfert_inutile", "allers_retours", "forcerejoin_par_terminal",
       "interruption_max_traversee_s", "t_app_int_s", "HDR"]
def v(g, r, m): return {k: x[m] for k, x in d[(g, r)].items() if isinstance(x.get(m), float) and not math.isnan(x[m])}

rows = []
for g in (2, 4, 8):
    for r, gam in GAM + [("HysteresisBest", "hyst")]:
        if not d.get((g, r)): continue
        row = dict(gw=g, regle=r if r == "HysteresisBest" else ("sans" if gam is None else gam), n=len(d[(g, r)]))
        for m in MET:
            x = list(v(g, r, m).values()); row[m] = round(float(np.median(x)), 4) if x else float("nan")
        rows.append(row)
with open("gamma.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

def holm(p):
    idx = sorted(range(len(p)), key=lambda i: p[i]); out = [0.0] * len(p); run = 0.0
    for k, i in enumerate(idx):
        run = max(run, min(1.0, (len(p) - k) * p[i])); out[i] = run
    return out
tests = []
for g in (2, 4, 8):
    for m in MET:
        fam = []
        for r, gam in GAM:
            for ref in ("g05", "HysteresisBest"):
                if r == ref or not d.get((g, r)) or not d.get((g, ref)): continue
                a, b = v(g, r, m), v(g, ref, m); k = sorted(set(a) & set(b))
                if len(k) < 5: continue
                x = np.array([a[i] for i in k]); y = np.array([b[i] for i in k])
                p = 1.0 if not np.any(x - y) else float(wilcoxon(x, y).pvalue)
                fam.append(dict(gw=g, metrique=m, gamma=("sans" if gam is None else gam), contre=ref, n=len(k),
                                ecart_median=round(float(np.median(x - y)), 4), p=p))
        for t, ph in zip(fam, holm([t["p"] for t in fam])): t["p_holm"] = ph
        tests += fam
with open("gamma_tests.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(tests[0])); w.writeheader(); w.writerows(tests)

for g in (2, 4, 8):
    print(f"\n{g} passerelles, cas A")
    print(f"{'gamma':>7} {'n':>3} {'livr':>6} {'ack':>6} {'incurs':>6} {'A/R':>5} {'FRR':>5} {'intmax':>6} {'Tapp':>5} {'HDR':>5}")
    for r in rows:
        if r["gw"] != g: continue
        print(f"{str(r['regle'])[:7]:>7} {r['n']:>3} {r['DER']:6.3f} {r['ack']:6.3f} {r['incursion_transfert_inutile']:6.2f} "
              f"{r['allers_retours']:5.2f} {r['forcerejoin_par_terminal']:5.2f} {r['interruption_max_traversee_s']:6.0f} "
              f"{r['t_app_int_s']:5.0f} {r['HDR']:5.2f}")
    cands = [r for r in rows if r["gw"] == g and r["regle"] != "HysteresisBest" and r["n"] == 20]
    if cands:
        best = max(cands, key=lambda r: r["DER"])
        print("  choix (livraison la plus haute) : gamma =", best["regle"])
