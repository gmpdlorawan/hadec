#!/bin/bash
# Toutes les campagnes : résultats principaux, validation, seuil beta, vitesse, trafic et canal, calibration.
# Relançable : les exécutions déjà faites sont sautées.
cd "$(dirname "$0")"
command -v opp_run >/dev/null || { echo "Chargez d'abord OMNeT++ : source <omnetpp>/setenv" >&2; exit 1; }
G="--**.ecn.gamma=1.01"
R="Passif Periodique ReactiveLC HysteresisBest FallGeo FallEq OracleEq"
echo "P1 $(date +%H:%M)"
python3 campagne.py --configs Hadec2 Hadec2_Gw4 Hadec2_Gw8 Hadec2Nu Hadec2Nu_Gw4 Hadec2Nu_Gw8 --cas A --reps 0-19 -j 2 --extra="$G" --tag _g101
python3 campagne.py --configs $R $(for c in $R; do echo ${c}_Gw4 ${c}_Gw8; done) --cas A --reps 0-19 -j 2
echo "P2 $(date +%H:%M)"
python3 campagne.py --configs Hadec2 Hadec2_Gw4 Hadec2_Gw8 --cas A --reps 20-39 -j 2 --extra="$G" --tag _g101
python3 campagne.py --configs ReactiveLC HysteresisBest FallGeo ReactiveLC_Gw4 HysteresisBest_Gw4 FallGeo_Gw4 ReactiveLC_Gw8 HysteresisBest_Gw8 FallGeo_Gw8 --cas A --reps 20-39 -j 2
echo "P3 $(date +%H:%M)"
for b in 03 07; do for c in Hadec2 Hadec2_Gw4 Hadec2_Gw8; do
  python3 campagne.py --configs $c --cas A --reps 0-19 -j 2 --extra="$G --**.ecn.beta=0.${b#0}" --tag _g101_b$b
done; done
echo "P4 $(date +%H:%M)"
for v in v20 v10 v2; do
  case $v in v2) D="--duree 8800s";; v10) D="--duree 2400s";; v20) D="--duree 1600s";; esac
  python3 campagne.py --configs Hadec2 --cas A --reps 0-19 -j 2 --prefixe $v $D --extra="$G" --tag _g101
  python3 campagne.py --configs HysteresisBest ReactiveLC FallGeo --cas A --reps 0-19 -j 2 --prefixe $v $D
done
echo "P5 $(date +%H:%M)"
for p in "30 27s 33s" "120 108s 132s"; do set -- $p
  T="--**.loRaNodes[*].app[0].timeToNextPacket=uniform($2,$3)"
  python3 campagne.py --configs Hadec2 --cas A --reps 0-9 -j 2 --extra="$G $T" --tag _g101_p$1
  python3 campagne.py --configs HysteresisBest ReactiveLC FallGeo --cas A --reps 0-9 -j 2 --extra="$T" --tag _p$1
done
for s in 5 10; do
  S="--**.LoRaMedium.pathLoss.sigma=$s --**.ecn.sigmaShadow=${s}dB"
  python3 campagne.py --configs Hadec2 --cas A --reps 0-9 -j 2 --extra="$G $S" --tag _g101_s$s
  python3 campagne.py --configs HysteresisBest ReactiveLC FallGeo --cas A --reps 0-9 -j 2 --extra="$S" --tag _s$s
done
for r in 05 08; do
  RL="--**.LoRaMedium.pathLoss.rhoLinks=0.${r#0}"
  python3 campagne.py --configs Hadec2 --cas A --reps 0-9 -j 2 --extra="$G $RL" --tag _g101_r$r
  python3 campagne.py --configs HysteresisBest ReactiveLC FallGeo --cas A --reps 0-9 -j 2 --extra="$RL" --tag _r$r
done
echo "P6 $(date +%H:%M)"
./calib.sh
echo "RELANCE TERMINEE $(date +%H:%M)"
