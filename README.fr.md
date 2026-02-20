# ESP32 CANBox Bridge

> **Langues disponibles :** **Français** | [English](README.md)

<p align="center">
  <img src="docs/images/hardware/build/BUILD_1.png" width="45%" alt="Module assemblé avec connecteur OBD-II"/>
  <img src="docs/images/hardware/build/BUILD_2.png" width="45%" alt="Boîtier imprimé 3D"/>
</p>

<p align="center">
  <img src="docs/images/demo/IMG_3356.gif" width="60%" alt="Lignes dynamiques en action"/>
  <br>
  <em>Lignes de guidage dynamiques de la caméra de recul en fonctionnement</em>
</p>

Ce projet est un **pont CAN universel** qui connecte n'importe quel véhicule à un autoradio Android. L'ESP32 lit les trames du bus CAN via le port OBD-II et les traduit vers des protocoles compris par les autoradios Android (Toyota RAV4, Raise, etc.).

Le projet a démarré comme une solution dédiée à la **Nissan Juke F15**, mais a depuis évolué vers une plateforme générique multi-véhicules.

**Points clés :**
- **Support multi-véhicules** — Les mappings CAN spécifiques à chaque véhicule sont définis dans des fichiers de configuration JSON, facilitant l'ajout de nouvelles voitures
- **Traduction de protocoles** — Convertit les données CAN constructeur vers les protocoles autoradio (Toyota RAV4, Raise/RZC)
- **Sans recompilation** — Changez de véhicule ou ajustez la calibration via commandes USB

> **Important :** Dans les paramètres de votre autoradio, configurez le protocole CAN sur **"Toyota RAV4"** pour que cela fonctionne.

> **Application requise :** Utilisez **[ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager)** (Android) pour configurer, calibrer et mettre à jour le firmware via USB.

---

## Fonctionnalités

- **Traduction en temps réel** des données CAN Nissan vers le protocole Toyota RAV4
- **Angle de braquage** pour les lignes de guidage de la caméra de recul
- **Données tableau de bord** : RPM, vitesse, tension batterie, température, niveau essence
- **État des portes** avec mise à jour automatique sur changement
- **Systèmes de sécurité** : Watchdog matériel, surveillance erreurs CAN, protection timeout

### État des Fonctionnalités

| Fonctionnalité | Statut | Notes |
| --- | --- | --- |
| Régime Moteur (RPM) | ✅ Fonctionnel | |
| Vitesse Véhicule | ✅ Fonctionnel | |
| Niveau Essence | ✅ Fonctionnel | Calibré pour Juke F15 (réservoir 45L) |
| Tension Batterie | ✅ Fonctionnel | |
| Direction / Lignes Dynamiques | ✅ Fonctionnel | Calibré pour Juke F15 |
| État des Portes | ✅ Fonctionnel | 4 portes + coffre |
| Clignotants | ✅ Fonctionnel | Gauche/droite |
| Feux | ✅ Fonctionnel | Phares, feux de route, veilleuses |
| Frein à Main | 📋 Prévu | Données CAN pas encore extraites |
| Température Extérieure | 📋 Prévu | Données CAN pas encore extraites |
| Conso. Instantanée | ✅ Fonctionnel | Depuis CAN 0x580 |
| Conso. Moyenne | ✅ Fonctionnel | Depuis CAN 0x580 |
| Autonomie Restante | ✅ Fonctionnel | Depuis CAN 0x54C |

> **Note :** La documentation du protocole Raise/Toyota RAV4 utilisé par les autoradios Android est rare. Certaines fonctionnalités sont encore en cours de reverse-engineering par manque de spécifications officielles du protocole.

Voir la **[Roadmap](ROADMAP.md)** pour les fonctionnalités à venir.

---

## Matériel Requis

### Liste des Composants (BOM)

