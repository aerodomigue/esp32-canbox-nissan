# Validation du streaming CAN / OBD

## Périmètre et conclusion

Cette validation porte sur le firmware du commit `5a2a6ff`, base de l'image
OTA précédemment livrée, et sur le code du framework Arduino installé pour
l'ESP32-C3. Aucun fichier du client Android n'est nécessaire à ce contrôle.
Les essais hôte utilisent des périphériques simulés ; ils ne reproduisent
pas une capture matérielle du véhicule.

**La réception CAN ne dépend pas d'une réponse OBD. En revanche, les
écritures USB peuvent bloquer la boucle et perdre des lignes.** Le flux
actuel n'offre donc pas une garantie d'export de toutes les trames présentes
sur le bus.

## Chemin exécuté

Dans [la boucle principale](../src/main.cpp), chaque tour traite les commandes
série, puis retire jusqu'à 32 trames de la file CAN avec
`ESP32Can.readFrame(rxFrame, 0)`. Le timeout nul ne laisse pas la lecture CAN
attendre une nouvelle trame. Pour chaque trame reçue :

1. [Le handler CAN](../src/CanCapture.cpp) décode les champs configurés et
   formate la trame originale en `RX` ou `ODB RAW_RX` si `LOG ON` est actif.
2. [Le client OBD](../src/ObdCalibration.cpp) examine la même trame et émet,
   si nécessaire, une référence ou une réponse de découverte distincte.
3. Après le drainage, `tick(millis())` fait progresser les requêtes OBD.

La fenêtre OBD de 250 ms est un contrôle d'échéance : `tick()` retourne
immédiatement tant que la requête est en cours. Elle n'est ni un `delay()`,
ni une attente bloquante d'un calculateur. Les requêtes utilisent également
`ESP32Can.writeFrame(frame, 0)` : le résultat indique une tentative de mise
en file, pas un acquittement du véhicule.

Les logs CAN, les références OBD et les réponses `SYS DATA` sont écrits par
la même tâche, successivement. Il n'y a pas deux producteurs de logs CAN/OBD
qui écrivent simultanément leurs chaînes. Le CAN est déjà formaté dans
`char line[160]`, et les messages du client OBD dans `char line[256]`.

## Point de blocage et risque de troncature

La configuration `ARDUINO_USB_MODE=1` et `ARDUINO_USB_CDC_ON_BOOT=1` sélectionne
`HWCDC` pour `Serial`. Le paramètre 115200 de `Serial.begin()` ne limite pas
la liaison USB Serial/JTAG comme un UART physique.

Le framework **Arduino ESP32 3.3.12 installé lors du contrôle** contient :

- `cores/esp32/HWCDC.cpp` : tampon TX par défaut de **256 octets** et
  `tx_timeout_ms = 100` ; un envoi connecté peut répéter jusqu'à **20 attentes
  successives**, soit environ deux secondes par appel à `write()` sans
  progrès, puis retourner un nombre d'octets inférieur à celui demandé.
- `cores/esp32/Print.cpp` : `println(texte)` effectue un `write()` pour le
  texte, puis un autre pour `\r\n`. `printf()` transmet également son résultat
  formaté à `write()`.

Ces sources sont sous
`.pio-core/packages/framework-arduinoespressif32/` dans l'environnement de
compilation. Le comportement peut différer avec une autre version du
framework ; la plateforme `stable` du projet n'est pas une version figée.

Le firmware appelle directement `Serial.println()` et `Serial.printf()`
dans le chemin de réception et **ne contrôle pas les octets réellement
acceptés**. Pendant une attente USB, la tâche ne retire plus les nouvelles
trames CAN. La file TWAI de 128 trames peut alors déborder. Si une écriture
échoue ou est courte, le reste de la ligne ou sa terminaison n'est pas
réessayé : des lignes peuvent disparaître ou se retrouver concaténées.
Les réponses aux commandes peuvent subir le même problème.

Une réponse OBD déjà arrivée peut aussi rester dans la file jusqu'après
l'expiration de sa fenêtre logicielle. Elle reste visible en `ODB RAW_RX`
si l'export passe, mais elle ne donne alors pas de référence `ODB RX`.
Les timestamps sont pris au retrait de la file, avant l'écriture USB :
ils ne représentent pas l'instant où le client reçoit la ligne.

`ODB DRIVER` expose les compteurs de pertes et la taille de la file, mais
emprunte lui aussi l'USB. Son absence ou l'absence d'un compteur croissant
dans un export incomplet ne prouve pas une capture sans pertes.

## Essais exécutés

La suite [native_stream](../test/test_log_streaming/test_log_streaming.cpp)
exécute les véritables `setup()` et `loop()`, le handler CAN, le client OBD,
les commandes série, le décodeur JSON et l'envoi radio. Seuls les
périphériques, le stockage persistant et l'horloge sont simulés.

