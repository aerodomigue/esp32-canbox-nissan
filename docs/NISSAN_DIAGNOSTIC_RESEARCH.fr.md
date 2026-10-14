# Diagnostic Nissan — Juke F15 2011, 1.6 DIG-T, boîte manuelle

> **Langues :** **Français** | [English](NISSAN_DIAGNOSTIC_RESEARCH.md)

Recherche effectuée le **14 octobre 2026** pour compléter les références
`ODB` de calibration. Véhicule cible : moteur **MR16DDT**, boîte manuelle.

## Résultat pour le firmware

**Aucune nouvelle commande constructeur retenue.** Les sources publiques
consultées donnent des pistes précises, mais aucune ne fournit une preuve
suffisante pour ajouter les mesures manquantes sur ce véhicule. Cela ne
signifie pas que CONSULT ne peut pas les lire : il manque la correspondance
vérifiée entre le calculateur du véhicule, la commande et les octets retournés.

Le firmware conserve les huit lectures service `01` et la découverte de
leurs capacités. Aucun service `21`/`22`, changement de session, scan de DIDs
ou mapping CAN supplémentaire n'est activé par cette recherche. Le format des
logs, le tampon USB et les valeurs du dashboard restent identiques.

## Couverture réelle de la liste à vérifier

Une lecture service `01` n'est envoyée que si un calculateur annonce le PID.
Une absence de réponse produit un timeout, jamais une valeur zéro inventée.

| Valeur | Référence interrogée actuellement | Limite pour la comparaison |
| --- | --- | --- |
| Régime moteur | `01 0C`, tr/min | Disponibilité annoncée par le calculateur |
| Vitesse | `01 0D`, km/h | Résolution de 1 km/h |
| Niveau de carburant | `01 2F`, % | Ne valide pas directement des litres |
| Tension batterie | `01 42`, V | Tension du calculateur, pas mesure aux bornes de la batterie |
| Angle du volant | Aucune | Mapping constructeur non vérifié pour ce Juke |
| Portes | Aucune | Mapping constructeur non vérifié pour ce Juke |
| Clignotants | Aucune | Distinguer commande du commodo et état des lampes |
| Éclairage | Aucune | Distinguer sélection manuelle, AUTO et état des lampes |
| Frein à main | Aucune | Ne pas utiliser le contact de la pédale de frein |
| Température extérieure | `01 46`, °C | Disponibilité annoncée par le calculateur |
| Consommation instantanée | `01 5E`, L/h | Débit de carburant ; aucune conversion automatique en L/100 km |
| Consommation moyenne | Aucune | Pas de commande vérifiée pour la valeur affichée au combiné |
| Autonomie restante | Aucune | Pas de commande vérifiée pour la valeur affichée au combiné |

Les deux autres lectures existantes sont la température du liquide de
refroidissement (`01 05`) et la pression absolue du collecteur (`01 0B`).
Implémentation : [ObdCalibration.cpp](../src/ObdCalibration.cpp).
Les trames CAN lues restent capturées brutes, même sans référence OBD décodée.
Le frein à main n'a pas non plus de champ dans le preset actuel.

La [nouvelle étude des trames périodiques](JUKE_F15_CAN_MAPPING.fr.md) identifie
néanmoins son bit CAN `0x5C5[0] & 0x04`, corroboré par la manœuvre du propriétaire.
Une lecture CAN périodique et une commande de diagnostic sont deux preuves
différentes. Les captures récentes n'interrogent que quatre des huit PIDs :
`0C`, `0D`, `05` et `42`. Le MAF `10` et le rapport commandé `44` sont annoncés
par l'ECU mais ne figurent pas dans la liste actuelle de lectures.

## Sources et pistes examinées

### Réponses publiées pour un Juke 2012