| Composant | Description | Lien |
| --- | --- | --- |
| **ESP32-C3 SuperMini** | Microcontrôleur avec TWAI (CAN) natif | [AliExpress](https://fr.aliexpress.com/item/1005007479144456.html) |
| **SN65HVD230** | Transceiver CAN (3.3V) | [AliExpress](https://fr.aliexpress.com/item/1005009371955871.html) |
| **L7805CV** | Régulateur de tension 5V | [AliExpress](https://fr.aliexpress.com/item/1005005961287271.html) |
| **Carte prototype PCB** | Plaque à trous 4x6cm | [AliExpress](https://fr.aliexpress.com/item/1005008880680070.html) |
| **Condensateur 25V 470µF** | Condensateur de filtrage | [AliExpress](https://fr.aliexpress.com/item/1005002075527957.html) |
| **Bornier PCB** | Borniers à vis pour câblage | [AliExpress](https://fr.aliexpress.com/item/1005006642865467.html) |
| **Fusible 1A** | Fusible de protection | [AliExpress](https://fr.aliexpress.com/item/1005001756852562.html) |
| **Prise OBD-II** | CAN-H (pin 6), CAN-L (pin 14) | - |

### Note sur l'Alimentation

L'ESP32 est alimenté via **USB depuis l'autoradio Android**.

**IMPORTANT - Si vous utilisez le 12V au lieu de l'USB :**
- **Ne JAMAIS utiliser le 12V permanent** - cela viderait la batterie en stationnement
- **Utiliser uniquement le 12V ACC (accessoire)** - il se coupe avec le contact
- Vous aurez besoin d'un régulateur de tension (comme le L7805CV dans le BOM) pour convertir le 12V en 5V
- **Le circuit 12V actuel nécessite des améliorations** : Il manque une protection contre l'inversion de polarité (risque de dommage en cas de mauvais câblage) et un filtrage correct du bruit électrique généré par l'alternateur. Des composants supplémentaires (diode, condensateurs, diode TVS) seraient recommandés pour une alimentation 12V robuste.

**Pourquoi l'USB est recommandé :**
1. **Gestion automatique de l'alimentation** : L'USB de l'autoradio n'est actif que lorsque le poste est allumé (contrôlé par ACC), évitant ainsi de vider la batterie.
2. **Stabilité de la tension** : Le 12V automobile est instable - il varie significativement au démarrage (chutes de tension), pendant la charge de l'alternateur (pics jusqu'à 14.5V), et contient du bruit électrique. L'USB fournit une alimentation 5V propre et régulée.

### Schéma de Câblage

<p align="center">
  <img src="docs/images/hardware/schematic/SCHEMATIC.png" width="90%" alt="Schéma de câblage"/>
  <br>
  <em>Schéma réalisé par Polihedron</em>
</p>

### Câblage (Pinout)

| Composant | Pin ESP32 | Destination | Note |
| --- | --- | --- | --- |
| **SN65HVD230** | `3.3V` / `GND` | Alimentation | **Ne pas utiliser 5V !** |
| | `GPIO 21` | CAN-TX | Sortie vers bus CAN |
| | `GPIO 20` | CAN-RX | Entrée depuis bus CAN |
| **Autoradio** | `GPIO 5` (TX) | Fil RX (Faisceau radio) | UART 38400 baud |
| | `GPIO 6` (RX) | Fil TX (Faisceau radio) | Réponses handshake |
| **LED Statut** | `GPIO 8` | LED Interne | Indicateur visuel |

### Connexion OBD-II

Le système se connecte au véhicule via le **port diagnostic OBD-II** :

```
Connecteur OBD-II (vue face au port)
┌─────────────────────────────┐
│  1   2   3   4   5   6   7  8  │
│                                 │
│  9  10  11  12  13  14  15  16 │
└─────────────────────────────┘

Pin 6  = CAN-H (High)
Pin 14 = CAN-L (Low)
Pin 16 = +12V Batterie
Pin 4/5 = Masse
```

> **Note :** Les ports OBD-II possèdent des résistances de terminaison intégrées, donc aucune résistance supplémentaire n'est nécessaire sur le module SN65HVD230. Si votre module a une résistance 120Ω (R120), vous pouvez la laisser ou la retirer - le bus fonctionnera dans les deux cas.

### À propos du Boîtier CAN d'Origine

Le **boîtier CAN Raise/RZC d'origine** livré avec l'autoradio Android est **conservé en place**. Il fournit l'**alimentation 6V pour la caméra de recul** et reste nécessaire pour cette fonction.

Le boîtier d'origine a probablement accès au signal CAN sur son connecteur, il serait donc possible de récupérer le bus CAN depuis celui-ci au lieu de l'OBD-II. Cependant, cela n'a pas été testé - la connexion OBD-II fonctionne bien et est plus simple à installer.

---

## Configuration USB (V2)

Le boîtier peut être configuré via USB série sans recompilation. Connectez-vous au port USB et utilisez un terminal à **115200 baud**.

### Commandes Disponibles

| Commande | Description |
| --- | --- |
| `CFG LIST` | Afficher les paramètres de calibration |
| `CFG SET <param> <valeur>` | Modifier un paramètre |
| `CFG SAVE` | Sauvegarder en flash (NVS) |
| `CAN STATUS` | État de la config CAN (mode MOCK/REAL) |
| `CAN LIST` | Lister les fichiers de config véhicule |
| `CAN LOAD <fichier>` | Charger une config véhicule |
| `CAN UPLOAD START/DATA/END` | Upload config via Base64 |
| `SYS INFO` | Informations système |
| `SYS DATA` | Données véhicule en temps réel |
| `HELP` | Liste complète des commandes |

Voir la **[Documentation du Protocole USB Série](docs/protocols/USB_SERIAL_PROTOCOL.md)** pour les détails complets.

### Support Multi-Véhicules

Les configurations véhicules sont stockées sous forme de fichiers JSON sur le filesystem du boîtier. Vous pouvez :
- Uploader plusieurs configs véhicules (NissanJukeF15.json, ToyotaCorolla.json, etc.)
- Basculer entre véhicules avec `CAN LOAD <fichier>`
- Utiliser le **Mode Mock** pour tester sans véhicule

---

## Architecture Logicielle

Le système est conçu pour être 100% autonome et résistant aux parasites électriques du véhicule :

1. **[Capture CAN](docs/technical/CAN_CAPTURE.md)** : Décode les trames selon la configuration JSON et met à jour les variables globales
2. **[Envoi Radio](docs/technical/RADIO_SEND.md)** : Formate et transmet les données au poste à plusieurs intervalles (200ms pour direction, 333ms pour RPM, 500ms pour vitesse, etc.)
3. **ConfigManager** : Stockage persistant des paramètres de calibration (NVS)
4. **SerialCommand** : Interface de configuration USB
5. **Watchdog Matériel** : Redémarrage automatique si le programme gèle plus de 5 secondes
6. **Watchdog CAN** : Force un reboot si aucune donnée CAN reçue pendant 30s alors que batterie > 11V

---

## Codes LED de Statut

La LED (GPIO 8) permet un diagnostic rapide sans connexion PC :

| Pattern | Statut | Signification |
| --- | --- | --- |
| **Clignotement rapide** | Normal | Données CAN reçues et traitées |
| **Clignotement lent (500ms)** | Mode Mock | Données simulées en cours |
| **Battement lent (1s)** | Veille | Système actif, mais bus CAN silencieux |
| **Allumée fixe au boot** | Démarrage | Système en initialisation |
| **Aucune activité** | Erreur | Système figé ou problème d'alimentation |

---

## Photos du Montage

### Assemblage PCB

<p align="center">
  <img src="docs/images/hardware/pcb/PCB_TOP.png" width="45%" alt="PCB vue dessus - composants soudés"/>
  <img src="docs/images/hardware/pcb/PCB_BOTTOM.png" width="45%" alt="PCB vue dessous"/>
</p>

*ESP32 avec transceiver CAN SN65HVD230 soudé sur plaque à trous*

---

## Installation et Configuration

L'installation de votre pont CAN ESP32 se déroule en deux étapes simples :

### 1. Flash Initial du Firmware
L'ESP32 doit d'abord être programmé avec le firmware avant d'être installé. Utilisez l'une des méthodes suivantes :

- **Option A : Navigateur Web (Recommandé)**
  Ouvrez **[web.esphome.io](https://web.esphome.io/)** dans un navigateur basé sur Chrome.
  1. Téléchargez le fichier **`all-firmware.bin`** depuis la page des [Dernières Releases](https://github.com/aerodomigue/Nissan-canbus-headunit/releases). Ce fichier unique contient le bootloader, les partitions et l'application.
  2. Connectez votre ESP32 à votre ordinateur via USB.
  3. Cliquez sur **Connect**, puis **Install** et sélectionnez le fichier `all-firmware.bin`.

- **Option B : Smartphone Android (Sans PC)**
  Vous pouvez flasher depuis votre téléphone avec un **adaptateur USB OTG** et l'application **[ESP32 Loader](https://play.google.com/store/apps/details?id=com.bluedot.esp32loader)**. Sélectionnez le fichier `all-firmware.bin` et flashez-le sur l'ESP32.

- **Option C : PlatformIO (Développeurs)**
  Si vous travaillez avec le code source :
  ```bash
  pio run --target upload
  ```
  *Note : Un fichier `all-firmware.bin` est généré automatiquement dans `.pio/build/` après chaque compilation.*

### 2. Configuration et Pilotage
Une fois flashé, branchez l'ESP32 sur un port USB de votre autoradio Android. Toute la configuration se fait ensuite via l'application compagnon :

**[ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager)** (App Android)
- **Sélection du Véhicule** : Chargez la config `.json` correspondant à votre voiture.
- **Calibration** : Réglez les offsets du volant ou la capacité du réservoir.
- **Mises à jour OTA** : Mettez à jour les futures versions du firmware sans fil depuis l'app.

---

## Données Supportées

| Donnée | ID CAN | Fréq. envoi | Notes |
| --- | --- | --- | --- |
| Angle Volant | 0x002 | 200ms | Pour lignes caméra |
| Régime Moteur | 0x180 | 333ms | |
| Vitesse Véhicule | 0x284 | 500ms | Capteur roue |
| Niveau Essence | 0x5C5 | — | Mappé sur 45L (capacité réservoir Juke F15) |
| Tension Batterie | 0x6F6 | — | Sortie alternateur |
| Température | 0x551 | 5000ms | LDR (utilisé comme ext.) |
| État Portes | 0x60D | 250ms | Toutes portes + coffre (ou sur changement) |
| Autonomie | 0x54C | 5000ms | Distance estimée |
| Conso. Instantanée | 0x580 | 1000ms | Unités 0.1 L/100km |
| Conso. Moyenne | 0x580 | 5000ms | Unités 0.1 L/100km |
| Compteur km | 0x5C5 | 10000ms | Total km (24-bit) |

---

## Application Compagnon

**[ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager)** - Application Android pour configurer et surveiller le CANBox via USB.

Fonctionnalités :
- Tableau de bord données véhicule en temps réel
- Gestion des configurations CAN
- Réglage des paramètres de calibration
- Mises à jour firmware (OTA)
- Débogage des trames CAN

---

## Références & Crédits

### Documentation CAN Nissan
- [NICOclub / Manuels Nissan](https://www.nicoclub.com/nissan-service-manuals)
- [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
- [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
- [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)

### Protocoles Radio (Toyota/Raise/RZC)
- **[Analyse du Protocole Toyota RAV4](docs/protocols/raise-toyota-rav4/RAV4_BODY_ENGINE_2018_2020.pdf)** - Compilé et analysé par [Mikescotland](https://forum.dudu-auto.com/u/Mikescotland) (Source Originale)
- [Protocole Raise Officiel (Chinois)](docs/protocols/raise-toyota-rav4/) - Documentation supplémentaire
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Protocole Raise](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
- [Forum DUDU-AUTO / Qashqai 2011 CANbus](https://forum.dudu-auto.com/d/1786-nissan-qashqai-2011-canbus/6)

---

## Licence

Ce projet est open source. Voir [LICENSE](LICENSE) pour les détails.
