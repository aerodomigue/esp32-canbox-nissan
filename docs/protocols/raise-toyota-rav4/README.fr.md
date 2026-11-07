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

Le contrat décrit les consommateurs analysés ; il n'affirme pas que toutes
les commandes documentées sont implémentées dans le firmware ESP32 actuel.
En particulier, la trame moteur `7D/09` existante correspond à FYT mais peut
faire lire son checksum comme température d'huile par DUDU. Le format commun
et la valeur d'huile qu'il exige sont expliqués dans les PDF ; aucune source
d'huile F15 ni aucun marqueur universel d'indisponibilité n'a été validé.

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
