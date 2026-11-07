# Capture CAN et références OBD pour calibration

> **Langues :** **Français** | [English](OBD_CALIBRATION.md)

En véhicule réel, `LOG ON` enregistre toutes les trames CAN reçues avec leurs
octets inchangés et interroge les références OBD-II **service 01** annoncées.
Avec le profil exact `Nissan Juke F15`, il tente aussi **cinq lectures candidates
service 22 dans trois groupes diagnostiques bornés `10C0`/`1081`**. Toutes les
lignes diagnostiques commencent par **`ODB`** : OBD-II est le nom du protocole,
`ODB` le préfixe exact du log. `LOG OFF` arrête capture et mesures, avec une
éventuelle unique tentative de fermeture de session au mieux. Les transmissions
déjà mises en file dans le contrôleur ne peuvent pas être rappelées.

Le diagnostic ne change ni le preset ni les valeurs envoyées au poste.
Les [preuves du 14 octobre](JUKE_F15_CAN_MAPPING.fr.md) restent liées à leur ancien
preset et à leur capture à basse vitesse ; ne pas les présenter comme une
validation de la configuration actuelle. Le [mapping F15/DUDU actuel](F15_DUDU_BOOT_TRIP_MAPPING.fr.md)
utilise vitesse `0x280 B4..B5 /100`, autonomie `0x2DE B6..B7 /10`, régime
`0x180 B0..B1 /8`, eau validée `0x551 B0−40` et huile explicitement provisoire
`0x580 B4−40`. Ces sources CAN périodiques diffèrent des PIDs/DIDs diagnostiques.
La consommation utilise un coefficient carburant provisoire et une moyenne RAM
depuis démarrage.

La fausse jauge `0x5C5` entière et les anciennes consommations `0x580` restent
retirées. `FuelValid: 0` distingue `Fuel: 0 L` de compatibilité d'une quantité
mesurée. `FuelRaw` n'est ni des L/h ni des L/100 km. La source extérieure incorrecte
est retirée sans remplacement inventé. Le guide actuel ci-dessus détaille les
validités, marqueurs provisoires et restrictions UART.

La [recherche des commandes Nissan et le tableau de couverture](NISSAN_DIAGNOSTIC_RESEARCH.fr.md)
précisent les valeurs réellement vérifiables. Aucune lecture constructeur
supplémentaire n'a été validée pour le Juke 2011 MR16DDT manuel : portes,
direction, clignotants, éclairage, frein à main, consommation moyenne et
autonomie n'ont actuellement pas de référence OBD interrogée.

## Utilisation

1. Installer la **nouvelle OTA applicative** contenant le client OBD étendu.
   Sélectionner le profil nommé exactement `Nissan Juke F15` pour les groupes
   diagnostiques. Transférer aussi le JSON actuel séparément pour installer le
   mapping véhicule F15/DUDU corrigé : une OTA ne remplace pas les presets LittleFS.
2. Ouvrir la sortie série USB et enregistrer **toutes les lignes**, notamment
   `RX 0x`, `ODB RAW_RX`, `ODB RX`, `ODB PROBE_RX`, `ODB SESSION_RX`,
   `ODB SESSION_STATE` et les erreurs diagnostiques,
   sans préfixe d'heure ou de couleur ajouté. Pour le moniteur PlatformIO :
   `.venv/bin/pio device monitor --filter direct`.
3. Envoyer `LOG ON`. Attendre les lignes `ODB SUPPORTED`, puis les `ODB RX`
   des PIDs annoncés et les éventuels résultats des sondes candidates.
   L'ESP32 effectue lui-même les lectures ; l'ELM327 n'a pas besoin de partager
   la prise de diagnostic avec lui.
4. Annoter les états réels : contact, démarrage, ralenti froid/chaud, régime
   affiché, jauge, vitesse et unités du combiné. Si possible, marquer les
   appuis sur l'embrayage et les passages neutre/marche arrière à l'arrêt.
   Envoyer `LOG OFF` à la fin.

Le mode démonstration n'envoie aucune requête OBD. Le polling est suspendu
pendant une OTA et reprend par une nouvelle découverte si `LOG ON` reste actif.

## Paramètres et transport

Le client découvre les capacités avec les PIDs `00`, puis `20` et `40` si
le calculateur annonce les pages suivantes. Les réponses sont conservées
séparément pour les huit adresses possibles `0x7E8` à `0x7EF`. Les lectures
de mesure ciblent l'adresse physique `0x7E0` à `0x7E7` du premier calculateur
qui annonce le PID demandé. Aucun paramètre non annoncé n'est interrogé.

