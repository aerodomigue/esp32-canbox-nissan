# Mapping CAN — Nissan Juke F15 2011 phase 1, 1.6 DIG-T manuel

> **Langues :** **Français** | [English](JUKE_F15_CAN_MAPPING.md)

État des connaissances au **14 octobre 2026**. Véhicule du propriétaire :
Juke F15 phase 1, moteur MR16DDT, boîte manuelle. Le marché, la référence
exacte des calculateurs et le JSON effectivement chargé dans LittleFS ne
sont pas identifiés par les logs. Le « preset actuel » désigne ici
[`data/NissanJukeF15.json`](../data/NissanJukeF15.json), à la révision
`cd02abf`, dont l'empreinte figure dans les résultats.

Cette étude complète le mapping documentaire. Elle ne modifie pas le preset
ni le firmware. Les propositions sont évaluées sur les octets et les
références disponibles, indépendamment de leur présence dans une ancienne PR.

## Résultat

- Le régime `0x180`, octets 0–1 BE divisés par **8**, est fortement étayé par
  36 références OBD ; le diviseur **7** du preset est contredit.
- La vitesse `0x284`, octets **4–5** BE divisés par 100, concorde bien avec
  l'OBD sur la plage capturée, **0–16 km/h**. Les octets 0–1 du preset ne
  représentent pas directement cette vitesse avec son facteur actuel.
- La température moteur `0x551[0]−40` concorde avec l'OBD sur **61–67 °C**.
- Le frein à main est fortement corroboré dans **`0x5C5[0] & 0x04`** :
  source Juke spécifique, deux transitions et récit du propriétaire.
- `0x580[3]`, utilisé comme consommation instantanée, suit exactement un
  compteur cyclique ; il ne fournit pas une consommation exploitable.
- Une définition Racelogic spécifique au Juke est maintenant disponible,
  avec des champs précis. Sa couverture 2011–2018 ne garantit pas chaque
  motorisation, boîte ou marché ; elle se confronte aux captures.

## Captures et contexte

| Capture | Contenu et utilité | Limite |
| --- | --- | --- |
| Historique ajouté en janvier 2026 | 1 013 trames acceptées, 3 rejets, 22 IDs | Enregistreur ancien suspect, aucune heure ni référence OBD ; voir [analyse historique](JUKE_2011_CAN_ANALYSIS.fr.md) |
| `canlog_20261014_193212_312.txt` | 165 échanges OBD, 162 mesures, 82,369 s | Aucun `RX` ordinaire : vérifie les références, pas leur équivalent CAN |
| `canlog_20261014_210614_663_762622981087673198.txt` | 92 610 `RX`, 36 IDs ordinaires, 144 échanges OBD, 141 mesures, 71,819 s | Une seule séquence de manœuvres à faible vitesse ; pas une calibration sur toute la plage |

Le propriétaire décrit la dernière séquence : frein à main desserré,
sortie de place et virage à gauche, déplacement estimé à environ 30 m,
marche arrière sur une distance également estimée, stationnement en marche
arrière puis frein à main serré. Les distances ne sont pas mesurées ; aucun
horodatage précis des gestes n'a été fourni. Elles ne servent pas à calibrer
une vitesse, une distance ou un angle. Il confirme séparément le retour du
fonctionnement des clignotants sur le dashboard.

Empreintes SHA-256, dans l'ordre du tableau :

```text
35de3a33b22716b88b28a2adfa1e6d6ed916b230cbbe2bba569b2206700df5c6
b6a28c234ec2b6425d1391b8db79fe5f34835f1a412ba8aa5fd79a5bdcf13ac2
e5c2a7539b3ad13da7c04cc15883a4b271a67c76fb3280a61878523d13311ce3
```

Le dernier fichier contient **93 188 lignes CRLF complètes**, sans rejet de
format. Les 144 séquences de requêtes sont continues et toutes possèdent une
réponse brute et un décodage cohérents. Les latences logicielles sont de
0–4 ms. Entre deux requêtes espacées de 500 ms, on observe 639–647 `RX`.
302 `RX` se placent entre une requête et sa réponse, sur 69 échanges : la
réception continue pendant l'attente OBD. Aucun compteur de perte ou d'erreur
ne progresse. Cela valide cette capture ; ce n'est pas une preuve qu'aucune
trame physique du véhicule n'a jamais été perdue.

