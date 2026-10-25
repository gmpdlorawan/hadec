"""
Choisit le réglage de l'hystérésis sur le balayage (results3/HystBalayage-*, produit par reglages.sh)
selon un critère fixé à l'avance :
cas PA -> médiane des accusés de réception la plus haute ; cas A -> médiane de la livraison montante la plus haute.
Écrit hysteresis_choix.csv ; ajoute [Config HysteresisBest] à omnetpp.ini s'il n'y est pas déjà
(le réglage retenu, marge 9 dB et 2 messages en cas A, y figure déjà).
"""
import glob, re, csv, collections
import numpy as np
from analyse import metriques

d = collections.defaultdict(list)
for f in glob.glob("results3/HystBalayage-*.sca"):
    m = metriques(f)
    hm = re.search(r"hm=(\d+)", f).group(1); hc = re.search(r"hc=(\d+)", f).group(1)
    d[(m["cas"], int(hm), int(hc))].append(m)
choix = {}
rows = []
for cas, crit in (("PA", "ack"), ("A", "DER")):
    best = None
    for (c, hm, hc), ms in sorted(d.items()):
        if c != cas: continue
        v = float(np.median([x[crit] for x in ms]))
        rows.append(dict(cas=cas, marge_dB=hm, messages=hc, critere=crit, mediane=round(v, 4), n=len(ms)))
        if best is None or v > best[0]: best = (v, hm, hc)
    choix[cas] = best
with open("hysteresis_choix.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
pa, a = choix["PA"], choix["A"]
txt = f"""

# réglage retenu par choisir_hysteresis.py (PA : max accusés de réception ; A : max livraison)
[Config HysteresisBest]
extends = HysteresisEq
**.ecn.hystMargin = ("${{cas}}" == "PA") ? {pa[1]}dB : {a[1]}dB
**.ecn.hystCount = ("${{cas}}" == "PA") ? {pa[2]} : {a[2]}

[Config HysteresisBest_Gw4]
extends = HysteresisBest, Gw4

[Config HysteresisBest_Gw8]
extends = HysteresisBest, Gw8
"""
if "[Config HysteresisBest]" not in open("omnetpp.ini").read():
    open("omnetpp.ini", "a").write(txt)
print("PA :", pa, " A :", a)