Le client définit **20 PIDs standard** ; seuls ceux annoncés par un
calculateur sont interrogés. `A`, `B`, `C`, `D` sont les octets de mesure
successifs après le service/PID, pas les octets d'encapsulation ISO-TP.
`AB = 256×A+B` et `CD = 256×C+D` : mots non signés, octet fort en premier.

| PID | Référence | Octets | Conversion | Unité |
| --- | --- | --- | --- | --- |
| `0C` | Régime | 2 | `AB/4` | tr/min |
| `0D` | Vitesse | 1 | `A` | km/h |
| `05` | Température liquide de refroidissement | 1 | `A−40` | °C |
| `2F` | Niveau de carburant | 1 | `100×A/255` | % |
| `42` | Tension du calculateur | 2 | `AB/1000` | V |
| `46` | Température ambiante | 1 | `A−40` | °C |
| `0B` | Pression absolue du collecteur | 1 | `A` | kPa |
| `5E` | Débit de carburant | 2 | `AB/20` | L/h |
| `03` | États des systèmes de carburant 1 et 2 | 2 | masques de bits `A`, `B` séparés | bitmask |
| `04` | Charge moteur calculée | 1 | `100×A/255` | % |
| `06` | Correction carburant court terme, banc 1 | 1 | `100×A/128−100` | % |
| `07` | Correction carburant long terme, banc 1 | 1 | `100×A/128−100` | % |
| `0F` | Température d'air d'admission | 1 | `A−40` | °C |
| `10` | Débit massique d'air | 2 | `AB/100` | g/s |
| `11` | Position du papillon | 1 | `100×A/255` | % |
| `13` | Présence des sondes à oxygène | 1 | masque de bits `A` | bitmask |
| `24` | Sonde à oxygène large bande 1 : rapport d'équivalence et tension | 4 | `AB/32768` ; `CD/8192` | ratio ; V |
| `44` | Rapport d'équivalence carburant/air commandé | 2 | `AB/32768` | ratio |
| `51` | Type de carburant | 1 | énumération `A` | enum |
| `5C` | Température d'huile moteur | 1 | `A−40` | °C |

Les huit définitions de PID précédentes sont conservées. Les PIDs `03` et
`24` produisent chacun deux lignes `ODB RX` pour une réponse. Les masques
d'état/présence et l'énumération du carburant restent des valeurs brutes,
pas des pourcentages.

Les libellés `unit=bitmask`, `unit=enum`, `unit=ratio` sont ceux du log ;
les températures standard utilisent `unit=degC`. Pour distinguer les deux
valeurs d'une réponse, utiliser aussi `name` : `fuel_system_1` et
`fuel_system_2` pour `03` ; `wideband_equivalence_ratio_1` et
`wideband_o2_voltage_1` pour `24`.

Ces conversions sont publiques dans la
[table PID de CSS Electronics](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979).
La disponibilité dépend des calculateurs de la voiture. Le niveau en %
n'est pas directement une quantité en litres ; la tension calculateur
n'est pas nécessairement identique à une tension d'alternateur. Aucun
rapprochement automatique entre L/h et L/100 km n'est fait.

Une seule requête est en cours, avec un intervalle minimal de **500 ms**
entre tentatives. La fenêtre de réponse est de **250 ms** ; la découverte
collecte les réponses de plusieurs calculateurs jusqu'à son expiration.
Après trois tentatives de découverte sans réponse exploitable, le client
attend cinq secondes avant de recommencer. Un PID annoncé qui ne répond pas
produit un timeout, jamais une valeur zéro inventée. Ces délais ne bloquent
pas le streaming CAN brut/USB.

Après découverte, l'ordonnanceur standard alloue **deux créneaux rapides pour
un créneau normal**. Le cycle rapide est `10, 03, 10, 24, 10, 44`, en sautant
les PIDs non pris en charge. Les autres PIDs annoncés tournent dans les
créneaux normaux. Le débit d'air et la richesse sont ainsi prioritaires,
mais les mesures ne sont pas simultanées : conserver leurs horodatages et
identifiants de requête/calculateur pour les comparer.

### Lectures candidates Nissan : profil F15 exact uniquement

Les cinq **candidates service `22`** sont distinctes des pages de support des
PIDs standard. Elles ne fonctionnent qu'en capture réelle avec le nom exact
`Nissan Juke F15`, après réponse du calculateur moteur `0x7E8` à la découverte
standard. L'adresse de requête physique est `0x7E0` ; les autres calculateurs
répondants ne lui sont pas substitués. La découverte doit aussi annoncer au
moins un des 20 PIDs définis pour passer à l'étape de polling.

