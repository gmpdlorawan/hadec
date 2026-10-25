# HADEC — déclenchement probabiliste de l'itinérance active LoRaWAN

Simulation de l'itinérance active LoRaWAN entre deux opérateurs, avec décision de transfert côté réseau.

HADEC place un nœud de coordination (ECN) entre les serveurs réseau de deux opérateurs LoRaWAN. À chaque message
d'un terminal, l'ECN estime par régression l'écart de signal entre les deux réseaux, calcule la probabilité que le
réseau visité soit meilleur au prochain moment d'action, et demande au réseau d'origine d'envoyer ForceRejoinReq
quand cette probabilité dépasse un seuil déduit des coûts.

Le code est une extension du simulateur FLoRa (OMNeT++ 6.2, INET 4.6), distribuée comme lui sous licence GPL :

| Élément | Rôle |
| --- | --- |
| `src/Roaming/Ecn.*` | nœud de coordination et règles comparées (HADEC, hystérésis, trajectoire, références informées) |
| `src/Roaming/RoamingNetworkServerApp.*` | serveur réseau d'un opérateur : itinérance passive et active (HRStartReq/Ans, nouvelle session) |
| `src/Roaming/RoamingLoRaApp.*` | terminal : ForceRejoinReq, Rejoin-request de type 0, rejoin périodique, règle réactive LinkCheck |
| `src/LoRaPhy/LoRaPathLossCorrelated.*` | modèle d'Oulu avec ombrage réciproque, corrélé dans l'espace et, en option, entre les liens |
| Modifications de FLoRa | fenêtres RX1/RX2 réglables, duty cycle EU868 par sous-bande, polarité IQ des descendants, correction de `LoRaGWRadio` |
| `simulations/hadec/` | scénario, traces, scripts de campagne et d'analyse statistique |

## Auteur

Djièta Ouindé Abdoul Fatas BAMOGO

Laboratoire LAMDI, Université Nazi BONI, Bobo-Dioulasso, Burkina Faso.

Pour reproduire les résultats : voir [`REPRODUIRE.md`](REPRODUIRE.md).
