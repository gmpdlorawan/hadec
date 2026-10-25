"""Lecture minimale des fichiers .sca d'OMNeT++ (scalaires) -> liste de (module, nom, valeur)."""
import shlex

def lire(path):
    out, attrs = [], {}
    with open(path) as f:
        for line in f:
            if line.startswith("scalar "):
                parts = shlex.split(line)
                out.append((parts[1], parts[2], float(parts[3]) if parts[3] not in ("nan", "-nan") else float("nan")))
            elif line.startswith("itervar ") or line.startswith("attr "):
                p = shlex.split(line); attrs[p[1]] = p[2] if len(p) > 2 else ""
    return out, attrs