| DID | Fonction candidate | Donnée enregistrée | Unité |
| --- | --- | --- | --- |
| `1101` | Contrôle eau candidat | premier octet `A` non signé, conversion candidate `A−50` | degC |
| `111F` | Température d'huile candidate | premier octet `A` non signé, conversion candidate `A−50` | degC |
| `1114` | Jauge de carburant candidate | premier octet `A` non signé, sans pourcentage ni volume | raw |
| `1183` | Rapport engagé candidat | premier octet `A` non signé, sans énumération des rapports | raw |
| `1304` | Embrayage candidat | premier octet `A` non signé, sans interprétation booléenne/par bit | raw |

Le firmware exécute trois petits groupes de session constructeur. Chaque groupe
est tenté une fois par session de capture activée, avec au moins **10 secondes
entre débuts de groupe**. Une reprise de capture réelle après OTA recommence la
découverte et le budget d'essais. Le premier groupe commence dans un créneau
normal de l'ordonnanceur :

```text
10 C0 → 22 11 01 → 22 11 1F → 10 81
10 C0 → 22 11 14 → 22 11 83 → 10 81
10 C0 → 22 13 04             → 10 81
```

Chaque ouverture envoie `02 10 C0 00 00 00 00 00` à `0x7E0`. Seul un positif
`50 C0` correspondant depuis `0x7E8` autorise les lectures ; `50 40` n'est pas
assimilé silencieusement. Le dernier `02 10 81 00 00 00 00 00` demande la fermeture
et enregistre `50 81`, un refus ou un timeout. Ce sont des **changements bornés
d'état diagnostique**, pas seulement des lectures service 22 isolées. Le code
Nissan CAN11 original de Car Scanner déclare cette paire ouverture/fermeture.

Il reste **une requête en attente**, au moins **500 ms** entre tentatives et
une fenêtre de **250 ms**. Un groupe suspend le polling OBD normal, tandis que
réception CAN, streaming USB et traitement radio continuent sans blocage.
Deux lectures donnent normalement quatre tentatives espacées de 500 ms. Quand
la phase de lecture atteint **2 secondes**, un traitement retardé ferme la session
plutôt qu'ajouter d'autres lectures. Aucun maintien, accès de sécurité, actuateur,
adaptation, effacement de DTC, reset, programmation ou balayage illimité n'est ajouté.

Refus/silence d'ouverture : aucune lecture et arrêt des groupes restants après
nettoyage. Une fermeture non confirmée arrête également les autres groupes et
consigne `state=unknown` ; le polling standard reprend. Un refus/timeout de mesure
est journalisé sans valeur inventée et ne démontre pas l'absence des autres DID.
Ainsi le refus du contrôle eau n'empêche pas l'essai d'huile dans une session
acceptée. Un positif tardif ne lève pas un refus terminal pour cette transaction.
Le NRC `0x78` peut précéder un positif dans les 250 ms initiales sans prolonger
la fenêtre. Un échec TX local ne fabrique ni session ouverte ni mesure reçue.

`LOG OFF`, profil, mode simulé ou OTA entraînent au mieux une tentative de fermeture
lorsqu'une ouverture a pu partir et que les 500 ms le permettent. Il n'y a pas
d'attente synchrone. Une fermeture mise en file reste **non confirmée** ; si délai
ou erreur TX empêche le nettoyage, `ODB SESSION_STATE` indique l'état inconnu.
Les trames déjà mises en file ne peuvent pas être rappelées. Changer de profil
pendant une capture ne réactive pas les groupes déjà tentés.

La capture contrôlée précédente contient quatre refus `7F 22 80` **sans ouverture
`10C0`**. Ils ne prouvent ni absence de sonde ni acceptation de session. Même une
future réponse positive ne validera pas seule le libellé ou la formule sur cet
ECU exact. Les conversions génériques de jauge divergent ; le moniteur ECM
MY2011 expose une tension et le combiné des litres. `1114` reste donc brut.
La sonde d'huile et un moniteur CONSULT en °C sont documentés dans cette famille
MR16DDT MY2011, mais le support de `111F` et l'identité de `0x580 B4` ne sont pas
indépendamment confirmés ici.

Seules les réponses standard 11 bits, non RTR, DLC 8, en trame ISO-TP simple sont
décodées. Longueur, ECU physique, service et PID/DID/écho de session doivent
correspondre. Une mesure candidate est exactement `62 DID-haut DID-bas A`
(longueur 4) ; un positif de session peut contenir deux à sept octets, avec ses
octets supplémentaires conservés en `ODB RAW_RX`. L'ISO-TP multi-trames n'est pas
réassemblé : les trames brutes restent enregistrées, sans application d'un offset
fixe. PIDs absents et candidates refusées sont indisponibles, jamais des zéros.
Aucun résultat diagnostique ne remplace le preset, le dashboard ou les mesures radio.

