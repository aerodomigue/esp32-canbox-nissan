# Relecture et validation locale de la PR #10

La [PR #10](https://github.com/aerodomigue/esp32-canbox-nissan/pull/10) et ses
sept commentaires de conversation ont été relus. À la demande du propriétaire,
les cinq propositions de mapping d’`anyzlo` sont **retirées** : le preset
actif reprend exactement celui de la tête PR `495f346`. Les protections du
décodeur sont conservées et renforcées. `LOG ON` ajoute les lectures OBD-II
service 01 et leurs résultats pour comparer les références aux trames CAN.
Toutes les lignes de diagnostic portent le préfixe exact **`ODB`**.

Les changements sont sur la branche dédiée `feat/obd-calibration-logging`,
sans suivi de la branche distante de la PR. La branche locale
`review/pr-10-anyzlo` est remise sur `origin/feat/can-decoding-fixes` (`495f346`).
L’état précédent reste accessible sur `archive/pr10-anyzlo-mappings`. Aucun push,
commentaire GitHub ni modification distante de la PR n’a été effectué.

## Preset actif et retrait des propositions

`data/NissanJukeF15.json` est identique octet par octet à la version `495f346`.
Le profil expérimental des propositions et les huit tests qui le chargeaient
ont été supprimés. Le tableau suivant décrit les changements retirés ;
aucun de ces remplacements n'est appliqué. Le format UART est également
celui de la tête PR : `0x23` sur trois octets et `0x24` sur deux octets.
L'essai non commité de `0x23` sur 13 octets a été annulé ; le troisième octet
constant `0x06`, les calculs de trajet et le relais climat ne sont pas introduits.

| Signal | Preset actif initial | Proposition retirée |
| --- | --- | --- |
| Régime | `0x180`, octets 0–1, brut / 7 | brut / 8 |
| Vitesse | `0x284`, octets 0–1, brut / 100 | octets 4–5, brut / 100 |
| Carburant | `0x5C5[0]`, `[255,0,0,45]` | `[255,-70,0,40]` |
| Autonomie | `0x54C`, octets 6–7, brut × 100 / 338 | `0x2DE`, octets 6–7, brut / 10 |
| Consommation instantanée | `0x580[3]`, brut | `0x580[1]`, échelle 1 |

Les tests du preset actif utilisent des octets contradictoires pour détecter
un changement involontaire de source.
Le retour au preset initial ne constitue pas une validation physique de ses
formules. L’analyse du Juke 2011 1.6 DIG-T manuel et ses sources sont dans
[JUKE_2011_CAN_ANALYSIS.fr.md](JUKE_2011_CAN_ANALYSIS.fr.md).

## Protection et capture

Le décodeur rejette les trames RTR, étendues, les IDs hors 11 bits, les DLC
supérieurs à huit et les trames trop courtes pour un champ configuré. La
validation porte sur **tous** les champs avant toute écriture : une trame
incomplète ne met pas partiellement à jour les variables véhicule.

La structure de réception est initialisée ; le log lit au maximum huit octets,
préserve les octets, l’ID, le DLC et les flags, et n’invente pas de payload RTR.
La file RX contient 128 trames et la boucle en retire au maximum 32 sans attente.
Le timestamp est pris au retrait de la file ; ce n’est pas une horloge matérielle
du bus. L’affichage USB peut encore ralentir la réception. `ODB DRIVER` publie
les pertes, erreurs et état du contrôleur au maximum une fois par seconde.
L’écho des caractères de commande est suspendu pendant le log pour préserver
le début des lignes. Le débit utilise `convertSpeed(500)` en kilobits/s.

## Références OBD et comparaison

En mode réel, `LOG ON` découvre les PIDs annoncés par les calculateurs puis
interroge seulement les références disponibles : régime, vitesse, températures
liquide/ambiante, niveau carburant, tension calculateur, pression absolue du
collecteur et débit de carburant. Une requête au maximum toutes les 500 ms,
une fenêtre de réponse de 250 ms et une temporisation de découverte évitent
une boucle de requêtes rapide. Les mesures utilisent l’adresse physique du
calculateur retenu. `LOG OFF` arrête les nouvelles requêtes ; le mode mock et
l’OTA suspendent le client. Les lectures ne modifient pas les valeurs de
l’autoradio ni les mappings.

Les logs distinguent `ODB TX`, `ODB RAW_RX`, `ODB SUPPORTED`, `ODB RX`,
`ODB NEGATIVE`, `ODB TIMEOUT`, `ODB STATE` et `ODB DRIVER`. `queued` signifie
mise en file, sans preuve d’acquittement sur le bus. Un timeout n’est jamais
converti en zéro. Les unités restent explicites : % ne devient pas litres,
L/h ne devient pas automatiquement L/100 km.

`tools/compare_obd_can.py` rapproche chaque référence des trames horodatées
les plus proches, en séparant session/PID/calculateur/unité. Il respecte les
rejets atomiques du preset et les débordements de `millis()`, rejette les
couples requête/ECU impossibles et conserve les paramètres sans candidat CAN.
Il produit `comparison.json` et `matches.csv`, sans changer le preset.
Le [guide de capture et comparaison](OBD_CALIBRATION.fr.md) explique les
commandes, conversions et limites.

L’exemple `test/fixtures/obd_calibration_example.txt` est **synthétique** :
RPM `/8` et vitesse octets 4–5 coïncident avec ses références ; le preset actif
présente des écarts de 91 tr/min et 50 km/h. Cela teste le mécanisme de
comparaison, sans apporter de preuve sur le véhicule.

## Capture historique disponible

Le fichier `canlog_20260627_222201.txt` cité dans la PR n’a pas été retrouvé.
L’export authentique `tools/data/nissan trame export.txt` a été récupéré dans
[l’historique Git](https://github.com/aerodomigue/esp32-canbox-nissan/blob/009f032dc395895bc8dd0eab34d55633301aed0a/tools/data/nissan%20trame%20export.txt).

- Révision : `009f032dc395895bc8dd0eab34d55633301aed0a`.
- Blob : `6c7faf62b99c09df014bda638e737fa1dad308fe`.
- SHA-256 : `35de3a33b22716b88b28a2adfa1e6d6ed916b230cbbe2bba569b2206700df5c6`.
- Ajout en janvier 2026 ; date de capture inconnue.
- Copie locale : `.local-validation/historical-nissan-trame-export.txt`.

Le propriétaire confirme le Juke 2011 DIG-T manuel, probablement au ralenti
à l’arrêt, sans certitude. Le code d’un ancien logger affichait une structure
non initialisée avant lecture CAN ; la version exacte utilisée pour cet export
reste inconnue. Cela fragilise sa provenance sans prouver la cause de chaque
anomalie. Il n’a ni timestamps ni références OBD synchronisées.

Le replay utilise le **décodeur C++ de production**. Sur 1 016 lignes,
1 013 trames sont acceptées et trois lignes sont rejetées : ID `0x3FC91000`
(ligne 107) et DLC 20 (lignes 547 et 698). Avec le preset actif, 157 trames sont
traitées, 856 IDs sont inconnus et aucune trame acceptée par le lecteur n’est
rejetée ensuite par le décodeur.

| Signal | Résultat du preset actif | Limite |
| --- | --- | --- |
| Régime, 114 trames | 182 tr/min constants | Suspect pour un moteur en marche ; mapping non validé |
| Vitesse | Aucun relevé `0x284` | Tests synthétiques uniquement |
| Carburant, 12 trames | 33 sous l’hypothèse litres | Nature de l’octet contestée ; aucun niveau réel connu |
| Autonomie | Pas de `0x54C`, valeur initiale 0 | 120 trames `0x2DE` visibles, remplacement retiré |
| Consommation, 10 trames | Dernier octet 3 : 6 | Motif cyclique `2,4,6,0` ; unité inconnue |

La relecture vérifie le comportement logiciel. Cette capture ne permet pas
une calibration physique ni de confirmer l’autonomie annoncée dans la PR.

## Environnement et résultats

L’environnement est isolé dans `.venv/` et `.pio-core/` ; `platformio.ini`
fixe désormais ce dernier chemin et les caches CI le suivent. Versions :
Python 3.14.4, PlatformIO 6.2.0, pioarduino 55.3.312, Arduino ESP32 3.3.12,
RISC-V 14.2.0, ArduinoJson 7.4.3 et ESP32-TWAI-CAN `01251a4`.
Les étapes reproductibles sont dans [DEVELOPMENT.md](DEVELOPMENT.md).

**152 tests C++ et 34 tests Python réussis**, aucun échec ni test ignoré avec
la capture complète.

| Suite C++ | Réussis |
| --- | ---: |
| OTA | 10 |
| Paramètres véhicule | 9 |
| Décodage CAN | 42 |
| Relecture CAN | 6 |
| Client OBD et formatage brut | 27 |
| Encodage UART | 24 |
| Commandes série | 34 |

Les 27 tests OBD/formatage passent également sous AddressSanitizer et
UndefinedBehaviorSanitizer. Ils couvrent notamment les huit conversions,
la découverte multi-ECU, les DLC/flags/longueurs ISO-TP, les erreurs TX,
les timeouts, l’arrêt/reprise et le débordement de `millis()`.
Les tests Python couvrent les formats historiques/actuels, le CSV, les unités,
les sessions, les offsets candidats, les délais et la protection des entrées.

- Firmware ESP32-C3 : compilation réussie, RAM 17 132 octets (5,2 %),
  programme d'environ 446 000 octets (34,0 % de la partition applicative).
- LittleFS : construction réussie avec le preset initial restauré.
- Exemple de comparaison : deux références et cinq trames, aucun rejet CAN.
- Preset initial identique à la PR et absence du profil expérimental : vérifiés.
- `pip check` et `git diff --check` : réussis.

Les journaux du retrait sont dans `.local-validation/revert-native-tests.log`,
`revert-python-tests.log`, `revert-can-replay.log`, `revert-firmware-build.log` et
`revert-filesystem-build.log`. Aucun ESP32 ni véhicule n'était connecté.
Une nouvelle capture simultanée CAN/ODB reste nécessaire.

## Reproduire la relecture complète

```bash
mkdir -p .local-validation
git show '009f032dc395895bc8dd0eab34d55633301aed0a:tools/data/nissan trame export.txt' > .local-validation/historical-nissan-trame-export.txt

CAN_LOG_PATH="$PWD/.local-validation/historical-nissan-trame-export.txt" \
  .venv/bin/pio test -e native -e native_radio -e native_serial
.venv/bin/python -B -m unittest discover -s tools -p 'test_*can*.py' -v
.venv/bin/pio run -e esp32-c3-devkitm-1
.venv/bin/pio run -e esp32-c3-devkitm-1 -t buildfs
```

Sans `CAN_LOG_PATH`, seul le test facultatif de capture complète est ignoré ;
l’extrait authentique reste testé. Pour appliquer le retour au preset initial
sur la voiture, charger le JSON dans LittleFS ou via CANBox Manager, puis
recharger le profil. Une OTA applicative seule laisse le preset installé intact.
