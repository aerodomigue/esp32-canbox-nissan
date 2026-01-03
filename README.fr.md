# 🚗 Nissan Juke (F15) to Android Auto CAN Bridge (ESP32-C3)

> [!NOTE]
> **Langues disponibles :** **Français 🇫🇷** | [English 🇬🇧](README.md)

> [!IMPORTANT]
> ### 🚧 STATUS : WORK IN PROGRESS (WIP) 🚧
>
>
> **En attente de validation Hardware**
> * **Code :** 100% implémenté (Capture CAN, Émission UART, Watchdog, Morse LED).
> * **Hardware :** Montage en cours (ESP32-C3 + SN65HVD230).
> * **Étape critique suivante :** Mesurer la résistance de terminaison ($120\Omega$ vs $\infty$) sur le boîtier Raise d'origine avant le premier branchement sur le véhicule.
> * **Dernière mise à jour :** Janvier 2026.

---


Ce projet est une passerelle (gateway) intelligente permettant d'intégrer les données télémétriques d'un Nissan Juke (Platform B) sur un autoradio Android. L'ESP32-C3 intercepte les trames du bus **CAN habitacle** et les traduit en temps réel pour le poste via le protocole **Raise (RZC)**.

---

## 🛠️ Hardware & Connexions

### 1. Liste des composants (BOM)

* **Microcontrôleur** : ESP32-C3 (RISC-V). Choisi pour son contrôleur TWAI (CAN) natif.
* **Transceiver CAN** : SN65HVD230. Indispensable pour l'interface physique 3.3V.
* **Alimentation** : Régulateur DC-DC Step-Down (12V -> 5V) type MP1584EN.
* **LED de Statut** : GPIO 8 (Diagnostic & Heartbeat).

### 2. Pinout (Câblage)

| Composant | Pin ESP32-C3 | Destination | Note |
| --- | --- | --- | --- |
| **SN65HVD230** | `3.3V` / `GND` | Alimentation | **Ne pas alimenter en 5V !** |
|  | `GPIO 21` | Pin CAN-TX | Sortie vers bus CAN |
|  | `GPIO 20` | Pin CAN-RX | Entrée depuis bus CAN |
| **Autoradio** | `GPIO 1` (TX) | Fil RX (Faisceau Poste) | UART 38400 baud |
| **Status LED** | `GPIO 8` | LED Interne | Diagnostic Morse & Heartbeat |

---

## ⚙️ Logique Logicielle & Sécurités

Le système est conçu pour être 100% autonome et résistant aux parasites électriques du véhicule :

1. **[Capture (docs/CAN_CAPTURE.md)](docs/CAN_CAPTURE.md)** : Analyse les trames Nissan (500kbps) et met à jour les variables globales (Vitesse, RPM, Portes, etc.).
2. **[Émission (docs/RADIO_SEND.md)](docs/RADIO_SEND.md)** : Formate et envoie les données au poste à deux fréquences distinctes (Flux Rapide 50ms / Flux Lent 800ms).
3. **Watchdog Hardware** : Si le programme se fige plus de 5s, l'ESP32 redémarre automatiquement.
4. **Watchdog CAN** : Si aucune donnée CAN n'est reçue pendant 30s alors que le moteur tourne (tension > 11V), le système force un reboot.

---

## 🚦 Codes d'erreur LED (Morse)

La LED (GPIO 8) permet un diagnostic rapide sans brancher de PC :

* **Éteinte** : Problème d'alimentation (VCC/GND).
* **Fixe** : ESP32 alimenté, mais aucune donnée reçue sur le bus CAN.
* **Clignotement très rapide (50ms)** : Erreur d'initialisation du contrôleur CAN (Vérifier GPIO 20/21).
* **Flash bref** : Trame de direction assistée (0x002) reçue. Tout est fonctionnel.

---

## ⚠️ Précautions : Résistance de Terminaison

Le bus CAN nécessite une adaptation d'impédance précise. Le module SN65HVD230 possède souvent une résistance **R120** intégrée. **Mesurez votre ancien boîtier CAN Raise (entre CAN-H et CAN-L) :**

### CAS A : Le boîtier Raise affiche 120 Ohms

> **Action : GARDEZ LA RÉSISTANCE.** Le boîtier d'origine servait de terminaison de ligne. L'ESP32 doit reprendre ce rôle.

### CAS B : Le boîtier Raise affiche l'Infini (OL / 1)

> **Action : RETIREZ LA RÉSISTANCE (Dessouder R120).** Le poste est en dérivation au milieu du bus. Laisser la résistance perturberait les autres calculateurs.

---

## 📚 Sources & Références

### 🚗 Nissan CAN & Manuels
* [NICOclub / Manuels d'atelier Nissan](https://www.nicoclub.com/nissan-service-manuals)
* [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
* [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
* [balrog-kun / Infos CAN Nissan Qashqai](https://github.com/balrog-kun/nissan-qashqai-can-info)

### 📻 Protocoles Radio (Raise/RZC)
* [smartgauges / canbox](https://github.com/smartgauges/canbox)
* [cxsichen / Protocole Raise (睿智诚)](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
* [Forum DUDU-AUTO / Qashqai 2011 CANbus](https://forum.dudu-auto.com/d/1786-nissan-qashqai-2011-canbus/6)