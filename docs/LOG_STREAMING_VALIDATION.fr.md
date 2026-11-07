# Streaming CAN / OBD avec tampon USB

## Fonctionnement

La sortie USB utilise maintenant un tampon circulaire fixe de **64 Kio**,
partagé par les logs CAN/OBD, les réponses aux commandes et les messages de
configuration. [UsbConsole](../src/UsbConsole.cpp) est le seul code applicatif
qui écrit dans le pilote USB. Il est appelé depuis la tâche principale ; aucun
thread concurrent n'est ajouté.

Chaque ligne `RX` ou `ODB` est copiée avec son CRLF en une seule admission
logique. Le drainage conserve la position exacte du prochain octet à envoyer :
si le pilote n'accepte qu'un préfixe, le suffixe reste devant toutes les lignes
suivantes. Une réponse `SYS DATA` ou OTA ne peut donc pas s'insérer au milieu
d'une ligne déjà commencée.

Le pilote HWCDC est configuré avant son démarrage avec **4 Kio de tampon TX**,
**1 Kio de tampon RX** et un timeout d'écriture de **zéro**. Le drainage :

- ne tente aucune écriture si la connexion ou l'espace disponible manque ;
- transmet au maximum l'espace annoncé par le pilote ;
- avance uniquement du nombre d'octets réellement acceptés ;
- s'arrête immédiatement sur un retour nul ;
- traite au plus **4 096 octets et huit écritures par appel**.

Il est appelé au début et à la fin de la boucle, y compris sur le chemin OTA.
Aucun `Serial.flush()` bloquant n'est utilisé. Les pauses précédant un
redémarrage volontaire servent aussi à drainer la file, pendant une durée
bornée ; le délai OTA de deux secondes reste appliqué.

## Acquisition, commandes et saturation

La réception CAN reste indépendante des réponses OBD : jusqu'à 32 trames sont
retirées par tour, avec un timeout nul et une file TWAI de 128 trames. La
fenêtre OBD de 250 ms est un contrôle d'échéance, pas une attente bloquante.
Le client OBD conserve ses cadences et ses conversions ; les références ne
remplacent pas les valeurs du preset envoyées au dashboard et à l'autoradio.

Le tampon réserve **12 Kio pour les réponses aux commandes**, ainsi que
**512 octets supplémentaires aux diagnostics** par rapport aux logs RAW.
Les commandes sont traitées une par tour. Une commande complète reste en RAM
jusqu'à ce que la réserve soit disponible, pendant que la réception CAN
continue. Les réponses sont assemblées avant leur émission. Une réponse trop
grande pour l'espace disponible, ou une allocation de formatage impossible,
est remplacée intégralement par :

```text
ERROR: Response exceeds USB buffer or allocation failed
```

Cette erreur concerne la sortie : une commande ayant des effets de bord peut
avoir été exécutée. Les octets des anciennes lignes déjà en cours d'envoi ne
sont jamais retirés pour fabriquer cette réponse.

Si l'hôte reste trop lent pour la quantité reçue, toute mémoire finie finit par
se remplir. Dans ce cas, une **nouvelle ligne entière** est refusée avant son
insertion et comptée ; aucun ancien octet en attente n'est écrasé. La lecture
CAN et la mise à jour des valeurs du véhicule continuent. Les compteurs de
saturation USB sont distincts des pertes matérielles/driver de `ODB DRIVER`.

`LOG OFF` arrête les nouvelles captures et requêtes. Les lignes déjà admises
restent dans l'ordre avant la réponse à cette commande. L'OTA suspend les
nouveaux logs ; les anciennes lignes précèdent `OK READY` dans la file. Les
accusés de données OTA sont ajoutés après l'écriture de leur bloc en flash.

## Format du retour

Les préfixes, champs et octets des lignes existantes sont conservés. Le préfixe
diagnostic exact reste **`ODB`**. Exemples synthétiques :

```text
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=1000 ext=0 rtr=0
ODB TX t_ms=1500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=1508 ext=0 rtr=0
ODB RX t_ms=1508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
ODB BUFFER t_ms=2000 queued_bytes=174 dropped_raw=0 dropped_odb=0 short_writes=0 rejected_responses=0
```

`ODB BUFFER` est une nouvelle ligne diagnostic, au plus une fois par seconde
en mode réel avec `LOG ON` :

