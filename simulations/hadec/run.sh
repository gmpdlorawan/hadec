#!/bin/bash
# Lance une simulation HADEC : ./run.sh -c Hadec2 -r 0 [autres options opp_run]
# INET_ROOT : chemin d'INET 4.6, absolu ou relatif à la racine du dépôt (comme pour `make`).
# Sans INET_ROOT, INET est cherché à côté du dépôt (../inet4.6).
HERE=$(cd "$(dirname "$0")" && pwd)
FLORA=$(cd "$HERE/../.." && pwd)
if [ -n "$INET_ROOT" ]; then
  case "$INET_ROOT" in /*) INET="$INET_ROOT" ;; *) INET="$FLORA/$INET_ROOT" ;; esac
else
  INET="$FLORA/../inet4.6"
fi
INET=$(cd "$INET" 2>/dev/null && pwd) || { echo "INET introuvable : définir INET_ROOT (absolu, ou relatif à $FLORA)" >&2; exit 1; }
command -v opp_run >/dev/null || { echo "Chargez d'abord OMNeT++ : source <omnetpp>/setenv" >&2; exit 1; }
cd "$HERE"
exec opp_run -l "$FLORA/src/flora" -l "$INET/src/INET" -n "$HERE/..:$FLORA/src:$INET/src" -u Cmdenv -f omnetpp.ini "$@"
