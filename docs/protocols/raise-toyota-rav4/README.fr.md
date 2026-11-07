# Documentation UART Raise Toyota/RAV4

[English](README.md)

Ce dossier documente le protocole UART entre le CANBox ESP32 et l'autoradio
Android. Le mapping CAN du véhicule Nissan F15 est décrit séparément dans
l'[analyse du mapping F15](../../JUKE_F15_CAN_MAPPING.fr.md).

## Référence d'encodage DUDU7

| Document | Langue | Contenu |
| --- | --- | --- |
| [ENCODAGE_UART_DUDU7_RAV4.fr.pdf](ENCODAGE_UART_DUDU7_RAV4.fr.pdf) | Français | Contrat d'encodage, 67 pages |
| [ENCODAGE_UART_DUDU7_RAV4.en.pdf](ENCODAGE_UART_DUDU7_RAV4.en.pdf) | Anglais | Contrat d'encodage, 67 pages |

Révision du **7 novembre 2026**, issue de l'archive OS DUDU7 fournie
**26.0131D**, des applications FYT et de DUDU Base **1.004025** restaurée.
Les documents décrivent **575 entrées typées** : champs, réglages, actions
et correspondances de touches. Ce total n'est pas un nombre de capteurs
physiquement validés.

La référence couvre les commandes dans les deux sens, les positions et
largeurs des champs en bits, le signe, l'ordre des octets, les unités et
conversions, les longueurs de paquets, le checksum, les préconditions du
récepteur et des exemples de trames complètes.
`ESP32_TX` signifie CANBox vers poste ; `ESP32_RX`, poste vers CANBox.
Les messages internes du MCU sont distingués de l'UART externe du CANBox.

### Validation et applicabilité

La référence distingue les décodeurs indépendants FYT et DUDU. Une même
commande peut utiliser des positions d'octets ou des règles de disponibilité
différentes. Les significations physiques, unités et marqueurs d'indisponibilité
inconnus restent signalés ; connaître la largeur de stockage d'un champ
ne valide pas son interprétation physique.

La validation comprend **8 740 contrôles structurels**, **271 assertions
utilisant les instructions originales des décodeurs** et **six méthodes de
test de l'encodeur**, dont les 4 096 valeurs signées sur 12 bits. Ce sont
des vérifications logicielles avec des frontières Android contrôlées,
pas des essais physiques sur le poste ni une calibration du véhicule.

Les PDF constituent une analyse datée des consommateurs, et non une liste des
commandes implémentées par l'ESP32. Leur avertissement sur l'ancien paquet
`7D/09` LEN3 décrit le firmware avant la modification des trajets depuis le
démarrage : DUDU pouvait lire son checksum comme température d'huile. Le nouvel
émetteur F15 utilise LEN4 `09 eau+40 eau+40 huile+40`, uniquement lorsque les deux
sources sont disponibles et fraîches. FYT reçoit l'eau en D2 ; DUDU reçoit
l'eau en D1 et l'huile en D3.

La source d'huile F15 `0x580 B4 - 40` reste explicitement **provisoire**,
acceptée pour comparaison après des rejeux cohérents, sans calibration
indépendante. Aucun octet d'indisponibilité d'huile n'est supporté par ce
décodeur DUDU : zéro et 255 signifient -40 °C et 215 °C. Sans huile disponible,
le paquet complet de températures DUDU est donc suspendu. L'ancien paquet LEN3
avec l'eau seule est conservé exclusivement pour les profils non DUDU.

Le mapping F15 actuel transmet aussi vitesse et vitesse moyenne depuis le
démarrage via `7D/03` avec **LEN6 et une unité métrique explicite** ; le temps
de `0x21` est exprimé en **minutes**. Les consommations instantanée et moyenne
depuis le démarrage sont des U16BE en `0,1 L/100 km` dans `0x22` et `0x23` ; les
cinq historiques restent indisponibles. Les deux trajets de `7D/04` sont des
miroirs explicites du trajet logiciel depuis le démarrage du boîtier, et non
les compteurs indépendants Trip A/B Nissan.

Le parseur de températures DUDU démarre en Celsius. Après remplacement d'un
boîtier qui envoyait des Fahrenheit sans redémarrer le récepteur, redémarrer
DUDU/le profil RAV4 avant de vérifier les températures : `7D/09` ne réinitialise
pas un ancien état Fahrenheit. Aucun paquet climatique `0x28` artificiel n'est
émis pour imposer cette unité. La source extérieure F15 non validée est omise ;
un capteur absent n'est pas présenté comme une mesure nulle. Voir le
[guide du mapping F15 depuis le démarrage](../../F15_DUDU_BOOT_TRIP_MAPPING.fr.md)
pour la disponibilité, les valeurs ignorées propres à DUDU, les interruptions
et les limites restantes.

Voir les [notes d'implémentation RadioSend](../../technical/RADIO_SEND.md)
pour le comportement actuel du firmware et les [preuves du mapping F15](../../JUKE_F15_CAN_MAPPING.fr.md)
pour la validation distincte des données du véhicule.

## Sources antérieures du protocole

- [Protocole RAV4 carrosserie/moteur, 2018–2020](RAV4_BODY_ENGINE_2018_2020.pdf),
  compilé et analysé par
  [Mikescotland](https://forum.dudu-auto.com/u/Mikescotland).
- [Protocole série Raise Camry/RAV4 V2.21.000, chinois](<睿志诚软件_丰田CAMRY & RAV4 串口通讯协议  V2.21.000 (1).pdf>).

Les PDF de ce dossier utilisent **Git LFS**, conformément au fichier
`.gitattributes` du dépôt. Après clonage, utiliser `git lfs pull` pour
récupérer leur contenu à la place des fichiers pointeurs.