Résultats consultables : [synthèse JSON](vehicle/nissan-juke-f15/evidence/20261014-210614-summary.json),
[283 rapprochements CAN/OBD](vehicle/nissan-juke-f15/evidence/20261014-210614-matches.csv),
[inventaire des IDs et intervalles](vehicle/nissan-juke-f15/evidence/20261014-210614-can-ids.csv),
[étude des signaux et compteurs](vehicle/nissan-juke-f15/evidence/20261014-210614-signals.json).
Les fichiers source complets sont conservés séparément ; les CSV donnent
leurs numéros de ligne, sans fabriquer de nouvelles mesures.

## Conventions et couches du protocole

`B0` désigne le premier octet, `bit0` son bit de poids faible.
`U16BE(Ba,Bb) = 256×Ba+Bb`. `I16LE` signifie signé 16 bits little endian.
Les périodes sont observées dans le log, pas des délais garantis.

La capture comporte du CAN classique standard 11 bits (`ext=0`, `rtr=0`),
avec des messages périodiques Nissan. Racelogic annonce **500 kbit/s** et
le raccordement OBD aux broches 6/14 pour cette famille de Juke. Un ID CAN
de message n'est pas automatiquement une adresse de calculateur.

Les échanges de diagnostic sont distincts : requêtes service `01` sur
`0x7DF` puis `0x7E0`, réponses `0x7E8`, transport ISO-TP simple trame dans
ces fichiers. Exemple réel de structure : `02 01 0C ...` demande le régime ;
`04 41 0C A B ...` contient sa référence `(256×A+B)/4`.
Les services constructeur CONSULT ne sont pas décodés par ce client.

Les PDF Raise/Toyota du dépôt décrivent la **liaison UART vers l'autoradio**.
Leurs commandes et unités ne déterminent pas les IDs ou facteurs des trames
Nissan reçues. Le préfixe USB exact reste `ODB`, bien que le protocole
s'appelle OBD ; voir le [guide de capture](OBD_CALIBRATION.fr.md).

## Tableau de mapping et niveau de preuve

« Corroboré » signifie accord avec les références ou les gestes observés
dans cette capture, et non validation universelle. « Candidat » exige encore
une mesure indépendante. « Contredit » concerne l'interprétation du preset,
pas l'existence du signal dans le véhicule.

