# Capture CAN et références OBD pour calibration

> **Langues :** **Français** | [English](OBD_CALIBRATION.md)

`LOG ON` active en mode véhicule réel deux fonctions : l'enregistrement de
toutes les trames lues, avec leurs octets bruts inchangés, et les lectures
OBD-II **service 01** pour disposer de références indépendantes. Avec le profil
nommé exactement `Nissan Juke F15`, quatre lectures candidates Nissan
**service 22** sont aussi tentées. Chaque ligne
de diagnostic commence par **`ODB`**, selon le préfixe demandé. OBD-II est
le nom du protocole ; `ODB` est le libellé exact des lignes du journal. `LOG OFF`
arrête les nouvelles requêtes et la capture ; une transmission déjà mise en
file dans le contrôleur ne peut pas être annulée par cette commande.

Les lectures OBD ne changent ni le preset, ni les valeurs décodées envoyées à
l'autoradio. La [capture CAN/OBD du 14 octobre et le mapping documenté](JUKE_F15_CAN_MAPPING.fr.md)
ont conduit à des corrections explicites dans le preset du dépôt : régime
`0x180 B0–B1 /8`, vitesse `0x284 B4–B5 /100`, frein à main `0x5C5 B0 bit2`,
contact de frein `0x180 B7 bit0` et compteur brut lié au carburant `0x551 B1`.
Il s'agit de champs CAN périodiques, pas de nouvelles commandes OBD. La
polarité du contact de frein et l'interprétation physique du compteur brut
restent à vérifier.

La jauge fondée sur l'octet entier `0x5C5` et les deux consommations `0x580`
ont été retirées. Dans le profil Juke, `FuelValid: 0` signale que la valeur
de compatibilité `Fuel: 0 L` est indisponible ; `FuelConsValid: INST=0 AVG=0`
empêche les trames UART de consommation. `FuelRaw` n'est ni des L/h ni des
L/100 km. Direction, température extérieure, kilométrage et autonomie
`0x54C` gardent leurs formules historiques à valider physiquement. Le profil
expérimental retiré de la PR reste historique ; les nouveaux changements
s'appuient sur les preuves véhicule citées.

La [recherche des commandes Nissan et le tableau de couverture](NISSAN_DIAGNOSTIC_RESEARCH.fr.md)
précisent les valeurs réellement vérifiables. Aucune lecture constructeur
supplémentaire n'a été validée pour le Juke 2011 MR16DDT manuel : portes,
direction, clignotants, éclairage, frein à main, consommation moyenne et
autonomie n'ont actuellement pas de référence OBD interrogée.

## Utilisation

1. Installer la **nouvelle OTA applicative** contenant le client OBD étendu.
   Cette mise à jour de capture ne nécessite pas de nouveau JSON : conserver
   le preset F15 existant et sélectionner le profil nommé exactement
   `Nissan Juke F15` pour activer les lectures candidates Nissan. Une OTA
   applicative ne remplace pas LittleFS.
2. Ouvrir la sortie série USB et enregistrer **toutes les lignes**, notamment
   `RX 0x`, `ODB RAW_RX`, `ODB RX`, `ODB PROBE_RX` et les erreurs diagnostiques,
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

Ces sondes sont des **lectures service `22`**, distinctes de la découverte
des PIDs standard. Elles sont activées seulement si le nom du profil actif
est exactement `Nissan Juke F15` et si le calculateur `0x7E8` a répondu à la
découverte des capacités Mode 01. Les requêtes ciblent alors l'adresse
physique `0x7E0` ; les autres calculateurs répondants ne sont pas utilisés
pour ces sondes. Ces lectures utilisent le cycle de polling standard et
nécessitent donc aussi qu'au moins un des 20 PIDs standard soit annoncé
par un calculateur répondant.

| DID | Fonction candidate | Donnée enregistrée | Unité |
| --- | --- | --- | --- |
| `111F` | Température d'huile candidate | premier octet de données `A` non signé, avec conversion candidate `A−50` | degC |
| `1114` | Jauge de carburant candidate | premier octet `A` non signé, sans conversion en pourcentage | raw |
| `1183` | Rapport engagé candidat | premier octet `A` non signé, sans énumération des rapports | raw |
| `1304` | Embrayage candidat | premier octet `A` non signé, sans interprétation booléenne/par bit | raw |

La première sonde utilise un créneau normal après découverte. Ensuite, au
maximum une sonde utilise un créneau normal toutes les **10 secondes**.
Les candidates tournent ; une réponse positive permet de nouvelles lectures.
Un timeout ou une réponse négative terminale désactive ce DID pour la session
`LOG ON` en cours ; une réponse positive ultérieure ne lève pas un refus
terminal. Le NRC `0x78` (réponse en attente) peut être suivi d'une réponse
positive dans la fenêtre initiale de 250 ms, sans la prolonger. Sans réponse
positive, la tentative expire et le DID est désactivé pour cette capture.
Un échec local de mise en file TX ne désactive pas le DID : il reste éligible
à la rotation espacée de 10 secondes. Relancer la capture permet un nouvel
essai. Les lectures standard continuent indépendamment.

Aucune ouverture de session `10 C0`, fermeture de session `10 81`, commande
d'actionneur, adaptation ou effacement de défaut n'est envoyée. Certaines
définitions Nissan publiées utilisent une autre session diagnostique : un
refus ou un silence ici ne prouve pas l'absence de la sonde dans la voiture.
Même une réponse positive ne valide pas physiquement le libellé ou la formule
de température d'huile sur ce MR16DDT manuel ; conserver les octets exacts
pour la vérification ultérieure. La jauge reste brute car les conversions
publiées divergent.

Des PIDs standard peuvent être absents du bitmap ; les sondes candidates
peuvent être refusées ou expirer. Ces absences sont possibles et ne sont
pas des mesures à zéro. Les lectures ne modifient ni le preset, ni le
dashboard, ni les données envoyées au poste et ne calibrent aucun mapping
CAN automatiquement.

Le client accepte uniquement les réponses standard 11 bits, non RTR, de
DLC 8, sur une trame ISO-TP simple dont la longueur, le service et le PID
ou DID correspondent à la requête en cours. Les réponses négatives sont
distinctes. Les messages multi-trames, autres identifiants constructeur et
commandes de contrôle ne sont pas interprétés par ce client.

## Format du log

Extrait **synthétique**, disponible dans
`test/fixtures/obd_calibration_example.txt` :

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

Autre exemple **synthétique**, montrant la lecture candidate distincte :

```text
ODB TX t_ms=2000 seq=5 can_id=0x7E0 service=0x22 did=0x111F status=queued data=03 22 11 1F 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 1F 56 00 00 00 | t_ms=2010 ext=0 rtr=0
ODB PROBE_RX t_ms=2010 seq=5 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x111F name=nissan_oil_temperature_candidate raw=0x56 value=36.000 unit=degC latency_ms=10
```

Les autres noms candidats sont `nissan_fuel_level_raw` (`1114`),
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
  `ODB TX`, qui conservent les octets envoyés exacts. Les formats service 01
  et les préfixes `RX 0x` / `ODB RAW_RX` restent inchangés.
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