**Huit tests réussissent**, avec les résultats suivants :

| Scénario | Résultat |
| --- | --- |
| Initialisation du firmware | Preset Juke réel chargé, CAN à 500 kbit/s et file RX configurée à 128 trames |
| Aucun calculateur ne répond | 960 trames brutes traitées sur 30 tours, malgré trois timeouts OBD et le passage en attente de nouvelle découverte |
| 40 trames disponibles pendant une requête OBD | 32 retirées au premier tour, les huit restantes au suivant ; timeouts CAN RX et TX tous nuls |
| Trames configurées, inconnues, étendues, RTR et DLC 0 | Octets et flags conservés ; le décodeur rejette les formats qu'il ne prend pas en charge |
| Réponse OBD puis `SYS DATA` envoyé en deux fragments | Référence OBD 643 RPM séparée du CAN 182 RPM ; bloc dashboard conforme et aucun écho partiel ajouté au préfixe RAW |
| Écriture USB volontairement courte | Ligne RAW tronquée, aucun réessai ; compteurs CAN inchangés malgré la perte d'export |
| Texte accepté, CRLF refusé, puis reprise | Les lignes `RX` et `ODB RAW_RX` sont concaténées sur une seule ligne |
| Attente USB simulée de 500 ms, arrivée synthétique de 1 000 trames/s | La file de 128 déborde : 372 trames perdues ; drainage normal après reprise |

Les durées et nombres de pertes de ce dernier scénario sont des **valeurs
de simulation**, pas des mesures du Juke. Les tests de troncature et de
blocage caractérisent la limitation actuelle : leur succès signifie que
le défaut a été reproduit, pas qu'il a été corrigé.

Commande de reproduction :

```bash
.venv/bin/pio test -e native_stream
```

Les suites préexistantes donnent **151 tests C++ réussis et un test de replay
externe ignoré** (`CAN_LOG_PATH` non fourni). Les **34 tests Python**
d'analyse et de comparaison passent également. L'image OTA `5a2a6ff`
reste celle précédemment livrée ; cette branche ajoute des tests, leur
exécution en CI et la documentation du protocole.

## Format à transmettre au client

Le préfixe exact est **`ODB`**. Exemples synthétiques :

```text
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=1000 ext=0 rtr=0
ODB TX t_ms=1500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=1508 ext=0 rtr=0
ODB RX t_ms=1508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

| Reconnaissance exacte | Destination |
| --- | --- |
| `RX 0x` | Trame CAN brute reçue, ID connu ou inconnu |
| `ODB RAW_RX 0x` | Trame brute reçue sur les IDs diagnostic standard `0x7DF–0x7EF` |
| `ODB TX ` | Commande OBD, octets envoyés au driver et statut `queued` / `failed` |
| `ODB RX ` | Référence OBD décodée |
| Autre ligne commençant par `ODB ` | Informations et événements du diagnostic |
| `=== Live Vehicle Data ===` | Début du dashboard demandé par `SYS DATA` |
| `=========================` | Fin de ce bloc dashboard |
| `OK`, `OK ...`, `ERROR: ...` | Réponses aux commandes |

Il n'existe pas de famille `TX` seule pour toutes les transmissions, ni de
préfixe `DASH` pour chaque valeur dashboard. Les échanges UART avec
l'autoradio ne sont pas exportés par `LOG ON`. Pour chaque trame retirée de
la file avec `LOG ON` actif, le firmware tente un export sous `RX` ou
`ODB RAW_RX`, puis éventuellement une interprétation dans une deuxième ligne
diagnostic. Les références OBD ne remplacent jamais
les valeurs CAN du dashboard.

Un lecteur doit reconstituer les lignes sur `\n`, garder le suffixe incomplet
entre deux lectures USB et reconnaître les familles de logs avant d'ajouter
des champs à une réponse dashboard. Une lecture USB peut contenir plusieurs
lignes ou une fraction de ligne. Pour le détail des sous-types et des
réponses, voir [le protocole USB](protocols/USB_SERIAL_PROTOCOL.md#routing-the-shared-stream).

## Limite de cette validation

Le blocage USB est possible et explicable dans le firmware. Sans le fichier
du nouvel enregistrement et une mesure sur le boîtier, cette validation
n'attribue pas chaque ligne manquante de cet enregistrement à cette cause.
Une correction possible utiliserait une file de sortie bornée, un drainage
non bloquant, le suivi des écritures partielles et des compteurs de pertes.
Les commandes et l'OTA doivent partager une sérialisation cohérente
avec cette sortie ; agrandir seulement une chaîne de formatage ne suffit pas.
