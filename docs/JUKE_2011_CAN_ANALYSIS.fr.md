# Analyse historique CAN — Juke 2011 1.6 DIG-T, boîte manuelle

> **Mise à jour du 14 octobre 2026 :** cette page conserve l'analyse du vieil
> export sans horodatages. Le [mapping courant en français](JUKE_F15_CAN_MAPPING.fr.md)
> et sa [version anglaise](JUKE_F15_CAN_MAPPING.md) intègrent désormais les
> captures CAN/OBD récentes, les manœuvres du propriétaire et les définitions
> Racelogic. Les limites de l'ancien enregistreur ne s'appliquent pas
> automatiquement aux nouveaux fichiers.
> Le retour au preset initial décrit ci-dessous est historique ; le guide
> courant détaille les corrections désormais appliquées au preset du dépôt.

L'analyse distingue les octets réellement présents, les hypothèses issues
d'autres Nissan et les mesures qui restent à faire. **L'ancien export ne
permet pas de valider physiquement les mappings de la PR #10** : le code de
l'enregistreur retrouvé dans son historique affichait la trame avant de la
lire. Les cinq mappings proposés dans la PR ont été **retirés**, ainsi que
leur profil expérimental et les tests qui le chargeaient. Le preset initial
de la tête PR est restauré. Les correctifs de robustesse restent actifs ;
[le log OBD](OBD_CALIBRATION.fr.md) permet de
collecter les références nécessaires pour décider des mappings.

## Véhicule et sources

