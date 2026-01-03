# 🛰️ Documentation Technique : Capture CAN Nissan

Ce module est responsable de l'écoute passive du bus **CAN High Speed** (Habitacle) du Nissan Juke. Il traduit les trames binaires du véhicule en variables normalisées pour le reste du programme.

## 📡 Spécifications Physiques

* **Vitesse du bus** : 500 kbps.
* **ID Format** : 11-bit (Standard).
* **Câblage** : Paire torsadée (CAN-H / CAN-L).
* **Point d'accès** : Connecteur 20 broches à l'arrière de l'autoradio (Fils Rose et Bleu) ou prise OBDII (Pins 6 et 14).

---

## 🔍 Logique de Décodage

Le module `CanCapture.cpp` traite les trames en temps réel via les identifiants suivants :

### 1. Direction et Dynamique

| ID CAN | Signal | Logique de calcul | Note |
| --- | --- | --- | --- |
| **0x002** | Angle Volant | `(int16)(Data[0]<<8 | Data[1])` | Point milieu à 0. Mappé pour la caméra. |
| **0x284** | Vitesse | `(uint16)(Data[0]<<8 | Data[1]) / 100` | Précision au centième en km/h. |
| **0x156** | RPM Moteur | `(uint16)(Data[1]<<8 | Data[2]) / 8` | Régime de rotation vilebrequin. |

### 2. État du Véhicule (BCM)

**ID 0x60D** regroupe les informations de carrosserie :

* **Ouvrants (Byte 0)** : Décodage par masques de bits.
* `0x01` : Conducteur | `0x02` : Passager | `0x04` : AR Gauche | `0x08` : AR Droit | `0x10` : Coffre.


* **Carburant (Byte 1)** : Valeur brute 0-255 représentant le niveau de la jauge.
* **Sécurité (Byte 2)** : Bit `0x01` actif si le frein à main est engagé.

### 3. Confort et Ordinateur de Bord

* **0x54C** : Gère la consommation moyenne (`Byte 2 * 0.1`) et l'autonomie (DTE) sur les `Bytes 4-5`.
* **0x5E5** : Tension batterie brute (`Byte 0 * 0.1`).
* **0x510** : Température extérieure. Nissan utilise un offset de -40 (`Byte 0 - 40`).

---

## 🛠️ Implémentation Logicielle

La capture repose sur la bibliothèque **ESP32-TWAI-CAN**.

* **Heartbeat** : La trame `0x002` (émise en permanence par la direction assistée) sert de témoin d'activité. La LED `GPIO 8` change d'état à chaque réception, confirmant que le transceiver `SN65HVD230` fonctionne.
* **Non-bloquant** : La fonction `handleCanCapture` traite une seule trame à la fois pour ne pas ralentir le reste du système.

---

## 🔗 Sources et Crédits (Nissan Reverse Engineering)

Ce travail de décodage s'appuie sur la documentation communautaire des véhicules sur plateforme Nissan B (Juke, Leaf, Micra) :

### 🚗 Nissan CAN & Manuals
* [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
* [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
* [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
* [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)

### 📻 Radio Protocols (Raise/RZC)
* [smartgauges / canbox](https://github.com/smartgauges/canbox)