| Champ | Signification |
| --- | --- |
| `queued_bytes` | Octets restant dans la file logicielle à cet instant, avant insertion de cette ligne |
| `dropped_raw` | Lignes RAW refusées faute de place ou dépassant la longueur admise |
| `dropped_odb` | Lignes diagnostic refusées dans les mêmes conditions |
| `short_writes` | Écritures ayant accepté moins d'octets que demandé ; leur suffixe est conservé |
| `rejected_responses` | Réponses refusées, notamment pour dépassement du tampon ou allocation impossible |

Les compteurs sont cumulatifs depuis le démarrage. Une écriture partielle
n'est pas en elle-même une perte. Une indisponibilité de l'USB diffère aussi
d'une déconnexion physique : aucun protocole d'acquittement par ligne ne
confirme les octets déjà acceptés par le pilote avant un débranchement.

| Reconnaissance exacte | Destination |
| --- | --- |
| `RX 0x` | Trame CAN brute reçue, ID connu ou inconnu |
| `ODB RAW_RX 0x` | Trame brute reçue sur les IDs diagnostic standard `0x7DF–0x7EF` |
| `ODB TX ` | Requête OBD et statut de mise en file `queued` / `failed` |
| `ODB RX ` | Référence OBD décodée |
| Autre ligne commençant par `ODB ` | Informations diagnostic, dont `ODB BUFFER` |
| `=== Live Vehicle Data ===` | Début du dashboard demandé par `SYS DATA` |
| `=========================` | Fin de ce bloc dashboard |
| `OK`, `OK ...`, `ERROR: ...` | Réponses aux commandes |

Il n'existe pas de famille `TX` seule pour toutes les transmissions, ni de
préfixe `DASH` pour chaque valeur dashboard. Les échanges UART avec
l'autoradio ne sont pas exportés par `LOG ON`. Les trames reçues sont admises
sous `RX` ou `ODB RAW_RX`, puis éventuellement interprétées dans une deuxième
ligne diagnostic. Les timestamps viennent du retrait de la file CAN, avant
l'attente éventuelle dans le tampon USB ; ce ne sont pas des timestamps
matériels ou des heures de réception par le client.

Le lecteur conserve les fragments USB jusqu'au `\n`, accepte LF/CRLF et route
les familles de logs avant d'accumuler des champs dashboard. Un fragment USB
peut contenir plusieurs lignes ou une partie de ligne. Voir le
[contrat complet de routage](protocols/USB_SERIAL_PROTOCOL.md#routing-the-shared-stream).

## Vérification

La suite [native_stream](../test/test_log_streaming/test_log_streaming.cpp)
exécute les véritables `setup()` et `loop()`, le handler CAN, le client OBD,
les commandes série, le décodeur JSON et l'envoi radio. Les périphériques,
le stockage persistant et l'horloge sont simulés.

Les scénarios couvrent notamment : 960 trames sans réponse OBD, 500 trames
reçues pendant une indisponibilité USB de 500 ms, les écritures partielles,
la coupure juste avant CRLF, le bouclage du tampon circulaire, les refus de
lignes entières à saturation, les commandes différées, les réponses trop
grandes (y compris la séparation entre écho et erreur) et les accusés OTA
jusqu'au redémarrage. Les tests OTA utilisent
une fixture de décodage Base64 ; ils vérifient le transport et l'ordre des
réponses, pas le décodeur Base64 ni une écriture physique en flash.

```bash
.venv/bin/pio test -e native -e native_radio -e native_serial -e native_stream
.venv/bin/python -B -m unittest discover -s tools -p 'test_*can*.py' -v
.venv/bin/pio run -e esp32-c3-devkitm-1
```

Résultat de la validation locale : **170 tests C++ réussis**, dont **18 tests
de streaming**, et **34 tests Python réussis**. La relecture complète de
l'export historique est incluse via `CAN_LOG_PATH`.

Ces essais sont des simulations logicielles. Aucun test sur le Juke ou une
carte ESP32 connectée n'est réalisé sur la VM. Le défaut antérieur, reproduit
sur `5a2a6ff` et documenté dans le commit de validation `70622fb`, provenait
d'appels directs à HWCDC depuis la capture, avec des retours courts ignorés.
