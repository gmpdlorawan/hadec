#!/bin/bash
# Réglages des règles (cas A sauf mention) : balayage de l'hystérésis (marge 3/6/9/12 dB x 1/2/3 messages, cas PA et A,
# 2 passerelles, graines 0 à 9) et balayage de gamma pour HADEC (0,3 / 0,5 / 0,7 / 0,9 ; 2/4/8 passerelles, graines 0 à 19).
# Relançable : les exécutions déjà faites sont sautées.
cd "$(dirname "$0")"
command -v opp_run >/dev/null || { echo "Chargez d'abord OMNeT++ : source <omnetpp>/setenv" >&2; exit 1; }
for cas in PA A; do for r in 0 1 2 3 4 5 6 7 8 9; do
  ls results3/HystBalayage-*cas=$cas*-r$r.sca >/dev/null 2>&1 && [ $(ls results3/HystBalayage-*cas=$cas*-r$r.sca | wc -l) -ge 12 ] && continue
  ./run.sh -c HystBalayage -r "\"\$cas\"==\"$cas\" && \$repetition==$r" --output-vector-file=/dev/null > /dev/null 2>&1 &
  [ $(jobs -r | wc -l) -ge 2 ] && wait -n
done; done; wait
for g in 03 05 07 09; do
  python3 campagne.py --configs Hadec2 Hadec2_Gw4 Hadec2_Gw8 --cas A --reps 0-19 -j 2 --extra="--**.ecn.gamma=0.${g#0}" --tag _g$g
done
python3 choisir_hysteresis.py
python3 gamma.py
echo "REGLAGES TERMINES"
