# CAN F15 vers DUDU RAV4 : unités et trajet depuis le démarrage

> **Langues :** **Français** | [English](F15_DUDU_BOOT_TRIP_MAPPING.md)

Révision du **7 novembre 2026**. Ce guide décrit l'implémentation actuelle et
remplace les anciens exemples de preset concernant vitesse, autonomie,
températures et durée de vie du trajet. Les rapports précédents conservent leurs
preuves historiques et leurs limites. Le véhicule du propriétaire est un **Juke
F15 2011, MR16DDT 1.6 DIG-T manuel**, raccordé aux broches OBD 6/14. L'ESP32
traduit son CAN Nissan vers le **protocole UART Toyota RAV4** du poste ; il
n'émet pas de trames CAN de véhicule Toyota.

## Sources et niveau de confirmation

Les octets sont numérotés à partir de zéro. `BE`/`LE` décrivent l'entier extrait
avant conversion dans l'unité physique.

| Grandeur | Source F15 | Conversion physique | Qualification |
| --- | --- | --- | --- |
| Vitesse | `0x280 B4..B5`, U16BE | brut / 100 km/h | Distance du long trajet cohérente avec les incréments du compteur ; remplace `0x284` |
| Kilométrage total | `0x5C5 B1..B3`, U24BE | brut en km | Les 247 498 km de la capture contrôlée correspondent aux photos |
| Régime | `0x180 B0..B1`, U16BE | brut / 8 tr/min | Recoupé avec l'OBD |
| Liquide de refroidissement | `0x551 B0`, U8 | brut − 40 °C | Confirmé par le propriétaire et l'OBD |
| Huile moteur | `0x580 B4`, U8 | brut − 40 °C | **Provisoire**, activée explicitement par le propriétaire ; évolution thermique plausible, identité physique sans confirmation indépendante |
| Autonomie restante | `0x2DE B6..B7`, U16BE | brut / 10 km | Fortement corroborée par la capture contrôlée et les photos |
| Incréments carburant | `0x551 B1`, U8 modulo 256 | delta × 80 mm³ | **Calibration volumique provisoire** ; évolution cohérente avec charge et décélération |
| Moyenne historique Nissan | `0x2DE B3..B4`, U16BE | 16000 / brut L/100 km, brut > 0 | Corroborée par les 9,9 photographiés ; référence documentaire, **pas la moyenne depuis démarrage envoyée à DUDU** |

Le brut `0xFFFF` est exclu pour vitesse et régime, `0xFF` pour le liquide.
Le moniteur MWI Nissan décrit les valeurs converties 655,35 km/h, 8191,875 tr/min
et 215 °C comme des indications de défaut. Le brut DTE `0xFFFF` est également
exclu comme indisponible. L'exclusion intervient **avant** conversion.
`estimated: true` indique le caractère provisoire de l'huile ; ce marqueur
n'améliore pas sa précision. Aucun marqueur invalide équivalent n'est établi
ici pour cette source d'huile.

L'ancienne température extérieure `0x540 B0−40` est retirée. Aucun coefficient
n'est inventé à partir du seul point photographié de 14 °C. Sans source fiable
de quantité de carburant, aucune jauge numérique n'est fabriquée. Les mappings
fonctionnels des portes, coffre, clignotants, feux et direction sont conservés.
La marche arrière utilise un fil dédié au poste dans cette installation ; son
identification CAN n'est donc pas nécessaire au déclenchement de la caméra.

## Consommation et durée de vie du trajet

Le trajet appartient à **un démarrage du boîtier**, en RAM uniquement, sans
sauvegarde NVS. `LOG ON`/`LOG OFF` ne le lancent pas et ne le réinitialisent pas.
Un rechargement conservant la calibration et les sources de l'estimateur préserve
ses cumuls, son dernier compteur et sa fenêtre instantanée. Un redémarrage crée
un nouveau trajet. Une modification de source/calibration après le début des
mesures ne doit pas créer discrètement une nouvelle moyenne présentée comme
couvrant tout le démarrage.

L'estimateur utilise des échantillons appariés et frais de compteur carburant et
de vitesse fractionnaire :

```text
delta compteur = (courant − précédent) modulo 256
volume en mm³ = somme des deltas × mm³ par incrément
distance en m = intégrale(vitesse en km/h × durée en ms / 3600)
moyenne L/100 km = volume en mm³ / (10 × distance en m)
valeur UART en 0,1 L/100 km = volume en mm³ / distance en m
```

