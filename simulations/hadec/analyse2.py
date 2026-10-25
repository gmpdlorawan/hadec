"""
Campagne du modèle corrigé (results2) : métriques par exécution, statistiques et figures.
- metriques2.csv : une ligne par exécution (colonnes gw = 2/4/8 passerelles par opérateur, regle = configuration sans suffixe)
- stats2.csv / stats2.md : médiane [Q1 ; Q3], tests de Wilcoxon appariés HADEC v2 - autre règle, correction de Holm
  par famille (densité, cas, métrique), taille d'effet rang-bisériel
- figures2/<metrique>.png|pdf (2 passerelles, toutes les règles) et figure_densite.png|pdf
Usage : python3 analyse2.py
"""
import os as _os, tempfile as _tf
_HADEC = _os.path.dirname(_os.path.abspath(__file__))
import glob, os, csv, pickle, math, collections
import numpy as np
from scipy.stats import wilcoxon
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker
from analyse import metriques

# RES=results3 : campagne avec le temps d'interruption (sorties suffixées 3, liste de règles objective)
RES = os.environ.get("RES", "results2"); N = os.environ.get("SUF", RES[-1])
# REPS=a-b : graines retenues ; SANS=1 : HADEC v2 sans dernière chance en cas A (fichiers Hadec2*_g101, gamma = 1,01)
R0, R1 = map(int, os.environ.get("REPS", "0-19").split("-"))
SANS = bool(os.environ.get("SANS"))

CACHE = _os.path.join(_tf.gettempdir(), "hadec_cache.pkl")
cache = pickle.load(open(CACHE, "rb")) if os.path.exists(CACHE) else {}
for f in glob.glob(f"{RES}/*.sca"):
    if "HystBalayage" in f: continue
    st = os.path.getmtime(f)
    if f not in cache or cache[f][0] != st:
        cache[f] = (st, metriques(f))
os.makedirs(os.path.dirname(CACHE), exist_ok=True)
pickle.dump(cache, open(CACHE, "wb"))

rows = []
for f, (_, m) in sorted(cache.items()):
    if "HystBalayage" in f or not f.startswith(f"{RES}/"): continue
    r = dict(m)
    r["gw"] = 8 if "_Gw8" in m["config"] else 4 if "_Gw4" in m["config"] else 2
    r["regle"] = m["config"].replace("_Gw4", "").replace("_Gw8", "")
    if not (R0 <= int(r["rep"]) <= R1): continue
    if SANS and r["cas"] == "A" and r["regle"] == "Hadec2": continue
    if SANS and r["regle"] == "Hadec2_g101": r["regle"] = "Hadec2"
    if SANS and r["cas"] == "A" and r["regle"] == "Hadec2Nu": continue
    if SANS and r["regle"] == "Hadec2Nu_g101": r["regle"] = "Hadec2Nu"
    if "_g" in r["regle"]: continue          # autres points du balayage de gamma
    rows.append(r)
keys = []
for r in rows:
    for k in r:
        if k not in keys: keys.append(k)