| Signal | Champ / formule étudiée | Preuve et état | Écart avec le preset actuel |
| --- | --- | --- | --- |
| Régime | `0x180 B0–B1 BE /8` ; copie `0x1F9 B2–B3 /8` | **Corroboré** : 36 références à ≤5 ms, erreur absolue moyenne 7,55 tr/min, maximum 28,125 ; OBD 612,5–1 762,5 | `/7` donne une erreur moyenne de 131,22 tr/min |
| Vitesse | `0x284 B4–B5 BE /100` | **Corroboré sur plage limitée** : 35 références à ≤10 ms ; erreur absolue moyenne 0,211 km/h, maximum 1,53 | Le preset lit B0–B1 ; exemple 32 contre 16 km/h OBD |
| Température moteur | `0x551 B0−40`, °C | **Corroboré** : 34/35 égales, un écart de 1 °C, à ≤51 ms | Formule actuelle concordante |
| Tension | `0x6F6 B0 /10`, V | **Cohérent** : écart absolu moyen 0,139 V, maximum 0,44 V face à la tension ECU | Garder la distinction entre points de mesure ; pas une calibration de batterie |
| Frein à main | `0x5C5 B0 bit2` | **Fortement corroboré** : 1→0→1, scénario utilisateur et définition Racelogic Juke | Aucun champ frein à main ; cet octet sert actuellement au niveau de carburant |
| Niveau de carburant | `0x5C5 B0` pris en entier | **Contredit comme jauge directe** : 0x44→0x40→0x44 avec le frein à main | Ne pas traiter les états combinés comme des litres ; vraie jauge non identifiée |
| Consommation instantanée | `0x580 B3` | **Contredit** : 701 valeurs, 700 transitions exactes 0→2→4→6→0 | Le champ actuel est de type compteur |
| Autre donnée carburant | Candidat `0x551 B1`, canal Racelogic en mm³ | **À caractériser** : unité temporelle/cycle, compte éventuel et facteur sur MR16DDT à établir | Aucune consommation L/h ou L/100 km validée |
| Consommation moyenne | `0x580 B4` | **Non validé** : 109→110→111 ; aucune valeur du combiné synchronisée | Ne pas interpréter automatiquement ces nombres comme 10,9–11,1 L/100 km |
| Autonomie | `0x54C B6–B7 ×100/338` ; alternative `0x2DE B6–B7 /10` | **Non validé** : formule du preset constante à 305 ; alternative comprenant 824 valeurs `0xFFFF`, puis 2 707→2 687→2 684 bruts | `0xFFFF` peut être une sentinelle ; ne pas afficher 6 553,5 km. Le bit d’unité attendu reste nul |
| Volant | Preset `0x002 B1–B2 I16BE` ; candidat `B0–B1 I16LE` | **Candidat** : séquence de braquage connue, mais pas de zéro, angle mesuré ou sens calibré | La continuité d'un champ n'établit pas des degrés ; vitesse de rotation et angle sont distincts |
| Clignotants | `0x60D B1 bit5` gauche, `bit6` droit | Source Racelogic concordante, fonctionnement confirmé par l'utilisateur | Aucun de ces bits actif dans les 720 trames de cette capture ; le correctif dashboard ne recalibre pas le mapping |
| Feux | `0x60D B0 bit1/bit2`, `B1 bit3` | Définition externe concordante avec les champs de feux du preset | Pas de changement d'éclairage annoté ; distinguer commande, contact et état réel |
| Portes | Bits de `0x60D B0` | **À valider porte par porte** ; convention gauche/droite/conducteur non certifiée | Les noms conducteur/passager dépendent du véhicule ; ne pas déduire une porte d'une transition isolée de B2 |
| Température extérieure | `0x540 B0−40` | **Non validé physiquement** : 718 valeurs constantes à 28 °C, sans référence ambiante | `01 46` n'est pas annoncé ; il faut un relevé indépendant |
| Kilométrage total | `0x5C5 B1–B3 BE` | **Candidat** : 719 valeurs constantes à 246 945, sans photo/relevé compteur associé | Une valeur réaliste ne confirme ni l'unité ni l'offset |

Les 35 références de vitesse contiennent **23 zéros**. Sur les 12 références
non nulles, l'erreur absolue moyenne du candidat B4–B5 est **0,617 km/h** ; on ne
présente pas la moyenne globale comme une calibration à vitesse élevée.
`0x280 B4–B5 /100` est aussi proche, mais cette corrélation ne distingue pas
à elle seule vitesse indiquée, estimation filtrée et vitesse de roue.

## Séquence du frein à main

Le candidat `0x5C5[0] & 0x04` vaut 1 au début, passe à 0 à
**t=35 194 ms**, soit **+4,211 s** après `LOG ON`, puis revient à 1 à
**t=100 294 ms**, soit **+69,311 s**. Le reste de cet octet reste à `0x40`.
Cette séquence correspond au frein à main desserré pour partir puis serré
après le stationnement décrit par le propriétaire. Une répétition à l'arrêt
avec gestes horodatés reste utile pour exclure un état toujours corrélé.

Les changements d'autres bits qui coïncident entre plusieurs IDs ne sont pas
automatiquement des freins à main : pédale de frein, embrayage et marche
arrière sont des états différents. La capture de manœuvres ne donne pas à
elle seule leurs instants précis.
Les vitesses comparées sont non signées : elles n'identifient pas à elles
seules les phases de marche arrière décrites par le propriétaire.

## Sources publiques et manuels

### Racelogic / VBOX : fichier spécifique Juke