Le carburant consommé au ralenti entre dans la moyenne. Celle-ci demande au
moins **100 m**, des sources fraîches et un trajet complet. L'instantané utilise
une fenêtre configurée de **1 s**. Sous **2 km/h**, sa valeur fraîche affichée est
zéro : c'est une convention d'affichage, pas un débit de carburant nul. Les
**0,97 L/h** estimés au ralenti servent de référence théorique indépendante ;
ce n'est ni l'unité envoyée au poste, ni une constante appliquée en roulant.

Une interruption des sources de plus de **500 ms**, une donnée invalide, une
progression invraisemblable du compteur ou un changement matériel de source/
calibration rend le trajet incomplet jusqu'au prochain démarrage. Les quantités
appariées déjà acquises restent en RAM. L'instantané peut reprendre avec des
données fraîches ; la moyenne de tout le démarrage ne repart pas discrètement
sur une autre période. Un reset du compteur ressemblant à son débordement normal
n'est pas toujours détectable. Le carburant consommé avant la première paire
exploitable ne peut pas être reconstruit.

Le rejeu du trajet du 17 octobre donne **14,942555 km**, **1,20552 L** avec
80 mm³/incrément, donc environ **8,07 L/100 km**, encodés en **8,1 L/100 km**.
Cela corrobore le comportement sans étalonner indépendamment les litres.
Le régime seul ne détermine pas la consommation : en décélération, il peut
rester élevé tandis que l'injection et les incréments deviennent nuls.

La vitesse moyenne est la distance divisée par le **temps depuis démarrage,
arrêts inclus**. Le temps de trajet est l'uptime du boîtier, converti des secondes
en **minutes entières** pour l'UART. Les trajets virtuels A et B reproduisent
la même distance intégrée depuis démarrage : **ce ne sont pas les compteurs A/B
Nissan**, ni deux historiques avec remise à zéro indépendante. Un intervalle de
distance perdu invalide ces totaux plutôt que masquer un trajet incomplet.

## Sélection du décodeur et contrat UART

La racine du preset choisit explicitement :

```json
"radioReceiver": "DUDU"
```

En son absence, le comportement générique **FYT** reste sélectionné. DUDU et FYT
n'attribuent pas toujours le même sens à une valeur indisponible. Le poste reste
configuré sur son profil CAN Toyota RAV4.

Enveloppe à **38400 bauds, 8N1** : `2E CMD LEN PAYLOAD CHECKSUM`.
`CHECKSUM = (CMD + LEN + somme(PAYLOAD)) XOR FF`, limité aux huit bits bas.
Les offsets ci-dessous portent sur le payload ; le sous-type `7D` est lui-même
`D0` et compte dans `LEN`. Les nombres sont non signés sauf indication contraire.

| Commande | Longueur | Payload et unités |
| --- | --- | --- |
| `7D / 03` | 6 | `D0=03` ; `D1..2` vitesse U16LE en **0,01 km/h** ; `D3..4` vitesse moyenne depuis démarrage U16LE en **0,01 km/h** ; `D5=00` métrique |
| `7D / 04` | 12 | `D0=04` ; `D1..3` kilométrage total U24LE en **km** ; `D4..5=F2 08`, anciens octets conservés sans signification validée ; `D6..8` trajet virtuel A U24LE en **0,1 km** ; `D9..11` trajet B, même valeur/unité |
| `7D / 0A` | 3 | `D0=0A` ; `D1..2` U16LE = **régime × 4** |
| `21` | 7 | `D0..1` vitesse moyenne U16BE en **0,1 km/h** ; `D2..3` temps U16BE en **minutes** ; `D4..5` autonomie U16BE en **km** ; `D6=02` métrique |
| `22` | 3 | `D0=02`, L/100 km ; `D1..2` consommation instantanée U16BE en **0,1 L/100 km** |
| `23` | 13 | `D0=02` ; `D1..2` moyenne du trajet U16BE en **0,1 L/100 km** ; `D3..12` cinq historiques indisponibles, chacun `FFFF` |
| `7D / 09` | 4 | `[09, eau+40, eau+40, huile+40]` ; chaque température est en **U8, décalage −40 °C** |
| `29` | 2 | Unités de guidage calibrées, signé 12 bits LE, limitées à **−500..+500** ; ce ne sont pas les dixièmes de degré Nissan bruts |

