# Relecture et validation locale de la PR #10

La [PR #10](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10) a été
relue, y compris les sept commentaires de conversation et l’absence de
commentaires de revue supplémentaires. Les cinq propositions d’`anyzlo` ont
été appliquées sur la tête de la PR, `495f346d20745bc8b23be6e580cf0aa62c2faf48`.
Les changements sont préparés sur la branche locale `review/pr-10-anyzlo`.
Aucun push ni modification de la PR sur GitHub n’a été effectué.

## Changements du preset

| Signal | Avant | Après | Proposition |
| --- | --- | --- | --- |
| Régime moteur | `0x180`, octets 0–1, brut / 7 | Même source, brut / 8 | [Commentaire](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10#issuecomment-6101376432) |
| Vitesse | `0x284`, octets 0–1, brut / 100 | Octets 4–5, brut / 100 | [Commentaire](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10#issuecomment-6101379982) |
| Carburant | `0x5C5`, octet 0, `[255, 0, 0, 45]` | `MAP_RANGE [255, -70, 0, 40]` | [Commentaire](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10#issuecomment-6101386358) |
| Autonomie | `0x54C`, octets 6–7, brut × 100 / 338 | `0x2DE`, octets 6–7, brut / 10 | [Commentaire](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10#issuecomment-6101390485) |
| Consommation instantanée | `0x580`, octet 3, brut | Octet 1, `SCALE [1, 1, 0]`, sans masque | [Commentaire](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10#issuecomment-6101392558) |

Les valeurs sur deux octets sont non signées et en big endian. L’ancienne
source d’autonomie est supprimée : une trame `0x54C` ne peut plus écraser la
valeur issue de `0x2DE`.

La formule carburant est appliquée exactement comme proposée, avec la division
entière du décodeur. Un octet non signé donne une plage de **0 à 31 L** :
`255 → 0`, `125 → 16`, `64 → 23`, `0 → 31`. Le point théorique `-70 → 40 L`
est inaccessible pour ce type d’entrée. La formule ne décrit donc pas la
capacité physique du réservoir ; aucune capacité NVS n’a été changée.

Les tests utilisent des octets contradictoires pour distinguer les anciennes
sources des nouvelles, vérifient la troncature entière et les bornes carburant,
et empêchent l’écrasement de l’autonomie par l’ancienne trame.

## Capture CAN disponible

Le fichier `canlog_20260627_222201.txt` cité dans la PR n’est présent ni dans le
clone ni dans les pièces jointes des commentaires. Un autre export authentique
a été retrouvé dans l’historique Git :

- Fichier : `tools/data/nissan trame export.txt`.
- Révision : `009f032dc395895bc8dd0eab34d55633301aed0a`, du 19 janvier 2026.
- Blob : `6c7faf62b99c09df014bda638e737fa1dad308fe`.
- SHA-256 : `35de3a33b22716b88b28a2adfa1e6d6ed916b230cbbe2bba569b2206700df5c6`.
- [Source historique](https://github.com/aerodomigue/esp32-canbox-nissan/blob/009f032dc395895bc8dd0eab34d55633301aed0a/tools/data/nissan%20trame%20export.txt).

Le décodeur C++ de production est utilisé pour relire cette capture ; les
résultats ne sont pas produits par une réimplémentation Python des formules.
Un extrait de quinze enregistrements est conservé comme fixture, avec les
numéros de lignes et la provenance. Le fichier complet reste disponible
localement dans `.local-validation/historical-nissan-trame-export.txt` et peut
être réextrait de Git.

Sur **1 016 enregistrements**, le lecteur accepte **1 013 trames**, rejette
**3 enregistrements**, puis le décodeur traite **277 trames** et compte
**736 trames inconnues**. Les rejets sont l’ID invalide `0x3FC91000` à la ligne
107 et deux DLC de 20 octets aux lignes 547 et 698.

| Signal | Trames | Résultat du nouveau preset | Portée de la vérification |
| --- | ---: | --- | --- |
| Régime `0x180` | 114 | **159 tr/min constants** (`0x04FB / 8`) | Valeur suspecte pour un moteur en marche. Le log ne confirme pas ce mapping. Les octets 2–3 varient, mais cela ne suffit pas à choisir une autre source. |
| Vitesse `0x284` | 0 | Aucun relevé | Correction couverte par tests synthétiques uniquement. |
| Carburant `0x5C5` | 12 | **23 L** (`0x40`) | Valeur plausible, sans mesure réelle de carburant. Compteur associé : 239 944 km. |
| Autonomie `0x2DE` | 120 | **375 km** (`0x0EAC / 10`) | Valeur plausible. Pas de relevé du tableau de bord ; `0x54C` absent. |
| Consommation `0x580` | 10 | Valeurs brutes **20 à 60** | Soit 2 à 6 L/100 km sous l’hypothèse d’unité de 0,1. L’ancien octet 3 suit le motif cyclique `2, 4, 6, 0`, ce qui suggère un compteur. |

Cette capture n’a pas de mesures synchronisées du tableau de bord ni de
contexte moteur fiable. **La cohérence du décodage logiciel est vérifiée,
mais la calibration physique reste partielle et le régime moteur doit être
vérifié sur véhicule.** Les trames de janvier ne peuvent pas confirmer les
305 km annoncés pour la capture de juin dans la PR.

## Environnement et vérifications

Les instructions reproductibles sont dans [DEVELOPMENT.md](DEVELOPMENT.md).
L’environnement Python et le cache PlatformIO sont locaux au projet : `.venv/`
et `.pio-core/`. Versions utilisées : Python 3.14.4, PlatformIO 6.2.0,
pioarduino 55.3.312, Arduino ESP32 3.3.12, compilateur RISC-V 14.2.0,
ArduinoJson 7.4.3, ESP32-TWAI-CAN `01251a4`. L’outil de fusion utilisé par
PlatformIO est esptool 5.4.0 ; le venv de développement contient esptool 5.5.0.

La configuration des tests série génère désormais `include/version.h` depuis
un clone neuf. CI lance les trois environnements natifs. Le débit CAN est
passé à `convertSpeed(500)` en kilobits/s, conformément à l’API TWAI ; l’ancien
appel avec `500000` débordait le paramètre `uint16_t` et reposait sur le débit
par défaut de la bibliothèque.

Le décodeur refuse également les trames trop courtes pour les champs du
preset, avant toute écriture. Cela évite qu’une trame de vitesse de moins de
six octets fabrique une vitesse à partir d’octets absents. Les tests vérifient
également que le rejet d’une trame ne met pas partiellement à jour ses champs.

Résultats finaux : **122 tests réussis**, aucun échec ni test ignoré lors de
l’exécution avec la capture complète.

| Environnement | Suite | Tests réussis |
| --- | --- | ---: |
| `native` | Logique OTA | 10 |
| `native` | Paramètres véhicule | 9 |
| `native` | Décodage CAN et trames tronquées | 39 |
| `native` | Relecture CAN authentique | 6 |
| `native_radio` | Encodage UART | 24 |
| `native_serial` | Commandes série | 34 |

- Compilation firmware ESP32-C3 : réussie, RAM 16 956 octets (5,2 %),
  programme 441 074 octets (33,7 % de la partition applicative).
- Construction LittleFS avec le nouveau preset : réussie.
- Test série avec suppression préalable du header généré : réussi,
  `include/version.h` recréé automatiquement.
- `pip check` : aucune dépendance incohérente.
- Vérification des paramètres JSON, des IDs uniques et des limites d’octets : réussie.
- Vérification de l’extrait contre les quinze lignes source : réussie.
- `git diff --check` : réussi.

Les journaux locaux sont dans `.local-validation/native-tests.log`,
`can-replay.log`, `firmware-build.log` et `filesystem-build.log`.

## Reproduire la relecture complète

Depuis la racine du dépôt, après la préparation de l’environnement :

```bash
export PLATFORMIO_CORE_DIR="$PWD/.pio-core"
mkdir -p .local-validation

git show '009f032dc395895bc8dd0eab34d55633301aed0a:tools/data/nissan trame export.txt' > .local-validation/historical-nissan-trame-export.txt

CAN_LOG_PATH="$PWD/.local-validation/historical-nissan-trame-export.txt" \
  .venv/bin/pio test -e native -e native_radio -e native_serial

.venv/bin/pio run -e esp32-c3-devkitm-1
.venv/bin/pio run -e esp32-c3-devkitm-1 -t buildfs
```

Le lecteur historique accepte le format `RX ID: 0x180 | DLC: 8 | Data: ...`.
Sans `CAN_LOG_PATH`, les tests de l’extrait authentique sont exécutés ; seul
le test facultatif de fichier externe est marqué ignoré.

Pour vérifier ces corrections dans le véhicule, il faudra charger le nouveau
JSON dans LittleFS ou via CANBox Manager et le recharger. Une mise à jour OTA
de l’application seule ne modifie pas le preset déjà installé. Aucun
matériel ESP32 ni véhicule n’était connecté pour cette validation.
