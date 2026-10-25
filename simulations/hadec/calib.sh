#!/bin/bash
# Calibration du score Conf_B (graines 0 à 9, et 20 à 29 pour la validation) : chaque ligne = prédiction au message k, écart observé au message suivant.
cd "$(dirname "$0")"
command -v opp_run >/dev/null || { echo "Chargez d'abord OMNeT++ : source <omnetpp>/setenv" >&2; exit 1; }
for c in Hadec2 Hadec2_Gw4 Hadec2_Gw8; do for r in 0 1 2 3 4 5 6 7 8 9 20 21 22 23 24 25 26 27 28 29; do
  out="results_calib/${c}-r${r}.csv"; [ -s "$out" ] && continue
  ./run.sh -c $c -r "\"\$cas\"==\"A\" && \$repetition==$r" "--**.ecn.gamma=1.01" "--**.ecn.calibFile=\"$out\"" \
     --output-scalar-file=/dev/null --output-vector-file=/dev/null > /dev/null 2>&1 && echo "$c r$r ok" || echo "$c r$r ECHEC"
done; done
echo CALIB TERMINEE
