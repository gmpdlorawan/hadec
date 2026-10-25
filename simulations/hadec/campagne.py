"""
Lance une campagne HADEC en parallèle.
Exemples :
  python3 campagne.py --configs Passif Hadec2 --cas PA A --reps 0-19 -j 2
  python3 campagne.py --configs Hadec2 --cas A --reps 0-9 --extra "--**.ecn.beta=0.3" --tag beta03
"""
import argparse, subprocess, itertools, os, sys
from concurrent.futures import ThreadPoolExecutor

def reps(s):
    out = []
    for part in s.split(","):
        a, _, b = part.partition("-"); out += list(range(int(a), int(b or a) + 1))
    return out

def job(cfg, cas, rep, extra, tag, prefix=None, tlim=None):
    filt = f'"$cas"=="{cas}" && $repetition=={rep}'
    args = ["./run.sh", "-c", cfg, "-r", filt]
    pref = prefix or "v5"
    if prefix:
        tf = f"traces/{prefix}_{rep}.movements"
        args += [f'--**.loRaNodes[*].mobility.traceFile="{tf}"', f'--**.ecn.traceFile="{tf}"']
    if tlim:
        args.append(f"--sim-time-limit={tlim}")
    if tag or prefix:
        args.append(f"--output-scalar-file=results3/{cfg}{tag}-cas={cas},prefixe={pref}-r{rep}.sca")
    args += extra
    out = None
    for a_ in args:
        if a_.startswith("--output-scalar-file="): out = a_.split("=", 1)[1]
    if out is None:
        out = f"results3/{cfg}-rx2m=3,cas={cas},prefixe=v5-r{rep}.sca"
    if os.path.exists(out) and os.path.getsize(out) > 1000:
        return cfg, cas, rep, True, ""          # déjà fait : reprise après interruption
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    ok = r.returncode == 0 and "Error" not in r.stdout
    return cfg, cas, rep, ok, (r.stdout[-400:] if not ok else "")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--configs", nargs="+", required=True)
    ap.add_argument("--cas", nargs="+", default=["PA", "A"])
    ap.add_argument("--reps", default="0-19")
    ap.add_argument("--extra", default="")
    ap.add_argument("--tag", default="")
    ap.add_argument("-j", type=int, default=2)
    ap.add_argument("--prefixe", default=None, help="traces/<prefixe>_<rep>.movements (ex. v2)")
    ap.add_argument("--duree", default=None, help="sim-time-limit (ex. 8800s)")
    a = ap.parse_args()
    extra = a.extra.split() if a.extra else []
    jobs = list(itertools.product(a.configs, a.cas, reps(a.reps)))
    with ThreadPoolExecutor(a.j) as ex:
        for cfg, cas, rep, ok, err in ex.map(lambda t: job(*t, extra, a.tag, a.prefixe, a.duree), jobs):
            print(f"{cfg:14s} {cas:3s} r{rep:<3d} {'ok' if ok else 'ECHEC'}", flush=True)
            if not ok: print(err)