Voir le [contrat F15/DUDU et ses sources](F15_DUDU_BOOT_TRIP_MAPPING.fr.md#essai-diagnostique-nissan-borné)
pour la provenance des sessions et les mappings physiques provisoires distincts.

## Format du log

Extrait **synthétique**, disponible dans
`test/fixtures/obd_calibration_example.txt` :

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

Groupe de session complet **synthétique**, pas un échange ECU observé :

```text
ODB TX t_ms=2000 seq=5 can_id=0x7E0 service=0x10 subfunction=0xC0 status=queued data=02 10 C0 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 02 50 C0 00 00 00 00 00 | t_ms=2010 ext=0 rtr=0
ODB SESSION_RX t_ms=2010 seq=5 request_id=0x7E0 ecu=0x7E8 service=0x10 subfunction=0xC0 status=opened length=2 latency_ms=10
ODB TX t_ms=2500 seq=6 can_id=0x7E0 service=0x22 did=0x1101 status=queued data=03 22 11 01 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 01 6E 00 00 00 | t_ms=2510 ext=0 rtr=0
ODB PROBE_RX t_ms=2510 seq=6 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x1101 name=nissan_coolant_temperature_candidate raw=0x6E value=60.000 unit=degC latency_ms=10
ODB TX t_ms=3000 seq=7 can_id=0x7E0 service=0x22 did=0x111F status=queued data=03 22 11 1F 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 1F 6F 00 00 00 | t_ms=3010 ext=0 rtr=0
ODB PROBE_RX t_ms=3010 seq=7 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x111F name=nissan_oil_temperature_candidate raw=0x6F value=61.000 unit=degC latency_ms=10
ODB TX t_ms=3500 seq=8 can_id=0x7E0 service=0x10 subfunction=0x81 status=queued data=02 10 81 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 02 50 81 00 00 00 00 00 | t_ms=3510 ext=0 rtr=0
ODB SESSION_RX t_ms=3510 seq=8 request_id=0x7E0 ecu=0x7E8 service=0x10 subfunction=0x81 status=closed length=2 latency_ms=10
```

Noms candidats : `nissan_coolant_temperature_candidate` (`1101`),
`nissan_oil_temperature_candidate` (`111F`), `nissan_fuel_level_raw` (`1114`),
`nissan_gear_position_raw` (`1183`) et `nissan_clutch_switch_raw` (`1304`).

- `RX` : trame CAN brute, ID/DLC/octets et flags inchangés.
- `ODB RAW_RX` : trame brute reçue dans la plage de diagnostic `0x7DF–0x7EF`.
- `ODB TX` : requête avec les octets exacts et le statut `queued` ou `failed`.
- `ODB SUPPORTED` : page de capacités reçue, bitmap et calculateur.
- `ODB RX` : référence service 01 décodée avec PID, calculateur, séquence, unité et latence.
- `ODB PROBE_RX` : résultat candidat service 22, avec `service=0x22`,
  `did=0x....`, `name=...`, `raw=0x..`, `value=...`, `unit=degC` ou `unit=raw`,
  ainsi que requête, calculateur, séquence, horodatage et latence. L'octet
  brut reste visible même lorsqu'une conversion candidate est affichée.
- `ODB NEGATIVE`, `ODB TIMEOUT` : réponses négatives ou absence de réponse.
  Les lignes service 22 portent `service=0x22 did=0x....`, de même que leurs
  `ODB TX`, qui conservent les octets envoyés exacts. Les sessions portent
  `service=0x10 subfunction=0xC0` ou `subfunction=0x81`. Les formats service 01
  et les préfixes `RX 0x` / `ODB RAW_RX` restent inchangés.
- `ODB SESSION_RX` : écho positif de session constructeur correspondant,
  `service=0x10`, `subfunction=0xC0`/`0x81`, statut opened/closed, longueur et latence.
- `ODB SESSION_STATE` : limite de lecture, annulation ou fermeture non confirmée ;
  `state=unknown` ne signifie pas fermeture réussie.
- `ODB STATE` : début/fin ou état de découverte.
- `ODB DRIVER` : pertes RX, échecs TX, occupation de la file, erreurs et état
  du contrôleur, au maximum une ligne par seconde.
- `ODB BUFFER` : occupation du tampon USB, lignes RAW/diagnostic refusées,
  écritures partielles reprises et réponses refusées, au même rythme.

`queued` signifie que TWAI a accepté la mise en file, pas que la voiture a
acquitté la requête. C'est le comportement de
[l'API TWAI Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32/api-reference/peripherals/twai.html).
Pour les trames reçues et les références associées, `t_ms` vient de `millis()`
au retrait de la file, avant l'affichage. `ODB TX` porte l'instant de tentative
de mise en file ; la latence inclut donc les délais logiciels et de file.
Ce n'est pas l'heure matérielle des échanges sur le bus. Les timestamps
reviennent à zéro après un redémarrage et débordent
après environ 49,7 jours.

La réception est non bloquante, avec une file de 128 trames et un drainage
limité à 32 trames par tour. Le formatage ne lit jamais plus de huit octets
et n'invente aucun payload pour une RTR. Le décodeur rejette les trames RTR,
étendues, hors 11 bits, ou trop courtes pour l'ensemble des champs configurés.
Les octets bruts restent visibles, y compris pour les IDs non configurés.
La sortie USB passe par un tampon de 64 Kio drainé sans attente de l'hôte.
Les écritures partielles sont reprises à l'octet suivant. Si le débit reste
insuffisant et que le tampon se remplit, seules des lignes nouvelles entières
sont refusées et comptées dans `ODB BUFFER`. Vérifier ces compteurs,
`ODB DRIVER` et les rejets du parseur avant d'exploiter une capture.

Pour séparer les journaux du dashboard dans un client, reconnaître les
préfixes exacts `RX 0x` et `ODB ` avant les réponses aux commandes. Le dashboard
est le bloc demandé par `SYS DATA`, de `=== Live Vehicle Data ===` à
`=========================` ; il n'existe pas de préfixe `DASH` ou `TX` seul.
Voir [le contrat de routage USB](protocols/USB_SERIAL_PROTOCOL.md#routing-the-shared-stream)
et [la validation du streaming firmware](LOG_STREAMING_VALIDATION.fr.md).

## Comparaison sur la VM

```bash
.venv/bin/python -B tools/analyze_can_log.py capture.log
.venv/bin/python -B tools/compare_obd_can.py capture.log \
  --preset data/NissanJukeF15.json --max-delay-ms 200 \
  --output-dir .local-validation/obd-comparison
```

Ajouter `--ecu 0x7E8` pour sélectionner un seul calculateur. L'outil conserve
les flux session/PID/calculateur/unité séparés, rapproche les trames horodatées les
plus proches et produit `comparison.json` ainsi que `matches.csv`. Il compare
les sorties du preset actif et plusieurs hypothèses, notamment RPM `/7`,
`/8` et les deux offsets de vitesse. Les PIDs sans candidat CAN restent
listés dans le rapport. Des unités différentes ne produisent pas d'erreur
numérique ; une capture constante ne permet pas de confirmer une échelle.
Les mappings ne sont jamais modifiés automatiquement. Les résultats candidats
service 22 sont distincts des références `ODB RX` service 01 : ces commandes
de comparaison ne valident pas la calibration physique des candidates Nissan
brutes. Conserver les `ODB PROBE_RX` et leurs trames brutes dans l'export complet.

Les messages de démarrage, échos et réponses aux commandes peuvent aussi
figurer dans l'export complet. Le lecteur les signale comme enregistrements
non reconnus ; lire les raisons de rejet pour les distinguer d'un ID, DLC ou
payload CAN invalide. Conserver les marqueurs de session dans le fichier.

Les changements `ODB STATE enabled=0/1` et les marqueurs de démarrage
séparent les sessions : aucune référence n'est rapprochée d'une autre session.
Un redémarrage sans marqueur ou des captures concaténées sans délimitation
doivent être analysés dans des fichiers séparés. Les références dont l'ECU
sort de `0x7E8–0x7EF`, ou dont l'adresse ne correspond pas à la requête
fonctionnelle `0x7DF` ou physique `ECU−8`, sont rejetées.

```bash
.venv/bin/python -B tools/compare_obd_can.py test/fixtures/obd_calibration_example.txt \
  --output-dir .local-validation/obd-example-comparison
.venv/bin/python -B -m unittest discover -s tools -p 'test_*can*.py' -v
.venv/bin/pio test -e native -e native_radio -e native_serial
```

Sur l'exemple synthétique, les hypothèses RPM `/8` et vitesse octets 4–5
coïncident avec les références OBD, tandis que le preset initial s'en écarte.
Cela vérifie le mécanisme de comparaison ; les anciennes trames sans
horodatage ni réponses OBD ne permettent pas cette calibration.