Le [dépôt OBDb Nissan-Juke, révision examinée](https://github.com/OBDb/Nissan-Juke/tree/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf)
a un fichier `signalsets/v3/default.json` sans commandes propres au modèle,
mais contient des **fixtures de réponses 2012** et une liste de commandes
prises en charge. Un fichier de définition vide ne signifie donc pas une
absence totale de données. Aucun dossier de tests 2011 n'est présent dans
cette révision.

Les fixtures moteur utilisent notamment `7E0` → `7E8`. Elles ne précisent
pas le moteur, la boîte et le marché nécessaires pour certifier notre
configuration. Les entrées annotées `din=C0,dout=81` comportent aussi une
configuration de sessions diagnostiques ; le client actuel ne l'implémente pas.

Deux confusions à éviter :

- [`22 13 05`](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/commands/7E0.7E8.221305%7Cdin%3DC0%2Cdout%3D81.yaml)
  est un contact de **pédale de frein**, pas une référence de frein à main.
- [`22 12 2B`](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/commands/7E0.7E8.22122B.yaml)
  concerne la vitesse de consigne du régulateur, pas l'autonomie.

Les autres fixtures constructeur concernent principalement tension,
avance à l'allumage, purge EVAP et températures. Elles ne complètent pas les
références manquantes de la liste ci-dessus. Voir la
[liste 2012](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/command_support.yaml).
Les commandes de transmission `7E1` → `7E9` ne sont pas retenues pour notre
boîte manuelle.

### Définitions Nissan génériques

Le [jeu OBDb Nissan, révision c322e4d](https://github.com/OBDb/Nissan/blob/c322e4de23fa3564c9dd384ef9fc1030b653d646/signalsets/v3/default.json)
contient notamment les candidats suivants, sans fixture Juke correspondante
dans la révision examinée :

| Adresse TX → RX | Commande | Interprétation proposée par la source | Décision |
| --- | --- | --- | --- |
| `740` → `760` | `22 12 08` | Angle de direction ABS, signé 16 bits /10 degrés | Applicabilité Juke 2011 non établie |
| `7E0` → `7E8` | `22 20 79` | Plusieurs états dont frein de stationnement | Applicabilité MR16DDT non établie |
| `7E0` → `7E8` | `22 11 86` | Carburant restant en litres | Applicabilité et échelle non confirmées sur ce véhicule |

Un identifiant seul ne suffit pas : dans ce même jeu, `22 12 08` adressé au
moteur `7E0` décrit une durée d'injection, alors que l'adresse `740` lui donne
le sens d'un angle. Une future sélection devra identifier **TX, RX, service,
identifiant, session et variante de calculateur** ensemble.

### Mesures publiées sur un Qashqai J10

L'[étude de balrog-kun, révision b1ffbe2](https://github.com/balrog-kun/nissan-qashqai-can-info/blob/b1ffbe27a60e8808539521cfd3a00959cbf791e9/README.md#bcm-non-standard-service-21-pids)
repose sur un **Qashqai diesel manuel 2013**. Elle décrit le BCM
`745` → `765`, avec une session `10 C0` avant les lectures `21` :

- `21 41` : commutateurs, notamment clignotants et pédale de frein ;
- `21 42` / `21 44` : états de portes et autres entrées ;
- `21 49` / `21 4A` : commodo et sélecteurs d'éclairage.

Le sélecteur d'éclairage n'est pas équivalent à l'état réel des lampes en
mode AUTO. La lecture du combiné adressé en `743`, `21 01`, retourne un bloc
long : elle nécessiterait aussi un transport ISO-TP multi-trame. L'étude
n'établit pas les mêmes offsets sur le Juke ; ces commandes restent des
pistes de recherche, pas des références activées.

### Documentation constructeur et fournisseur

Une [copie du manuel Nissan **2011 JUKE**, section EXL, révision septembre 2010](https://www.scribd.com/document/1016834926/EXL-JUKE-F15-SEP-10)
liste les moniteurs de portes, de commodo et d'éclairage. La page EXL-22
mentionne aussi `PKB SW`, reçu du combiné par CAN, dans la variante sans
Intelligent Key. Le frein à main est donc bien documenté comme observable
par CONSULT sur cette variante ; son absence du firmware ne signifie pas
son absence du bus. Ces pages ne donnent pas les commandes ni les offsets.

Le manuel Nissan Juke 2012, section
[BCS, Data Monitor](https://www.nicoclub.com/manuals/Juke/2012/BCS.pdf),
décrit les fonctions consultables du BCM. Il ne donne pas la combinaison
adresse/service/identifiant/offset permettant de reproduire ces lectures.
Son millésime reste distinct de 2011.

La section [BRC du Juke 2012 avec VDC](https://www.nicoclub.com/service-manual?fsm_download=Juke%2F2012%2FBRC.pdf),
pages BRC-40, 44 et 57, décrit aussi le moniteur d'angle de direction ABS.
Elle fournit des valeurs de contrôle en degrés, mais pas le service,
l'identifiant ou les octets nécessaires à son interrogation.

La [fiche Racelogic Juke F15 2011–2018](https://www.racelogic.co.uk/_downloads/vbox/Vehicles/Other/Docs/Nissan-Juke%20%28F15%29%202011-2018.pdf)
liste plusieurs signaux recherchés, dont frein à main, portes, éclairage et
consommation. Cette liste confirme une couverture annoncée par le fournisseur,
pas des commandes diagnostiques publiques ni leurs formules.

**Complément du 14 octobre :** la [page VBOX publique](https://vboxautomotive.co.uk/en/can-database-documents)
donne aussi accès au fichier REF Juke avec IDs, champs et facteurs des messages
CAN périodiques. Le [mapping documenté](JUKE_F15_CAN_MAPPING.fr.md) confronte
ces définitions aux captures, notamment pour le frein à main et la vitesse.
La fiche PDF seule ne contenait pas ces offsets ; le REF ne fournit toujours
pas les commandes CONSULT correspondant aux signaux.

## Ce qui permettrait une intégration vérifiable

Il faut une capture de diagnostic sur le véhicule cible, associée à la
référence du calculateur et aux valeurs affichées par l'outil pendant les
changements d'état. Elle doit conserver les requêtes, les réponses, les
sessions et les échanges ISO-TP éventuels. Une réponse positive isolée ne
prouve ni la signification ni l'échelle d'une mesure.

La capture de cet échange doit être passive. **`LOG ON` n'est pas un mode
passif**, car il lance déjà les lectures service `01` ; ne pas le présenter
comme un simple observateur d'une session CONSULT. Le firmware actuel
n'offre pas de commande distincte de capture seule.

Une fois une commande étayée, son décodage pourra être testé avec des réponses
réelles, des états opposés, des longueurs incorrectes et des timeouts, avant
activation dans les logs `ODB`. Les valeurs actuelles du dashboard ne
constituent pas une preuve indépendante de calibration.

## Validation de ce bilan

Les suites existantes ont été exécutées le 14 octobre 2026 : **170 tests C++
réussis**, dont 18 de streaming et la relecture complète du log historique,
et **34 tests Python réussis**. Aucun code firmware ou preset n'est modifié.
Ces résultats vérifient le comportement logiciel existant ; ils ne constituent
pas des essais des commandes candidates sur la voiture.
