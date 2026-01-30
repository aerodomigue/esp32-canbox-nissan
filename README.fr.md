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
| Frein à Main | ✅ Fonctionnel | |
| Température Extérieure | 📋 Prévu | Données CAN pas encore extraites |
| Conso. Instantanée | 📋 Prévu | Données CAN pas encore extraites |
| Autonomie Restante | 📋 Prévu | Données CAN pas encore extraites |

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

L'ESP32 est alimenté via **USB depuis l'autoradio Android**, et non pas depuis le 12V du véhicule.

**Pourquoi ?** Le 12V disponible sur le connecteur CAN du poste est **permanent** (toujours alimenté, même contact coupé). L'utiliser viderait lentement la batterie en stationnement. En utilisant le port USB du poste, l'ESP32 ne s'allume que lorsque l'autoradio est actif.

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
2. **[Envoi Radio](docs/technical/RADIO_SEND.md)** : Formate et transmet les données au poste à deux intervalles (100ms pour direction, 400ms pour tableau de bord)
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

## Compilation & Flash

Ce projet utilise **PlatformIO**. Pour compiler et flasher :

```bash
# Cloner le dépôt
git clone https://github.com/yourusername/Nissan-canbus-headunit.git
cd Nissan-canbus-headunit

# Compiler
pio run

# Téléverser sur l'ESP32
pio run --target upload

# Moniteur série
pio device monitor
```

---

## Données Supportées

| Donnée | ID CAN | Fréquence | Notes |
| --- | --- | --- | --- |
| Angle Volant | 0x002 | 100ms | Pour lignes caméra |
| Régime Moteur | 0x180 | 400ms | |
| Vitesse Véhicule | 0x284 | 400ms | Capteur roue |
| Niveau Essence | 0x5C5 | 400ms | Mappé sur 45L (capacité réservoir Juke F15) |
| Tension Batterie | 0x6F6 | 400ms | Sortie alternateur |
| Température | 0x551 | 400ms | LDR (utilisé comme ext.) |
| État Portes | 0x60D | Sur changement | Toutes portes + coffre |
| Autonomie | 0x54C | 400ms | Distance estimée |

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
- **[Spécification Protocole Toyota RAV4](docs/protocols/raise-toyota-rav4/)** - Documentation officielle du protocole Raise (PDF)
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Protocole Raise](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
- [Forum DUDU-AUTO / Qashqai 2011 CANbus](https://forum.dudu-auto.com/d/1786-nissan-qashqai-2011-canbus/6)

---

## Licence

Ce projet est open source. Voir [LICENSE](LICENSE) pour les détails.