with open(f"metriques{N}.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=keys); w.writeheader(); w.writerows(rows)

ORDRE = ["Passif", "Periodique", "Reactive", "ReactiveLC", "NaiveEntendEq", "NaivePerdEq", "HysteresisEq", "HysteresisBest",
         "FallEq", "FallGeo", "OracleEq", "Hadec2Nu", "Hadec2Sejour", "Hadec2"]
OBJ = RES == "results3" or bool(os.environ.get("OBJ"))
if OBJ:  # comparaison objective
    ORDRE = ["Passif", "Periodique", "ReactiveLC", "HysteresisBest", "FallGeo", "Hadec2Nu", "Hadec2", "FallEq", "OracleEq"]
NOMS = {"Passif": "Passif seul", "Periodique": "Rejoin\npériodique", "Reactive": "Réactive\n(acc. rec.)", "ReactiveLC": "Réactive\n(LinkCheck)", "NaiveEntendEq": "« B\nentend »",
        "NaivePerdEq": "« A\nperd »", "HysteresisEq": "Hystérésis\n6 dB, 2", "HysteresisBest": "Hystérésis\nréglée",
        "FallEq": "Trajectoire\nGPS exact", "FallGeo": "Trajectoire\ngéoloc. 200 m", "OracleEq": "Trajectoire\nconnue", "Hadec2Nu": "HADEC v2\nsans options",
        "Hadec2Sejour": "HADEC v2\n+ séjour", "Hadec2": "HADEC v2"}
METRIQUES = [
    ("DER", "Livraison des messages montants", "part livrée", +1),
    ("DER_frontiere", "Livraison des messages montants, zone frontière (±2 km)", "part livrée", +1),
    ("ack", "Accusés de réception reçus par le terminal", "part reçue", +1),
    ("traversee_finB", "Traversées terminées gérées par B", "part des traversées", 0),
    ("allers_retours", "Allers-retours par traversée", "nombre moyen", -1),
    ("incursion_transfert_inutile", "Incursions courtes suivies d'un transfert", "part des incursions", -1),
    ("forcerejoin_par_terminal", "ForceRejoinReq envoyés", "par terminal", -1),
    ("succes_par_declenchement", "Reconnexions réussies par ForceRejoinReq envoyé", "part", +1),
    ("relayes_par_terminal", "Messages montants relayés par l'autre opérateur", "par terminal", -1),
    ("signalisation_par_terminal", "Signalisation entre réseaux et ECN", "messages par terminal", -1),
    ("dl_perdus_dc", "Messages descendants perdus (duty cycle)", "part des descendants", -1),
    ("interruption_max_traversee_s", "Plus longue interruption de service pendant une traversée", "secondes", -1),
    ("t_app_int_s", "Interruption au changement de réseau (T_app_int)", "secondes", -1),
    ("HDR", "Traversées avec changement de réseau sans perte (HDR)", "part des traversées", +1),
    ("interruption_totale_s", "Interruption de service cumulée", "secondes par terminal", -1),
    ("energie_J", "Énergie consommée", "joules par terminal", -1),
]
d = collections.defaultdict(dict)
for r in rows:
    d[(r["gw"], r["cas"], r["regle"])][int(r["rep"])] = r

def vals(g, cas, c, m):
    return [v[m] for v in d.get((g, cas, c), {}).values() if isinstance(v.get(m), float) and not math.isnan(v[m])]

def holm(p):
    idx = sorted(range(len(p)), key=lambda i: p[i]); out = [0.0] * len(p); run = 0.0
    for k, i in enumerate(idx):
        run = max(run, min(1.0, (len(p) - k) * p[i])); out[i] = run
    return out

stats = []
for g in (2, 4, 8):
    for cas in ("PA", "A"):
        for m, lib, _, _ in METRIQUES:
            ref = d.get((g, cas, "Hadec2"), {})
            fam = []
            for c in ORDRE:
                v = vals(g, cas, c, m)
                if not v: continue
                row = dict(gw=g, cas=cas, metrique=m, libelle=lib, regle=c, n=len(v), mediane=np.median(v),
                           q1=np.percentile(v, 25), q3=np.percentile(v, 75))
                if c != "Hadec2" and ref:
                    o = d[(g, cas, c)]
                    k = [i for i in sorted(set(o) & set(ref)) if not math.isnan(ref[i][m]) and not math.isnan(o[i][m])]
                    a = np.array([ref[i][m] for i in k]); b = np.array([o[i][m] for i in k]); diff = a - b
                    if len(k) >= 5 and np.any(diff != 0):
                        p = wilcoxon(a, b).pvalue
                        nz = diff[diff != 0]; rk = np.argsort(np.argsort(np.abs(nz))) + 1
                        row["effet_rb"] = (rk[nz > 0].sum() - rk[nz < 0].sum()) / rk.sum()
                    else:
                        p = 1.0; row["effet_rb"] = 0.0
                    row["p"] = p; row["ecart_median"] = float(np.median(diff)) if len(k) else float("nan")
                    fam.append(row)
                stats.append(row)
            for r, pa in zip(fam, holm([r["p"] for r in fam])):
                r["p_holm"] = pa
cols = ["gw", "cas", "metrique", "libelle", "regle", "n", "mediane", "q1", "q3", "ecart_median", "effet_rb", "p", "p_holm"]
with open(f"stats{N}.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=cols, extrasaction="ignore"); w.writeheader()
    for r in stats: w.writerow({k: r.get(k, "") for k in cols})
with open(f"stats{N}.md", "w") as fh:
    for g in (2, 4, 8):
        for cas in ("PA", "A"):
            fh.write(f"\n## {g} passerelles par opérateur — cas {cas}\n")
            for m, lib, _, _ in METRIQUES:
                sel = [r for r in stats if r["gw"] == g and r["cas"] == cas and r["metrique"] == m]
                if not sel: continue
                fh.write(f"\n### {lib}\n\n| Règle | n | Médiane [Q1 ; Q3] | Écart HADEC v2 − règle | p (Holm) |\n|---|---|---|---|---|\n")
                for r in sel:
                    ec = r.get("ecart_median"); p = r.get("p_holm")
                    fh.write(f"| {r['regle']} | {r['n']} | {r['mediane']:.3f} [{r['q1']:.3f} ; {r['q3']:.3f}] | "
                             f"{'' if ec is None else f'{ec:+.3f}'} | {'' if p is None else f'{p:.2g}'} |\n")

# ------------------------- figures -------------------------
BLEU, GRIS, FOND, ENCRE, ENCRE2, GRILLE = "#2a78d6", "#a3a29d", "#e3e2dc", "#0b0b0b", "#52514e", "#ecebe6"
plt.rcParams.update({"font.size": 9, "axes.edgecolor": "#c9c8c2", "xtick.color": ENCRE2, "ytick.color": ENCRE2})
os.makedirs(f"figures{N}", exist_ok=True)
ACTIF = bool(os.environ.get("ACTIF"))   # itinérance active seule : cas A seul, « Aucun transfert » au lieu de « Passif seul »
if ACTIF: NOMS["Passif"] = "Aucun\ntransfert"
if ACTIF: NOMS = {k: v.replace("HADEC v2", "HADEC") for k, v in NOMS.items()}
PANNEAUX = (("A", "Itinérance active, accord actif seul"),) if ACTIF else (("PA", "Cas PA : accords passif et actif"), ("A", "Cas A : accord actif seul"))
for m, titre, unite, sens in METRIQUES:
    fig, axes = plt.subplots(1, len(PANNEAUX), figsize=(8.5 if ACTIF else 12.5, 4.1), sharey=True, squeeze=False)
    axes = axes[0]
    for ax, (cas, nom) in zip(axes, PANNEAUX):
        cfgs = [c for c in ORDRE if vals(2, cas, c, m)]
        if cfgs:
            bp = ax.boxplot([vals(2, cas, c, m) for c in cfgs], widths=0.55, patch_artist=True,
                            medianprops=dict(color=ENCRE, linewidth=1.5), whiskerprops=dict(color=GRIS), capprops=dict(color=GRIS),
                            flierprops=dict(marker="o", markersize=3, markerfacecolor=GRIS, markeredgecolor="none"))
            for patch, c in zip(bp["boxes"], cfgs):
                patch.set_facecolor(BLEU if c == "Hadec2" else FOND); patch.set_edgecolor(BLEU if c == "Hadec2" else GRIS)
            ref = vals(2, cas, "Passif", m)
            if ref: ax.axhline(np.median(ref), color=ENCRE2, linewidth=0.9, linestyle=(0, (4, 3)), zorder=0)
            ax.set_xticks(range(1, len(cfgs) + 1)); ax.set_xticklabels([NOMS[c] for c in cfgs], fontsize=6.5)
        else:
            ax.text(0.5, 0.5, "sans objet", ha="center", va="center", color=ENCRE2, transform=ax.transAxes)
        ax.set_title(nom, fontsize=9.5, loc="left", color=ENCRE)
        ax.grid(axis="y", color=GRILLE, linewidth=0.8); ax.set_axisbelow(True)
        for s in ("top", "right"): ax.spines[s].set_visible(False)
    axes[0].set_ylabel(unite, color=ENCRE2)
    lecture = {+1: "plus haut = mieux", -1: "plus bas = mieux", 0: "à lire avec le coût"}[sens]
    fig.suptitle(f"{titre}  ({lecture})", x=0.01, ha="left", fontsize=11, color=ENCRE)
    fig.text(0.01, 0.01, "2 passerelles par opérateur ; boîtes : médiane et quartiles sur 20 graines ; pointillé : " + ("aucun transfert." if ACTIF else "itinérance passive seule."),
             fontsize=7.5, color=ENCRE2)
    fig.tight_layout(rect=(0, 0.04, 1, 0.93))
    for ext in ("png", "pdf"): fig.savefig(f"figures{N}/{m}.{ext}", dpi=200)
    plt.close(fig)

SERIES = [("Hadec2", "HADEC" if ACTIF else "HADEC v2", "#2a78d6"), ("HysteresisBest", "Hystérésis réglée", "#eb6834"),
          ("FallGeo", "Trajectoire, géoloc. 200 m (inspirée de Fall)", "#1baf7a"), ("ReactiveLC", "Réactive, LinkCheck (inspirée de LoRaRoam)", "#d55181"),
          ("Passif", "Aucun transfert" if ACTIF else "Passif seul", "#8a8984")]
PAN = [("PA", "ack", "Cas PA — accusés de réception"), ("PA", "relayes_par_terminal", "Cas PA — messages relayés par terminal"),
       ("A", "DER", "Cas A — livraison montante"), ("PA", "dl_perdus_dc", "Cas PA — descendants perdus (duty cycle)")]
if OBJ:  # itinérance active seule
    PAN = [("A", "DER", "Livraison montante"), ("A", "ack", "Accusés de réception"),
           ("A", "incursion_transfert_inutile", "Incursions suivies d'un transfert"), ("A", "forcerejoin_par_terminal", "ForceRejoinReq par terminal")]
    if any(vals(g, "A", "Hadec2", "interruption_max_traversee_s") for g in (2, 4, 8)):
        PAN[3] = ("A", "interruption_max_traversee_s", "Plus longue interruption (s)")
fig, axes = plt.subplots(2, 2, figsize=(10, 6.4))
for ax, (cas, m, titre) in zip(axes.flat, PAN):
    for k, (c, nom, col) in enumerate(SERIES):
        xs, me, lo, hi = [], [], [], []
        for g in (2, 4, 8):
            v = vals(g, cas, c, m)
            if not v: continue
            xs.append(g * (1 + 0.035 * (k - 2))); me.append(np.median(v)); lo.append(np.percentile(v, 25)); hi.append(np.percentile(v, 75))
        if xs:
            ax.errorbar(xs, me, yerr=[np.array(me) - lo, np.array(hi) - me], color=col, marker="o", markersize=4, linewidth=2,
                        elinewidth=1.2, label=nom)
    ax.set_xscale("log", base=2); ax.set_xticks([2, 4, 8]); ax.set_xticklabels(["2", "4", "8"])
    ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.set_xlabel("passerelles par opérateur"); ax.set_title(titre, loc="left", fontsize=10)
    ax.grid(color=GRILLE, linewidth=0.8); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
axes.flat[0].legend(frameon=False, fontsize=8)
fig.suptitle("Effet de la densité de passerelles — médiane et intervalle interquartile sur 20 graines", x=0.01, ha="left", fontsize=11)
fig.tight_layout(rect=(0, 0, 1, 0.96))
for ext in ("png", "pdf"): fig.savefig(f"figure_densite{N if N != '2' else ''}.{ext}", dpi=200)
print(len(rows), "exécutions ; stats2.md, figures2/, figure_densite écrits")
