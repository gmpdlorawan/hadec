"""
Extraction des métriques HADEC par exécution (un fichier .sca = une exécution).
Usage : python3 analyse.py results/*.sca  -> écrit metriques.csv
"""
import sys, re, csv, glob, os, math, collections
from sca import lire

FRONTIERE = 10000.0

def metriques(path):
    rows, attrs = lire(path)
    run = os.path.basename(path)[:-4]
    cfg = run.split("-")[0]
    rep = int(re.search(r"r(\d+)$", run).group(1))
    itv = dict(re.findall(r"(\w+)=([^,\-]+)", run.split("-", 1)[1]))
    pref = itv.get("prefixe", "v5")
    meta = {}
    mp = f"traces/{pref}_{rep}.csv"
    if os.path.exists(mp):
        for r in csv.DictReader(open(mp)): meta[int(r["index"])] = r["kind"]
    dev = collections.defaultdict(dict)
    ns = collections.defaultdict(float); ecn = {}; gw = collections.defaultdict(float)
    delivered = collections.defaultdict(float); deliveredB = collections.defaultdict(float)
    relayed = collections.defaultdict(float)
    for mod, name, v in rows:
        m = re.search(r"loRaNodes\[(\d+)\]\.app\[0\]$", mod)
        if m: dev[int(m.group(1))][name] = v; continue
        if "networkServer" in mod:
            if name.startswith("delivered "): delivered[int(name.split()[1])] += v
            elif name.startswith("deliveredBorder "): deliveredB[int(name.split()[1])] += v
            elif name.startswith("deliveredRelayed "): relayed[int(name.split()[1])] += v
            else: ns[name] += v
            continue
        if mod.endswith(".ecn"): ecn[name] = v; continue
        if "loRaGW" in mod and (name.startswith("GW_") or name.startswith("downlink")): gw[name] += v
        m = re.search(r"loRaNodes\[(\d+)\]\.LoRaNic\.radio\.energyConsumer$", mod)
        if m and name == "totalEnergyConsumed": dev[int(m.group(1))]["energy"] = v
    out = dict(run=run, config=cfg, rep=rep, **itv)
    n = len(dev)
    S = lambda k: sum(d.get(k, 0) for d in dev.values() if not (isinstance(d.get(k, 0), float) and math.isnan(d.get(k, 0))))
    sent = S("sent"); out["DER"] = sum(delivered.values()) / max(1, sent)
    out["ack"] = S("acks") / max(1, S("sentConfirmed"))
    out["ack_frontiere"] = S("acksBorder") / max(1, S("sentConfirmedBorder"))
    out["DER_frontiere"] = sum(deliveredB.values()) / max(1, S("sentBorder"))
    trav = [d for i, d in dev.items() if meta.get(i) == "traversee"]
    inc = [d for i, d in dev.items() if meta.get(i) == "incursion"]
    if trav:
        out["traversee_finB"] = sum(d["finalNet"] == 1 for d in trav) / len(trav)
        out["changements_par_traversee"] = sum(d["switches"] for d in trav) / len(trav)
        out["allers_retours"] = sum(max(0, d["switches"] - 1) for d in trav) / len(trav)
        xs = [d["firstSwitchX"] - FRONTIERE for d in trav if not math.isnan(d["firstSwitchX"])]
        out["position_premier_passage"] = sorted(xs)[len(xs)//2] if xs else float("nan")
        if any("interruptionMax" in d for d in trav):
            out["interruption_max_traversee_s"] = sum(d.get("interruptionMax", 0) for d in trav) / len(trav)
        if any("handoverGap" in d for d in trav):
            # T_app_int : dernier message livré avant le premier changement de réseau -> premier livré après (traversées avec changement)
            g = [d["handoverGap"] for d in trav if not math.isnan(d.get("handoverGap", float("nan")))]
            out["t_app_int_s"] = sum(g) / len(g) if g else float("nan")
            # HDR : part des traversées dont le changement s'est fait sans perdre de message (écart <= 1,5 période nominale)
            out["HDR"] = sum(1 for x in g if x <= 90.0) / len(trav)
    if any("interruptionTotal" in d for d in dev.values()):
        out["interruption_totale_s"] = S("interruptionTotal") / max(1, n)
    if inc:
        out["incursion_transfert_inutile"] = sum(d["switches"] > 0 for d in inc) / len(inc)
        out["incursion_changements"] = sum(d["switches"] for d in inc) / len(inc)
    out["forcerejoin_par_terminal"] = ns["forceRejoinSent"] / max(1, n)
    out["reconnexions_reussies"] = S("joinAccepts") / max(1, S("rejoinsSent")) if S("rejoinsSent") else float("nan")
    # chaîne d'une reconnexion déclenchée par le réseau : ForceRejoinReq reçu, acceptation reçue, succès par déclenchement
    fr = ns["forceRejoinSent"]
    out["frr_recus"] = S("forceRejoinsReceived") / fr if fr else float("nan")
    out["acceptations_recues"] = S("joinAccepts") / ns["joinAcceptsSent"] if ns["joinAcceptsSent"] else float("nan")
    out["succes_par_declenchement"] = S("joinAccepts") / fr if fr else float("nan")
    sig = ns["bkPRStart"] + ns["bkXmitData"] + ns["bkObs"] + ns["bkHRStart"] + ns["bkSwitchDone"] + ecn.get("ecnPrepares", 0)
    out["signalisation_par_terminal"] = sig / max(1, n)
    out["rejoins_par_terminal"] = S("rejoinsSent") / max(1, n)
    out["dl_perdus_dc"] = (gw["GW_droppedDC"] + gw["downlinkDroppedBand"]) / max(1, gw["downlinkOnTime"] + gw["downlinkRx2"])
    out["energie_J"] = S("energy") / max(1, n)
    # coût de l'itinérance passive : messages montants livrés uniquement par le relais d'un autre opérateur, et descendants relayés
    out["relayes_par_terminal"] = sum(relayed.values()) / max(1, n)
    out["dl_relayes_par_terminal"] = ns["dlViaRelay"] / max(1, n)
    return out

if __name__ == "__main__":
    files = sys.argv[1:] or glob.glob("results/*.sca")
    res = [metriques(f) for f in sorted(files)]
    keys = []
    for r in res:
        for k in r:
            if k not in keys: keys.append(k)
    with open("metriques.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(res)
    for r in res:
        print(r["run"][:40], " ".join(f"{k}={r[k]:.3f}" for k in keys[4:] if isinstance(r.get(k), float)))