Le propriétaire confirme que l'export provient de son Juke 2011 1.6 DIG-T
manuel, probablement au ralenti et à l'arrêt, sans certitude sur ces deux
conditions. Nissan décrit cette motorisation MR16DDT avec une boîte manuelle
six rapports en deux roues motrices dans son
[communiqué européen de lancement](https://europe.nissannews.com/en-GB/releases/nissan-juke-orders-hit-15-000).
Le marché et les unités du combiné de ce véhicule restent à confirmer.

Les recherches portent sur des sources primaires : captures et code de leurs
auteurs, documents Nissan et liste CAN du fabricant Racelogic. Les six vrais
PDF Nissan EC, FL, GI, LAN, MWI et TM ont été récupérés depuis les objets Git
LFS du dépôt, avec vérification des SHA-256. Les fichiers du clone restent
inchangés ; les téléchargements et extractions sont dans
`.local-validation/manual-text/`.

Ces manuels sont **MY2012, révision octobre 2011, marché États-Unis/Canada**
(GI-23). Ils apportent un contexte proche, mais ne constituent pas une table
des IDs et offsets pour un Juke européen 2011. Les travaux Qashqai J10 diesel,
Sentra et 370Z sont également des références de comparaison, pas des mappings
certifiés pour le MR16DDT.

## Provenance et défaut de l'ancien enregistreur

Le fichier de juin `canlog_20260627_222201.txt` cité dans la PR n'a pas été
retrouvé. Le fichier analysé est `tools/data/nissan trame export.txt`, ajouté
dans le commit `e4de885df0643e6de14dfb1c5be9fe15ae01165d`, puis supprimé dans
`6753b65f1bf4bf1d475ccc8c30e4e6bf491f38da`. Les dates Git situent sa présence
dans l'historique en janvier 2026 ; **la date de capture n'est pas connue**.

- Copie locale : `.local-validation/historical-nissan-trame-export.txt`.
- Blob Git : `6c7faf62b99c09df014bda638e737fa1dad308fe`.
- SHA-256 : `35de3a33b22716b88b28a2adfa1e6d6ed916b230cbbe2bba569b2206700df5c6`.
- 1 016 lignes : 1 013 trames au format CAN classique standard, 3 rejets,
  22 IDs, aucun horodatage.
- Rejets : ID `0x3FC91000`, ligne 107 ; DLC 20, lignes 547 et 698.
- Parmi les trames acceptées, 40 portent l'ID `0x000` et un DLC nul.
  Ce format est légal, mais ces enregistrements sont suspects dans ce contexte.

Dans le [code du commit d'ajout](https://github.com/aerodomigue/esp32-canbox-nissan/blob/e4de885df0643e6de14dfb1c5be9fe15ae01165d/src/main.cpp#L214),
`CanFrame rxFrame;` est déclaré sans initialisation à la ligne 214. Le bloc
série affiche ses champs à partir de la ligne 244, puis
`ESP32Can.readFrame(rxFrame)` n'arrive qu'à la ligne 270. La boucle d'affichage
se fie au DLC sans le borner à huit octets.

Ce code peut donc afficher des données non initialisées ou résiduelles, et
lire au-delà du tableau de données. Il fournit une explication plausible aux
enregistrements impossibles et vides. Nous ne disposons pas de la version
exacte du firmware qui a produit l'export : le lien causal reste une
hypothèse, mais la fiabilité de cette capture n'est pas établie. **Rejeter les
trois lignes invalides ne rend pas les 1 013 autres physiquement fiables.**
Le firmware actuel appelle bien la lecture avant le traitement et le log.

## Résultats mesurables dans le fichier

Les offsets ci-dessous commencent à zéro. BE signifie big endian. Les valeurs
sont des résultats numériques de l'export, sans présumer leur signification.

| ID / champ | Observation | Interprétation et limite |
| --- | --- | --- |
| `0x180`, octets 0–1 BE | 114 valeurs identiques : `0x04FB = 1275` ; `/8 = 159,375` | La formule proposée, retirée, aurait produit 159 tr/min, le preset initial `/7` produit 182. Les deux sont incompatibles avec un ralenti normal si le contexte est exact. |
| `0x1F9`, octets 2–3 BE | 116 valeurs identiques : 1275 | Copie du candidat précédent, sans référence indépendante de régime. |
| `0x180`, octets 2–3 BE `>>4` | 866 à 1 030, 32 changements, puis 981 | Égale le champ `0x160`, octets 1–2 BE `&0xFFF`, sur 114/114 comparaisons avec la trame précédente. Cette redondance ne prouve pas qu'il s'agit du régime. |
| `0x280`, octets 4–5 BE | 60 valeurs nulles | Compatible avec l'arrêt supposé ; aucune calibration de vitesse hors zéro. `0x284` et `0x285` absents. |
| `0x5C5`, octet 0 | 12 valeurs constantes : `0x40` | La formule proposée, retirée, aurait calculé 23, le preset initial 33 ; la nature « niveau de carburant » de cet octet est fortement douteuse. |
| `0x5C5`, octets 1–3 BE | 239 944 constants | Candidat kilométrage, à comparer au compteur réel. |
| `0x2DE`, octets 6–7 BE | 120 valeurs : 3 756 ; `/10 = 375,6` | La formule proposée, retirée, aurait produit 375 ; le preset actif ne traite pas cet ID. L'unité km n'est pas confirmée. Octet 4 = `0xF0`, donc son bit 1 vaut zéro. |
| `0x580`, octet 1 | 10 valeurs brutes : 20, 30, 46, 60 | Unité inconnue ; ne permet pas de conclure à 2–6 L/100 km. |
| `0x551`, octet 0 | 9 valeurs : 107 ; `-40 = 67` | Candidat température de liquide de refroidissement, compatible avec des travaux Nissan proches, sans mesure synchronisée. |

Des motifs de compteurs apparaissent dans les quatre bits faibles de
`0x180[6]` et `0x182[5]`, dans `(0x580[3] >> 1) & 3`, ainsi que dans les deux
bits faibles de `0x6E2[2]`. Les valeurs 120–123 de ce dernier ne suffisent donc
pas à identifier une tension de 12,0–12,3 V. Le nibble faible de `0x180[3]`
reste constant à 3 : ce n'est pas un compteur observé dans cet export.
Sans horodatage, aucune durée ni fréquence de publication n'est calculable.

## Confrontation aux documents

**Régime.** La [capture Qashqai J10 de balrog-kun](https://github.com/balrog-kun/nissan-qashqai-can-info/blob/b1ffbe27a60e8808539521cfd3a00959cbf791e9/README.md)
décrit le régime dans `0x180[0:2]` et une copie dans `0x1F9[2:4]`, avec une
échelle proche de `/8`. Elle décrit le champ suivant comme possiblement lié
au couple ou à la charge. Les [manuels Nissan EC](https://github.com/aerodomigue/esp32-canbox-nissan/blob/495f346d20745bc8b23be6e580cf0aa62c2faf48/docs/vehicle/nissan-juke-f15/EC.pdf)
(EC-535) donnent 600 ± 50 tr/min à chaud, sans charge, au neutre en M/T, pour
leur version nord-américaine. [MWI](https://github.com/aerodomigue/esp32-canbox-nissan/blob/495f346d20745bc8b23be6e580cf0aa62c2faf48/docs/vehicle/nissan-juke-f15/MWI.pdf)
(MWI-20/24) donne 8 191,875 tr/min comme valeur de défaut ;
`65535 / 8 = 8191,875` corrobore cette résolution **par inférence**, sans
identifier les octets. Le brut 1 275 pourrait ressembler à un ralenti accéléré,
mais retirer le diviseur sur cette seule base serait injustifié.

**Vitesse et autonomie.** Le même auteur Qashqai situe la vitesse `/100` dans
les octets 4–5 de `0x280` et `0x284`, ce qui justifie une hypothèse à comparer
avec les références OBD, sans changer l'offset du preset actif. Sa formule
d'autonomie `/10` pour `0x2DE[6:8]` est liée au mode
L/100 km, signalé par le bit 1 de l'octet 4. Ce bit est nul dans notre export,
et le brut 3 756 n'est pas un multiple de dix comme dans sa description.
Les différences de véhicule, d'unité ou de capture restent ouvertes.

**Carburant.** Les [observations Sentra 2010 de jackm](https://github.com/jackm/carhack/blob/0442dc7ac08b59080da8363d6e023c884d5995fd/nissan.md)
associent des bits de `0x5C5[0]` à l'éclairage et au frein à main ; le travail
Qashqai y décrit aussi des états, avec le kilométrage dans les octets suivants.
Cela contredit l'interprétation de tout l'octet comme niveau. MWI-15/16 décrit
une jauge reçue directement du capteur par le combiné, qui calcule l'autonomie.
La formule proposée `MAP_RANGE [255,-70,0,40]` produit seulement 0–31 L sur
un UINT8. La plausibilité isolée de 23 L ne constitue pas une validation.

**Consommation.** La [liste Racelogic propre au Juke F15 2011–2018](https://www.racelogic.co.uk/_downloads/vbox/Vehicles/Other/Docs/Nissan-Juke%20%28F15%29%202011-2018.pdf)
indique 500 kbit/s et un canal de consommation en **mm³**. Elle ne publie pas
l'ID ni l'offset et précise que les signaux varient selon les versions. Cela
ne prouve pas que `0x580[1]` soit ce canal, mais empêche de présumer des
L/100 km à partir des seules valeurs brutes ou du format UART de l'autoradio.

## Parseur et reproduction

`tools/analyze_can_log.py` utilise uniquement la bibliothèque standard Python.
Il accepte les formats série historique et actuel, les lignes `ODB RAW_RX`
et le CSV original `timestamp_ms,RX,can_id,dlc,data`. Le CSV exporté ajoute
`source_line,ext,rtr,origin` pour conserver les métadonnées. Les IDs 29 bits
sont conservés avec le flag explicite `ext=1` ; les RTR gardent leur DLC sans
payload. Les hypothèses numériques portent seulement sur les trames de données
standard. Le lecteur refuse les DLC hors 0–8, les octets incohérents, les IDs
hors du format déclaré et les horodatages invalides.
Il produit les statistiques par octet, les fenêtres BE/LE, les variations
de bits et les motifs de compteurs, puis les hypothèses numériques ci-dessus.
Les horodatages absents restent absents. Les lignes rejetées sont rapportées
avec leur numéro et leur raison.

```bash
mkdir -p .local-validation
git show '009f032dc395895bc8dd0eab34d55633301aed0a:tools/data/nissan trame export.txt' > .local-validation/historical-nissan-trame-export.txt
.venv/bin/python -B tools/analyze_can_log.py .local-validation/historical-nissan-trame-export.txt
.venv/bin/python -B -m unittest discover -s tools -p test_analyze_can_log.py -v
```

Sorties locales : `.local-validation/deep-analysis/analysis.json` et
`frames.csv`. Le JSON contient l'empreinte de la source ; le CSV conserve
les octets et le numéro de ligne source. Le parseur explore des hypothèses ;
les tests de relecture C++ continuent d'exercer le décodeur de production.

Vérification initiale : **13 tests Python réussis**, couvrant les formats, les
rejets, la reprise après un CSV corrompu, les compteurs, la redondance 12 bits
et la protection du fichier source, y compris contre les liens physiques.
L'analyse complète retrouve 1 013 trames et trois rejets ; réimporter son CSV
retrouve les mêmes 1 013 charges utiles, sans rejet. Les vérifications actuelles
du décodeur, du client OBD et de la construction
sont détaillées dans [le rapport PR #10](PR10_VALIDATION.fr.md).

## Capture nécessaire pour trancher

1. Enregistrer uniquement après une lecture CAN réussie, avec DLC borné et
   horodatage par trame. Conserver le firmware exact, le preset et les
   unités affichées au combiné. Distinguer les trames reçues des injections
   série et des valeurs de démonstration.
2. Annoter des séquences à l'arrêt : contact sans moteur, démarrage, ralenti
   froid puis chaud, et quelques paliers de régime connus. Relever le compte-tours
   ou le régime d'un outil OBD existant au même instant pour comparer `/8`,
   le brut et le champ 12 bits partagé.
3. À niveau de carburant constant, comparer contact, éclairage et frein à
   main ; ensuite comparer une capture avant/après un plein avec les litres
   ajoutés. Cela sépare les états de `0x5C5[0]` d'un véritable niveau.
4. Relever simultanément kilométrage, autonomie et unité de consommation.
   Vérifier le bit d'unité de `0x2DE` et chercher les champs qui suivent les
   changements réels de jauge et d'autonomie.
5. Pour la vitesse, il faudra des points non nuls avec une référence connue.
   Une capture à l'arrêt ne permet pas de choisir entre plusieurs échelles.

Les propositions d'anyzlo ont été retirées du code, des profils et des tests
expérimentaux. Le format UART reste celui de la tête PR, y compris les
commandes `0x23` et `0x24`. Les statistiques de trajet, l'estimation du réservoir
et le relais de climatisation proposés n'ont pas été introduits. Une nouvelle
capture documentée avec les références OBD reste nécessaire avant de présenter
le carburant, le régime ou les unités de consommation comme calibrés sur cette voiture.
Cette étape est maintenant partiellement réalisée : voir le
[bilan des captures récentes](JUKE_F15_CAN_MAPPING.fr.md), qui établit le régime,
la vitesse à faible allure et le bit de frein à main, tout en gardant ouvertes
les unités de consommation et la jauge.
