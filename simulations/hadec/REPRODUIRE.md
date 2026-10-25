# Simulation HADEC : installation et campagnes

## Versions

| Logiciel | Version |
| --- | --- |
| OMNeT++ | 6.2.0 |
| INET | 4.6.0 |
| FLoRa | 1.3.2, avec les extensions de ce dépôt (`src/Roaming/`, `src/LoRaPhy/`) |
| Python | 3.10 ou plus, avec numpy, scipy, matplotlib |

FLoRa est distribué sous licence GPL ; ces extensions le sont aussi.

## Contenu

| Élément | Emplacement |
| --- | --- |
| Extensions de FLoRa : deux serveurs réseau, itinérance active (ForceRejoinReq, Rejoin-request type 0, HRStartReq/Ans), ECN et règles comparées | `src/Roaming/`, `src/LoRaPhy/LoRaPathLossCorrelated.*`, `src/LoRaPhy/LoRaReceiver.cc` |
| Scénario et configurations (une configuration par règle et par densité) | `simulations/hadec/omnetpp.ini` |
| Traces de mobilité (fournies) : 5 m/s, graines 0 à 39 ; 2, 10 et 20 m/s, graines 0 à 19 | `simulations/hadec/traces/`, produites par `generer_traces.py` |
| Lancement d'une simulation | `run.sh` |
| Lancement des campagnes (reprise automatique) | `campagne.py`, `campagnes.sh` (toutes les campagnes), `reglages.sh` (balayages de réglage de l'hystérésis et de gamma) |
| Métriques par exécution | `analyse.py` (fonction `metriques`) |
| Statistiques (Wilcoxon apparié, Holm, rang-bisériel) | `analyse2.py`, `analyse_sensibilite.py`, `analyse_complements.py` |
| Réglages et calibration | `choisir_hysteresis.py`, `gamma.py`, `analyse_calibration.py` |
| Étude en une dimension (sans simulateur) | `analyse_preliminaire.py` |

## Étapes

```sh
# 0. Environnement
source <omnetpp>/setenv
export INET_ROOT=<chemin d'INET 4.6>     # RELATIF à la racine du dépôt, par ex. ../inet4.6
#    (le Makefile de FLoRa ajoute « ../ » devant : un chemin absolu fait échouer la compilation ;
#     run.sh accepte les deux formes)

# 1. Compiler FLoRa et les extensions (depuis la racine du dépôt)
make makefiles && make MODE=release

# 2. Traces : déjà fournies. Pour les régénérer (résultat identique octet pour octet) :
cd simulations/hadec
python3 generer_traces.py --graines 40 --vitesse 5  --part-incursion 0.3 --prefixe v5  --sortie traces/
for v in 2 10 20; do
  python3 generer_traces.py --graines 20 --vitesse $v --part-incursion 0.3 --prefixe v$v --sortie traces/
done

# 3. Scénario de référence : HADEC, 2 passerelles par opérateur, graine 0 (moins d'une minute)
mkdir -p results3
./run.sh -c Hadec2 -r '"$cas"=="A" && $repetition==0' --**.ecn.gamma=1.01 \
         --output-scalar-file='results3/Hadec2_g101-cas=A,prefixe=v5-r0.sca' --output-vector-file=/dev/null
python3 -c "from analyse import metriques; m = metriques('results3/Hadec2_g101-cas=A,prefixe=v5-r0.sca'); print(m['DER'], m['t_app_int_s'])"
#    attendu : 0.812152203338848 84.4081646485598  (livraison montante, T_app_int en s)

# 4. Campagnes complètes (environ 2 300 simulations ; 6 à 7 heures sur deux cœurs ; 3,5 Mo par exécution)
./campagnes.sh                    # résultats principaux, validation, beta, vitesse, compléments, calibration ;
                                  # relançable : les exécutions déjà faites sont sautées

# 5. Statistiques (fichiers metriques*.csv, stats*.csv, sensib_*.csv, compl_*.csv)
RES=results3 ACTIF=1 SANS=1 REPS=0-19 SUF=4 python3 analyse2.py    # résultats principaux
RES=results3 ACTIF=1 SANS=1 REPS=20-39 SUF=V python3 analyse2.py   # validation
python3 analyse_sensibilite.py                                      # vitesse et beta
python3 analyse_complements.py                                      # période, ombrage, corrélation
python3 analyse_calibration.py                                      # calibration du score (données de calib.sh)

# 6. Réglages (facultatif ; environ 480 simulations) et étude en une dimension
./reglages.sh                     # balayages de l'hystérésis et de gamma, puis choisir_hysteresis.py et gamma.py
python3 analyse_preliminaire.py   # quelques secondes, sans simulateur
```

Les résultats ne sont pas fournis : ils se régénèrent avec les étapes 4 et 5.

HADEC dans sa version retenue correspond à `Hadec2` avec `gamma = 1.01` (pas de règle de dernière chance) et `beta = 0.5`.
Les graines 20 à 39 servent uniquement à la validation : aucun réglage n'a été choisi sur elles.

## Exactitude de la reproduction

Chaque graine fixe tout l'aléa d'une exécution : relancer une exécution redonne exactement les mêmes métriques, quels
que soient la machine, le dossier ou le nom des fichiers de sortie. Cette propriété a été rétablie par une correction
de FLoRa (`src/LoRa/LoRaGWRadio.cc`) : la passerelle gardait parfois dans sa liste des réceptions en cours un minuteur
déjà supprimé, et un nouveau minuteur alloué à la même adresse mémoire était pris pour une réception en cours. Avant
cette correction, environ une exécution sur dix dépendait de la disposition de la mémoire.

## Points pratiques

- Supprimer les fichiers `*.vec` (aucun script ne les lit) pour économiser le disque ; `omnetpp.ini` désactive déjà l'enregistrement des vecteurs.
- `campagne.py` saute les exécutions dont le `.sca` existe : supprimer les `.sca` tronqués (moins de 3 Mo) avant de relancer une campagne interrompue.
- Les options OMNeT++ se passent à `campagne.py` sous la forme `--extra="--**.ecn.beta=0.3"` (avec `=`).