Le liquide est dupliqué parce que FYT lit `D2`, tandis que DUDU lit `D1` pour
l'eau et `D3` pour l'huile. Un paquet de trois octets est trop court pour l'huile
DUDU et peut lui faire lire le checksum comme une température. Le paquet de
quatre octets n'est envoyé que si **les deux sources configurées sont fraîches**
(moins de 2 s) et représentables. L'huile n'est jamais remplacée par une copie
de l'eau. Un décodeur DUDU neuf démarre en Celsius, mais `7D/09` ne réinitialise
pas un ancien sélecteur Fahrenheit. Si un précédent boîtier l'a laissé actif,
redémarrer le décodeur/poste ou réinitialiser son profil RAV4 avant de vérifier
l'affichage. Aucun paquet de climatisation `0x28` non validé n'est émis simplement
pour forcer ce bit : il modifierait aussi d'autres champs HVAC. Aucune
température extérieure n'est envoyée sans source valide.

Pour DUDU, `FFFF` dans la vitesse moyenne ou l'autonomie signifie **ignorer le
champ**, et non effacer l'ancienne valeur. Cela permet de continuer à actualiser
la vitesse ou l'autonomie fraîche lorsque l'autre champ est inconnu. En FYT
générique, les paquets sont retenus lorsqu'un champ associé nécessaire manque.
Une consommation courante invalide n'est pas envoyée : DUDU afficherait `FFFF`
comme 6553,5 pour l'instantané et les décodeurs de moyenne n'ont pas de sentinelle
d'effacement commune. Les historiques restent indisponibles. Le temps est limité
à 65534 minutes pour éviter un débordement. Un champ retenu peut laisser sa
précédente valeur visible au poste.
Le consommateur Toyota `m3` de DUDU examiné ignore les octets de temps de `0x21` :
encoder des minutes correctes ne prouve pas leur affichage sur cet écran DUDU.
La visibilité des cartes de température dépend aussi de la présentation véhicule
sélectionnée.

Le paquet kilométrage/trajets exige une source kilométrique fraîche et un trajet
complet. DUDU peut tronquer l'affichage texte A/B aux kilomètres entiers malgré
la résolution UART de 0,1 km. Les états de validité et de calibration de `SYS DATA`
ne sont pas ajoutés comme bits supplémentaires dans le protocole Toyota.

Dans le build audité, les profils RAV4 standard `393328` (config16) et `786544`
(config15) autorisent les cartes eau/huile ; RAV4 AMP `458864` (config18) les
exclut. Envoyer sa valeur UART ne suffit pas à rendre visible une carte exclue.

## Informations non fabriquées

- **Extérieur et quantité dans le réservoir :** pas de source F15 validée.
- **Frein à main UART :** décodage CAN et `SYS DATA` disponibles. `7D/06` modifie
  aussi un état de ceinture non confirmé ; cette modification ne l'envoie pas.
- **Carte tension externe :** l'application DUDU examinée utilise une mesure MCU
  interne. Décoder la tension Nissan ne prouve pas l'existence d'une commande
  Toyota UART externe pour actualiser cette carte.
- **Alertes, ceinture, anciens trajets inconnus :** aucune valeur synthétique
  n'est présentée comme une mesure du véhicule.

## Essai diagnostique Nissan borné

Le diagnostic fournit des références de capture, sans remplacer le preset ou
les valeurs radio. Le service standard `01` ne demande que les PIDs annoncés.
Dans le profil F15 exact en `LOG ON` réel, le chemin moteur est TX physique
`7E0`, RX `7E8`, une requête en attente, 500 ms entre tentatives et fenêtre de
250 ms. Chaque groupe est tenté une seule fois par `LOG ON`, avec au moins
10 s entre débuts de groupe :

```text
10 C0 → 22 11 01 → 22 11 1F → 10 81
10 C0 → 22 11 14 → 22 11 83 → 10 81
10 C0 → 22 13 04             → 10 81
```

