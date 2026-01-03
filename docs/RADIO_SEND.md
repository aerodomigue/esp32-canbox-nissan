# 📻 Protocole Radio (UART Send)

Le module `RadioSend.cpp` communique avec le MCU du poste Android via le protocole **Raise**.

## 🛰 Spécifications Série
* **Baudrate** : 38400 bps
* **Format** : 8N1 (8 bits, No parity, 1 Stop bit)
* **Connectique** : Sortie GPIO 1 (TX) vers entrée RX du faisceau radio.

## 📦 Structure d'une Trame (0x2E)
Toutes les trames suivent ce format standard :
`[Header: 0x2E] [Commande] [Longueur] [Data...] [Checksum]`

> **Calcul du Checksum** : `(Commande + Longueur + DataSum) ^ 0xFF`

### Trames Implémentées
| ID | Fonction | Fréquence | Détail Data |
| :--- | :--- | :--- | :--- |
| **0x29** | Volant | 50ms | Angle dynamique pour lignes de guidage caméra. |
| **0x26** | Moteur | 50ms | Régime moteur pour tableau de bord virtuel. |
| **0x24** | Portes | 800ms | 1 octet (Bitmask: 0=Cond, 1=Pass, 2=ARG, 3=ARD, 4=Coffre, 7=Frein). |
| **0x27** | Système | 800ms | Bloc composite de 7 octets (Essence, Tension, Temp, DTE, Conso, Vitesse). |

## 🔗 Sources du Protocole
* [smartgauges / canbox](https://github.com/smartgauges/canbox)
* [cxsichen / Raise Protocol (睿智诚)](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)