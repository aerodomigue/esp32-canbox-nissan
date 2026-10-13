# Capture CAN et références OBD pour calibration

`LOG ON` active en mode véhicule réel deux fonctions : l'enregistrement de
toutes les trames lues, avec leurs octets bruts inchangés, et les lectures
OBD-II **service 01** pour disposer de références indépendantes. Chaque ligne
de diagnostic commence par **`ODB`**, selon le préfixe demandé. OBD-II est
le nom du protocole ; `ODB` est le libellé exact des lignes du journal. `LOG OFF`
arrête les nouvelles requêtes et la capture ; une transmission déjà mise en
file dans le contrôleur ne peut pas être annulée par cette commande.

Les lectures OBD ne changent ni le preset, ni les valeurs décodées envoyées à
l'autoradio. Les propositions de mapping de la PR #10 et leur profil
expérimental ont été retirés ; les corrections de robustesse restent actives.
Le preset actif reprend exactement la tête PR `495f346` : régime `/7`,
vitesse octets 0–1, carburant `[255,0,0,45]`, autonomie `0x54C ×100/338`,
consommation instantanée octet 3. Ce retour ne valide pas physiquement ces
anciens mappings ; la capture sert précisément à les vérifier.

## Utilisation

1. Charger le firmware et le preset actif dans LittleFS ou avec CANBox Manager,
   puis recharger le profil. Une OTA applicative seule ne remplace pas le JSON
   déjà installé sur l'ESP32.
2. Ouvrir la sortie série USB et enregistrer les lignes sans ajouter de
   préfixe d'heure ou de couleur au fichier. Pour le moniteur PlatformIO :
   `.venv/bin/pio device monitor --filter direct`.
3. Envoyer `LOG ON`. Attendre les lignes `ODB SUPPORTED`, puis les
   `ODB RX` correspondant aux paramètres disponibles.
4. Annoter les états réels : contact, démarrage, ralenti froid/chaud, régime
   affiché, jauge, vitesse et unités du combiné. Envoyer `LOG OFF` à la fin.

Le mode démonstration n'envoie aucune requête OBD. Le polling est suspendu
pendant une OTA et reprend par une nouvelle découverte si `LOG ON` reste actif.

## Paramètres et transport

Le client découvre les capacités avec les PIDs `00`, puis `20` et `40` si
le calculateur annonce les pages suivantes. Les réponses sont conservées
séparément pour les huit adresses possibles `0x7E8` à `0x7EF`. Les lectures
de mesure ciblent l'adresse physique `0x7E0` à `0x7E7` du premier calculateur
qui annonce le PID demandé. Aucun paramètre non annoncé n'est interrogé.

| PID | Référence | Conversion | Unité |
| --- | --- | --- | --- |
| `0C` | Régime | `(256×A+B)/4` | tr/min |
| `0D` | Vitesse | `A` | km/h |
| `05` | Température liquide de refroidissement | `A−40` | °C |
| `2F` | Niveau de carburant | `100×A/255` | % |
| `42` | Tension du calculateur | `(256×A+B)/1000` | V |
| `46` | Température ambiante | `A−40` | °C |
| `0B` | Pression absolue du collecteur | `A` | kPa |
| `5E` | Débit de carburant | `(256×A+B)/20` | L/h |

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
produit un timeout, jamais une valeur zéro inventée.

Le client accepte uniquement les réponses standard 11 bits, non RTR, de
DLC 8, sur une trame ISO-TP simple dont la longueur, le service et le PID
correspondent à la requête. Les réponses négatives sont distinctes. Les
messages multi-trames, services constructeur et commandes de contrôle ne
sont pas interprétés par ce client.

## Format du log

Extrait **synthétique**, disponible dans
`test/fixtures/obd_calibration_example.txt` :

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

- `RX` : trame CAN brute, ID/DLC/octets et flags inchangés.
- `ODB RAW_RX` : trame brute reçue dans la plage de diagnostic `0x7DF–0x7EF`.
- `ODB TX` : requête avec les octets exacts et le statut `queued` ou `failed`.
- `ODB SUPPORTED` : page de capacités reçue, bitmap et calculateur.
- `ODB RX` : référence décodée avec PID, calculateur, séquence, unité et latence.
- `ODB NEGATIVE`, `ODB TIMEOUT` : réponses négatives ou absence de réponse.
- `ODB STATE` : début/fin ou état de découverte.
- `ODB DRIVER` : pertes RX, échecs TX, occupation de la file, erreurs et état
  du contrôleur, au maximum une ligne par seconde.

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
L'USB peut encore ralentir la boucle : vérifier `ODB DRIVER` et les rejets
du parseur avant de tirer des conclusions d'une capture.

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
Les mappings ne sont jamais modifiés automatiquement.

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