La [liste publique VBOX](https://vboxautomotive.co.uk/en/can-database-documents)
fournit, en plus de la
[fiche PDF Juke F15 2011–2018](https://www.racelogic.co.uk/_downloads/vbox/Vehicles/Other/Docs/Nissan-Juke%20%28F15%29%202011-2018.pdf),
un [fichier de définitions REF](https://www.racelogic.co.uk/_downloads/vbox/Vehicle_CAN_Files/New/Nissan-Juke%20%28F15%29%202011-2018.REF).
Consulté le 14 octobre 2026 : 2 650 octets, SHA-256
`960a5d9b2e8323ad8a69c5226ac7179a5a8addafc112c47276f70a4f1023d7f4`.

Le REF apporte des champs précis que la fiche PDF seule ne fournissait pas.
Il corrobore notamment la vitesse, la température et le bit de frein à main.
Son régime se limite à `0x180 B0×32` : c'est la partie haute de
`U16BE(B0,B1)/8`, quantifiée par pas de 32 tr/min. Cette approximation ne
remplace pas les deux octets confrontés à l'OBD. Son canal carburant désigne
`0x551 B1` ; le comportement de compteur observé ne valide ni le facteur
physique ni une consommation instantanée.

Le [manuel RLVBCAN01](https://www.racelogic.co.uk/_downloads/vbox/Manuals/Input_Modules/RLVBCAN01_Manual.pdf),
pages 10–11, explique la numérotation : en Motorola VBOX, les bits 63–56
appartiennent à B0, 55–48 à B1 ; en Intel, 7–0 à B0 et 15–8 à B1.
Ainsi `start=58,length=1,Motorola` désigne B0 bit2, et
`start=8,length=8,Intel` désigne B1. Ce n'est pas la convention de start-bit
Motorola d'un DBC Vector ; copier les positions telles quelles déplacerait
les signaux. La base fournisseur reste dans le cache local ; seuls les
faits nécessaires sont documentés ici, sans la republier intégralement.

### Manuels Nissan du dépôt

Les PDF examinés sont **MY2012, révision octobre 2011, Amérique du Nord**,
et non une spécification exacte du véhicule 2011 du propriétaire.
[LAN](vehicle/nissan-juke-f15/LAN.pdf), pages LAN-27 à LAN-31, distingue les
configurations M/T/CVT et 2WD/AWD puis les émetteurs/récepteurs des signaux.
Ces matrices ne donnent pas les IDs et offsets CAN.

Le combiné est essentiel pour la jauge et l'autonomie :
[MWI](vehicle/nissan-juke-f15/MWI.pdf), MWI-15/16, décrit l'entrée du capteur
et les calculs du combiné. [EC](vehicle/nissan-juke-f15/EC.pdf), EC-29,
décrit ensuite le signal de niveau transmis par CAN du combiné à l'ECM,
sans fournir son ID ni ses octets. La jauge reste donc une piste à rechercher
dans les trames, malgré l'absence de référence `01 2F` dans les logs.
[FL](vehicle/nissan-juke-f15/FL.pdf), FL-31,
donne **50 L en 2WD et 45 L en AWD** pour cette édition. Le réglage 45 L du
projet n'est donc pas validé sur le Juke manuel du propriétaire par ce manuel.
Le [catalogue corrigé](vehicle/nissan-juke-f15/README.fr.md) précise la
provenance, les vrais titres et les sections utiles.

### Travaux sur d'autres Nissan

Les études [Qashqai J10 diesel manuel 2013](https://github.com/balrog-kun/nissan-qashqai-can-info/blob/b1ffbe27a60e8808539521cfd3a00959cbf791e9/README.md)
et [Sentra 2010](https://github.com/jackm/carhack/blob/0442dc7ac08b59080da8363d6e023c884d5995fd/nissan.md)
servent de pistes pour les copies de régime, les vitesses et les états du
combiné. Le Qashqai décrit `0x002 B0–B1` en signé little endian, à
0,1 degré par unité, et `0x551 B1` comme compteur moteur/carburant encore
non résolu. Ses unités et sa motorisation ne certifient pas celles du Juke.

Le [code auteur pour une 370Z 2010](https://github.com/Knio/carhack/blob/64d873755ee8ce894fb871372890a52662cb5621/carhack/processors/nissan_370z/__init__.py#L41)
lit aussi le volant en little endian signé. Pour le frein à main,
[son code](https://github.com/Knio/carhack/blob/64d873755ee8ce894fb871372890a52662cb5621/carhack/processors/nissan_370z/__init__.py#L124)
utilise `B0 & 0x04`, cohérent avec ses exemples 68/64 ; son tableau Markdown
mentionne contradictoirement `0x02`. La confrontation au code et aux octets
évite de recopier cette erreur.

La [révision OpenDBC examinée](https://github.com/commaai/opendbc/tree/156c2c08ee6e07b6c2d0f3700c81c3ba85f4f14e/opendbc/dbc)
contient des définitions Nissan Xterra 2011, Leaf 2018 et X-Trail 2017,
mais pas de Juke. Le sens de l'angle diffère même entre le Xterra (`+0,1`)
et les définitions Nissan modernes (`−0,1`) : la convention de signe du
volant et celle de la caméra doivent être distinguées. Les fichiers sous
licence MIT sont des pistes, pas une calibration de notre variante.
La [liste AiM](https://www.aim-sportline.com/en/documentation-stock-ecu-connections.htm)
propose des fiches 350Z, 370Z, GT-R et Versa, sans entrée Juke dans la liste
consultée ; ces fiches décrivent des canaux, pas une table publique complète
d'IDs et d'offsets.

Les définitions d'autres moteurs, boîtes ou générations ne sont pas
importées sans confrontation. Les recherches de commandes CONSULT restent
dans le [bilan diagnostic](NISSAN_DIAGNOSTIC_RESEARCH.fr.md) : une définition
CAN périodique ne fournit pas une commande de diagnostic constructeur.

## Consommation : données encore nécessaires

Le candidat `0x551 B1` compte 701 valeurs. Ses 700 différences modulo 256
se répartissent en 343 zéros, 330 incréments de 1, 25 de 2 et deux de 3,
soit **386 unités cumulées**, avec un débordement `255→0` entre les lignes
43 094 et 43 231. Ce comportement justifie une étude de compteur cumulé,
pas l'affichage direct de l'octet comme consommation instantanée. L'unité
physique par incrément, les éventuelles remises à zéro et la relation avec
l'injection restent à calibrer ; le libellé mm³ du fournisseur ne les résout
pas à lui seul.


L'ECU `0x7E8` annonce `01 10` (MAF), `01 44` (rapport d'équivalence commandé),
`01 03`, `01 04`, `01 06`, `01 07` et `01 11`. Ces PIDs ne sont pas demandés
par les huit lectures actuelles. `01 5E` (débit carburant direct), `01 2F`
(niveau), `01 46` et `01 0B` ne sont pas annoncés dans les pages capturées.
Les deux captures récentes annoncent les mêmes bitmaps.

Car Scanner peut enregistrer des consommations calculées et leurs paramètres
de calcul via l'option « Always calculate and save fuel consumption » ;
[documentation de l'éditeur](https://www.carscanner.info/records/).
La méthode réellement utilisée sur ce véhicule reste à identifier.
Un export CSV avec ces dépendances permettrait de comparer une estimation
fondée sur le MAF au canal CAN candidat. Ni des mm³ sans base temporelle,
ni le régime seul, ne donnent directement des L/h ou des L/100 km.

## Validation de l'analyse

Les **34 tests Python** des outils d'analyse passent. L'export CSV puis sa
réimportation conservent les IDs, DLC et octets des trois captures :
**1 013**, **165** et **92 754** trames respectivement, sans altération des
charges utiles. Les 283 rapprochements publiés et les transitions binaires
du JSON ont été confrontés aux lignes brutes d'origine. Ces vérifications
portent sur les outils et les preuves documentaires ; aucun nouvel essai
matériel ni modification de firmware n'est réalisé dans cette étude.

## Reproduction et prochaines captures

Avec le fichier source d'empreinte identique :

```bash
python3 -B tools/analyze_can_log.py /chemin/vers/canlog_20261014_210614_663_762622981087673198.txt \
  --output-dir .local-validation/juke-current-raw
python3 -B tools/compare_obd_can.py /chemin/vers/canlog_20261014_210614_663_762622981087673198.txt \
  --preset data/NissanJukeF15.json --max-delay-ms 60 \
  --output-dir .local-validation/juke-current-comparison
python3 -B -m unittest discover -s tools -p 'test_*can*.py'
```

Le rapprochement choisit la trame CAN la plus proche dans la même session,
au maximum 60 ms. Les timestamps sont ceux du logiciel ESP32 ; la latence
des capteurs et le filtrage ECU ne sont pas connus. Les références OBD sont
espacées d'environ deux secondes par paramètre, et non simultanées.

Pour compléter les champs restant ouverts : gestes séparés et horodatés du
frein à main, des portes, des feux et des clignotants ; volant centré puis
angles et sens connus ; relevés du combiné pour jauge, kilométrage, autonomie
et unités ; collecte des paramètres de consommation avec une référence.
La distance estimée des manœuvres ne remplace pas une mesure. Les nouvelles
lectures OBD et les changements de preset doivent faire l'objet d'une
implémentation et d'une validation distinctes de ce bilan documentaire.