Seul un `50 C0` positif correspondant autorise les lectures. `1101` est un
contrôle eau candidat et `111F` une huile candidate : U8−50 °C après `62 + DID`.
Les trois autres DID, `1114` (carburant), `1183` (rapport) et `1304` (embrayage),
restent bruts. Le refus du contrôle ne prouve pas l'absence
d'huile. Refus/silence d'ouverture ou fermeture non confirmée arrêtent les autres
groupes. Un retard de traitement ferme la session au lieu de prolonger les
lectures. `LOG OFF`, profil, mode simulé ou OTA entraînent une fermeture bornée
au mieux ; si espacement/transmission empêchent la confirmation,
`ODB SESSION_STATE` signale un état inconnu. Aucun maintien de session, balayage,
accès de sécurité, actuateur, reset ou effacement de défaut n'est ajouté.

La capture existante contient quatre refus `7F 22 80` **sans session ouverte**.
Le code Nissan CAN11 original de Car Scanner déclare `10C0` avant les lectures
constructeur et `1081` après. Cela justifie un essai, sans prouver l'acceptation
de session ou le support de ces DID sur notre ECU. Ne pas attribuer au NRC `80`
une signification UDS générique inventée. Les captures `RX` et `ODB RAW_RX`
continuent sans attente bloquante. Les nouveaux `ODB SESSION_RX`/`SESSION_STATE`
complètent les `ODB TX`, `NEGATIVE`, `TIMEOUT`, `PROBE_RX` existants sans changer
le découpage des lignes USB.

## Sources et installation

- [Preuves du mapping F15](JUKE_F15_CAN_MAPPING.fr.md) et [extraction VBOX archivée](vehicle/nissan-juke-f15/Juke_F15_CAN_REF_decode.txt) : historique des sources CAN et provenance.
- [Référence UART DUDU7/FYT, PDF français](protocols/raise-toyota-rav4/ENCODAGE_UART_DUDU7_RAV4.fr.pdf) : OS DUDU fourni **26.0131D**, FYT `j0/c3`, consommateur Toyota `m3` de DUDU Base restauré **1.004025**. Les instructions originales déterminent unités et largeurs ; leur exécution logicielle ne prouve pas toute configuration UI réelle.
- [Diagnostic EOT Juke MR16DDT MY2011](https://charm.li/Nissan-Datsun/2011/Juke%20FWD%20L4-1.6L%20Turbo%20%28MR16DDT%29/Repair%20and%20Diagnosis/A%20L%20L%20%20Diagnostic%20Trouble%20Codes%20%28%20DTC%20%29/Testing%20and%20Inspection/P%20Code%20Charts/P0197/) et [moniteurs CONSULT ECM](https://charm.li/Nissan-Datsun/2011/Juke%20FWD%20L4-1.6L%20Turbo%20%28MR16DDT%29/Repair%20and%20Diagnosis/Powertrain%20Management/Computers%20and%20Control%20Systems/Testing%20and%20Inspection/Scan%20Tool%20Testing%20and%20Procedures/CONSULT-III%20Function%20%28ECM%29/) : contenu constructeur sur miroir public, prouvant une sonde et un moniteur huile en °C dans cette famille, pas le lien à `580`/`111F` sur ce VIN.
- [Bêta officielle Car Scanner](https://www.carscanner.info/beta/), APK analysé **2.2.42** : déclarations CIL de session et formules U8−50. La tension de jauge ECM diffère de la quantité du combiné ; les échelles génériques contradictoires de `1114` ne sont pas résolues par le libellé d'une application.

Installer l'OTA **et transférer séparément le nouveau `NissanJukeF15.json`**, puis
`CAN LOAD NissanJukeF15.json`. L'OTA seule ne remplace pas les presets LittleFS.
Conserver toutes les lignes `RX`/`ODB` lors de la vérification. Les tests et le
rejeu de capture vérifient le logiciel ; l'identité physique de l'huile et les
litres absolus restent à confronter à une référence indépendante. Les autres
profils gardent FYT comme décodeur par défaut.

## Validation logicielle

[Relevé de validation](validation/F15_DUDU_BOOT_TRIP_20261107.json) : 296 tests natifs,
36 tests Python, plus le rejeu complet des 1 235 533 trames (aucun rejet du parseur
ou du décodeur). Les vérifications indépendantes exécutent 1 829 assertions sur les
instructions originales du récepteur et 208 assertions des vrais octets UART
produits par le C++ vers ce récepteur. Elles valident les unités et le cadrage
logiciels, sans confirmer l'identité physique de l'huile ni étalonner les litres.
