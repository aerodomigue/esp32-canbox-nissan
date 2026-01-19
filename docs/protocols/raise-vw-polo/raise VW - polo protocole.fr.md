Plateforme Volkswagen – Protocole de communication CAN BUS entre module et poste DVD 1.4
=======================================================================================

Ce document décrit le protocole de communication entre le système de poste / navigation DVD (HOST) et le décodeur CAN BUS (SLAVE).  
Il couvre la **couche physique**, la **couche liaison de données** et la **couche application**.

---

Couche physique
---------------

- **Interface** : UART standard
- **Niveau logique** : 5 V TTL
- **Mode UART** : 8N1  
  - 8 bits de données  
  - pas de parité  
  - 1 bit d’arrêt
- **Débit** : fixé à **38400 bps**

### Conventions

- **HOST** : poste / unité principale DVD / NAVI
- **SLAVE** : décodeur CAN BUS (boîtier CAN)

---

Couche liaison de données
-------------------------

### Structure de trame

La trame UART est définie comme suit :

| Ordre | Champ     | Description                                      |
|-------|-----------|--------------------------------------------------|
| 1     | Head Code | Fixé à `0x2E`                                    |
| 2     | Data Type | Voir le tableau *Définition du DataType* ci‑dessous |
| 3     | Length    | Longueur du champ de données                     |
| 4     | Data0     |                                                  |
| 5     | Data1     |                                                  |
| 6..N  | …         | Data2…DataN (charge utile)                       |
| N     | Checksum  | Voir ci‑dessous                                  |

**Checksum** :

\[
Checksum = \bigl(DataType + Length + Data0 + \dots + DataN\bigr) \oplus 0xFF
\]

---

ACK / NACK
----------

### Définition

ACK/NACK est une trame de réponse sur 1 octet :

| Valeur | Signification                  |
|--------|--------------------------------|
| `0xFF` | ACK (trame correcte)          |
| `0xF0` | NACK (Checksum incorrect / erreur) |

- La trame de réponse **ne contient qu’un seul octet**.

### Règles de temporisation

1. Après réception d’une trame complète, le récepteur **doit répondre par ACK ou NACK dans les 10 ms**.  
2. L’émetteur doit être capable de recevoir l’ACK/NACK dans une fenêtre de **0 à 100 ms** après l’envoi de la trame.  
3. Si aucun ACK n’est reçu dans les 100 ms, la trame **doit être retransmise**.  
   - Si la même trame a été retransmise **trois fois sans ACK**, toute nouvelle émission doit être arrêtée et un traitement d’erreur approprié doit être exécuté.

---

Couche application
------------------

### Définition du champ DataType

Le champ `DataType` définit la sémantique de la charge utile.

#### DataType de SLAVE → HOST

| DataType | Nom / Description                  | Remarques                                                                                  |
|----------|------------------------------------|--------------------------------------------------------------------------------------------|
| `0x14`   | Information sur le niveau de rétroéclairage | Envoyé à la mise sous tension, lors des changements de rétroéclairage et sur demande |
| `0x16`   | Vitesse véhicule                   |                                                                                            |
| `0x20`   | Informations touches volant        |                                                                                            |
| `0x21`   | Informations de climatisation      |                                                                                            |
| `0x22`   | Informations radar de recul        |                                                                                            |
| `0x23`   | Informations radar avant           |                                                                                            |
| `0x24`   | Informations véhicule de base      | Le décodeur envoie de manière proactive en cas de changement ; peut aussi être interrogé par le poste |
| `0x25`   | État de l’aide au stationnement    |                                                                                            |
| `0x26`   | Angle de volant                    |                                                                                            |
| `0x27`   | État de l’amplificateur            |                                                                                            |
| `0x30`   | Informations de version            | Envoyé à la mise sous tension ou sur requête                                              |
| `0x41`   | Informations de carrosserie        |                                                                                            |

#### DataType de HOST → SLAVE

| DataType | Nom / Description              | Remarques                                                                                                                           |
|----------|--------------------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| `0x81`   | Start / End                    | Établissement / libération de la connexion                                                                                          |
| `0x90`   | Requête d’informations de contrôle | Demande au décodeur d’envoyer certains types de données (`0x14, 0x16, 0x21, 0x24, 0x25, 0x26, 0x30, 0x41`) au poste                 |
| `0xA0`   | Commande de contrôle ampli     | Contrôle des paramètres de l’amplificateur externe                                                                                  |
| `0xC0`   | Sélection de source            | Sélection de la source audio/vidéo active                                                                                           |
| `0xC1`   | Indication d’ICÔNE / mode de lecture | Affichage de l’état / icône pour la source courante et le mode de lecture                                                      |
| `0xC2`   | Informations tuner (radio)     | Fréquence, bande et numéro de présélection                                                                                          |
| `0xC3`   | Informations de lecture média  | Informations génériques de lecture (piste, dossier, temps, etc.) selon le type de média                                            |
| `0xC4`   | Contrôle d’affichage du volume | Indication de volume et de mute                                                                                                     |
| `0xC6`   | Contrôle du volume radar       | Activation/désactivation du son du radar arrière et mode de stationnement                                                           |

---

Formats de données détaillés
----------------------------

### 1) Informations de rétroéclairage – `0x14` (SLAVE → HOST)

| Champ    | Valeur  | Description                         |
|----------|---------|-------------------------------------|
| DataType | `0x14`  | Type de données                     |
| Length   | `0x01`  | Longueur = 1 octet                  |
| Data0    | 0x00–FF | Luminosité du rétroéclairage des touches |

- `Data0 = 0x00` → plus sombre  
- `Data0 = 0xFF` → plus lumineux  
- Cette valeur reflète la **luminosité du rétroéclairage des touches d’origine**.  
- Si on l’utilise pour contrôler la luminosité de l’écran, noter :  
  - Quand `Data0 = 0x00`, l’environnement est lumineux et le rétroéclairage des touches est coupé, donc la **luminosité écran doit être relativement élevée**.  
- Cette trame est envoyée :
  - à la mise sous tension,  
  - à chaque changement de valeur,  
  - et sur requête explicite.

---

### 2) Vitesse véhicule – `0x16` (SLAVE → HOST)

| Champ    | Valeur | Description                          |
|----------|--------|--------------------------------------|
| DataType | `0x16` | Type de données                      |
| Length   | `0x02` | Longueur = 2 octets                  |
| Data0    | LSB    | Octet de poids faible                |
| Data1    | MSB    | Octet de poids fort                  |

Vitesse véhicule :

\[
Vitesse\,(km/h) = \frac{(Data1 \times 256 + Data0)}{16}
\]

---

### 3) Touches volant – `0x20` (SLAVE → HOST)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0x20` | Type de données    |
| Length   | `0x02` | Longueur = 2 octets |

#### Data0 – Code touche

| Valeur | Signification                      |
|--------|------------------------------------|
| 0x00   | Aucune touche / touche relâchée    |
| 0x01   | VOL+                               |
| 0x02   | VOL–                               |
| 0x03   | `>` / MENU HAUT                    |
| 0x04   | `<` / MENU BAS                     |
| 0x05   | TEL                                |
| 0x06   | MUTE                               |
| 0x07   | SRC                                |
| 0x08   | MIC                                |

#### Data1 – État de la touche

| Valeur | Signification           |
|--------|-------------------------|
| 0      | Touche relâchée         |
| 1      | Touche enfoncée         |
| 2      | Appui continu valide    |

---

### 4) Informations de climatisation – `0x21` (SLAVE → HOST)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0x21` | Type de données    |
| Length   | `0x05` | Longueur = 5 octets |

#### Data0 – État A/C

- **Bit7** : Marche/Arrêt A/C principal  
  - 0 : OFF  
  - 1 : ON
- **Bit6** : Compresseur A/C ON/OFF  
  - 0 : A/C OFF  
  - 1 : A/C ON
- **Bit5** : Mode de circulation d’air  
  - 0 : air neuf (extérieur)  
  - 1 : recyclage (intérieur)
- **Bit4** : Indicateur AUTO (ventilation forte)  
  - 0 : OFF  
  - 1 : ON
- **Bit3** : Indicateur AUTO (ventilation faible)  
  - 0 : OFF  
  - 1 : ON
- **Bit2** : Indicateur DUAL  
  - 0 : OFF  
  - 1 : ON
- **Bit1** : Indicateur MAX FRONT  
  - 0 : OFF  
  - 1 : ON
- **Bit0** : Indicateur REAR  
  - 0 : OFF  
  - 1 : ON

#### Data1 – Ventilation / direction de l’air

- **Bit7** : Volets supérieurs ON/OFF  
- **Bit6** : Volets frontaux (niveau visage) ON/OFF  
- **Bit5** : Volets inférieurs ON/OFF  
- **Bit4** : Demande d’affichage climatisation  
  - 0 : ne pas afficher  
  - 1 : demander l’affichage des infos A/C
- **Bit3..Bit0** : Niveau de ventilation  
  - 0x00–0x07 → niveau 0–7

#### Data2 – Température conducteur

- `0x00` : LO  
- `0x1F` : HI  
- `0x01–0x11` : 18 °C – 26 °C, pas de 0,5 °C (autres valeurs invalides)

#### Data3 – Température passager

- Même codage que Data2 (côté conducteur).

#### Data4 – Chauffage sièges / AQS

- **Bit7** : Recyclage AQS  
  - 0 : recyclage non‑AQS  
  - 1 : recyclage AQS
- **Bit6** : réservé  
- **Bit5..Bit4** : Niveau de chauffage siège gauche  
  - 00 : pas d’affichage  
  - 01–11 : niveau 1–3
- **Bit1..Bit0** : Niveau de chauffage siège droit  
  - 00 : pas d’affichage  
  - 01–11 : niveau 1–3

---

### 5) Radar de recul – `0x22` (SLAVE → HOST)

Envoyé **uniquement lorsque les distances changent**.

| Champ    | Valeur | Description                         |
|----------|--------|-------------------------------------|
| DataType | `0x22` | Type de données                     |
| Length   | `0x04` | Longueur = 4 octets                 |

Pour tous les octets de distance ci‑dessous :

- `0x00` : ne pas afficher  
- `0x01` : distance la plus proche  
- `0x0A` : distance la plus éloignée  
- Plage : `0x00–0x0A`

| Octet | Description                                  |
|-------|----------------------------------------------|
| Data0 | Distance capteur arrière gauche             |
| Data1 | Distance capteur arrière centre‑gauche      |
| Data2 | Distance capteur arrière centre‑droit       |
| Data3 | Distance capteur arrière droit              |

---

### 6) Radar avant – `0x23` (SLAVE → HOST)

Également envoyé **uniquement lorsque les distances changent**.

| Champ    | Valeur | Description                         |
|----------|--------|-------------------------------------|
| DataType | `0x23` | Type de données                     |
| Length   | `0x04` | Longueur = 4 octets                 |

Le codage des distances est identique à celui de `0x22`.

| Octet | Description                                  |
|-------|----------------------------------------------|
| Data0 | Distance capteur avant gauche               |
| Data1 | Distance capteur avant centre‑gauche        |
| Data2 | Distance capteur avant centre‑droit         |
| Data3 | Distance capteur avant droit                |

---

### 7) Informations de base – `0x24` (SLAVE → HOST)

Envoyé **de manière proactive en cas de changement de données**, ou sur **requête explicite** du poste DVD.

| Champ    | Valeur | Description                 |
|----------|--------|-----------------------------|
| DataType | `0x24` | Type de données             |
| Length   | `0x01` | Longueur = 1 octet          |
| Data0    |        | Informations marche arrière / stationnement |

#### Data0 – Marche arrière / stationnement

- **Bit7..Bit3** : réservés  
- **Bit2** : État des feux  
  - 0 : OFF  
  - 1 : ON
- **Bit1** : État du rapport P ou frein à main  
  - 0 : hors P / frein à main relâché  
  - 1 : en P ou frein à main serré
- **Bit0** : État marche arrière  
  - 0 : pas en marche arrière  
  - 1 : marche arrière engagée

---

### 8) État de l’aide au stationnement – `0x25` (SLAVE → HOST)

| Champ    | Valeur | Description                        |
|----------|--------|------------------------------------|
| DataType | `0x25` | Type de données                    |
| Length   | `0x02` | Longueur = 2 octets               |
| Data0    |        | État de l’aide au stationnement   |
| Data1    |        | Réservé                           |

#### Data0 – Bits

- **Bit7..Bit2** : réservés  
- **Bit1** : Aide au stationnement ON/OFF  
  - 0 : OFF  
  - 1 : ON
- **Bit0** : Son du radar ON/OFF  
  - 0 : aucun son radar  
  - 1 : son radar actif

---

### 9) Angle de volant – `0x26` (SLAVE → HOST)

| Champ    | Valeur | Description                 |
|----------|--------|-----------------------------|
| DataType | `0x26` | Type de données             |
| Length   | `0x02` | Longueur = 2 octets         |
| Data0    | ESP1   | Octet faible angle volant   |
| Data1    | ESP2   | Octet fort angle volant     |

Exemple d’interprétation :

- `ESP = 0` → volant centré  
- `ESP > 0` → braquage à gauche  
- `ESP < 0` → braquage à droite

---

### 10) Informations de carrosserie – `0x41` (SLAVE → HOST)

| Champ     | Description                           |
|-----------|---------------------------------------|
| DataType  | `0x41`                                |
| Length    | `N` (variable)                        |
| Data0     | Commande                              |
| Data1..11 | Données (selon la commande)          |

#### Table des commandes

##### Commande `0x01` – Portes / sécurité / coffre / lave‑glace

- `N = 2`
- Bits de `Data[1]` :
  - Bit7 : Ceinture conducteur  
    - 0 : normal (ceinture bouclée)  
    - 1 : ceinture non bouclée
  - Bit6 : Niveau de lave‑glace  
    - 0 : normal  
    - 1 : niveau lave‑glace faible
  - Bit5 : Frein à main  
    - 0 : normal (relâché)  
    - 1 : frein à main serré
  - Bit4 : État du coffre  
    - 0 : fermé  
    - 1 : ouvert
  - Bit3 : Porte arrière (si présente)  
    - 0 : fermée  
    - 1 : ouverte
  - Bit2 : Porte arrière gauche  
    - 0 : fermée  
    - 1 : ouverte
  - Bit1 : Porte avant droite  
    - 0 : fermée  
    - 1 : ouverte
  - Bit0 : Porte avant gauche  
    - 0 : fermée  
    - 1 : ouverte

##### Commande `0x02` – Moteur / vitesse / batterie / température extérieure / kilométrage / carburant

- `N = 13`

| Octets       | Description                                             |
|--------------|---------------------------------------------------------|
| Data1–Data2  | Régime moteur (RPM) : `RPM = Data1 * 256 + Data2`      |
| Data3–Data4  | Vitesse instantanée (km/h) : `(Data3 * 256 + Data4) * 0.01` |
| Data5–Data6  | Tension batterie (V) : `(Data5 * 256 + Data6) * 0.01`  |
| Data7–Data8  | Température extérieure (°C) : `(Data7 * 256 + Data8) * 0.1`.<br>Plage valide : −50 °C à 77,5 °C (valeurs négatives en complément à deux sur 2 octets) |
| Data9–Data11 | Distance parcourue (km) : `Data9 * 65536 + Data10 * 256 + Data11` |
| Data12       | Carburant restant (L) : `Fuel = Data12`                |

##### Commande `0x03` – Drapeaux d’alarme

- `N = 2`
- Bits de `Data1` :
  - Bit7 : Alerte faible niveau carburant  
    - 1 : carburant faible ; 0 : normal
  - Bit6 : Alerte tension batterie  
    - 1 : tension batterie faible ; 0 : normal
  - Bit5..Bit0 : réservés

---

### 11) État de l’amplificateur – `0x27` (SLAVE → HOST)

Envoyé lorsque les réglages d’amplification changent.

| Champ    | Valeur | Description                                       |
|----------|--------|---------------------------------------------------|
| DataType | `0x27` | Type de données                                   |
| Length   | `0x08` | Longueur = 8 octets                               |

| Octet | Nom                      | Plage / Signification                        |
|-------|--------------------------|----------------------------------------------|
| Data0 | Type d’amplificateur     | `0x00` : pas d’ampli ; `0x01` : ampli sans contrôle CAN ; `0xFF` : ampli inconnu |
| Data1 | Volume                   | 0–30 (unsigned char)                         |
| Data2 | BALANCE                  | −9 – +9 (signed char)                        |
| Data3 | FADER                    | −9 – +9 (signed char)                        |
| Data4 | BASS                     | −9 – +9 (signed char)                        |
| Data5 | MIDTONE                  | −9 – +9 (signed char)                        |
| Data6 | TREBLE                   | −9 – +9 (signed char)                        |
| Data7 | Volume dépendant vitesse | 0–7 (unsigned char)                          |

---

### 12) Start / End – `0x81` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0x81` | Type de données    |
| Length   | `0x01` | Longueur = 1 octet |
| Data0    |        | Type de commande   |

#### Data0 – Type de commande

| Valeur | Signification                                                                                                                                                          |
|--------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0x01   | **Start** – au démarrage du système, le HOST envoie cette commande pour établir la connexion. La connexion est établie lorsque le SLAVE répond par un ACK.           |
| 0x00   | **End** – à l’arrêt du système, le HOST envoie cette commande pour couper la connexion. Après réception de l’ACK du SLAVE, le HOST arrête toute communication.        |

---

### 13) Requête d’informations de contrôle – `0x90` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0x90` | Type de données    |
| Length   | `N`    | Longueur des données |

| Octet | Description                             |
|-------|-----------------------------------------|
| Data0 | Contenu demandé (voir ci‑dessous)       |
| Data1 | Utilisé seulement pour `0x41` (infos carrosserie) |

**Data0 – Contenu demandé :**

- `0x14` : Informations rétroéclairage  
- `0x16` : Vitesse véhicule  
- `0x21` : Informations climatisation  
- `0x24` : Informations de base  
- `0x25` : État de l’aide au stationnement  
- `0x26` : Angle de volant  
- `0x30` : Informations de version  
- `0x41` : Informations de carrosserie (valeur de commande précisée dans Data1)

---

### 14) Contrôle de l’amplificateur – `0xA0` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xA0` | Type de données    |
| Length   | `0x02` | Longueur = 2 octets |

| Octet | Description |
|-------|-------------|
| Data0 | Commande    |
| Data1 | Paramètre   |

#### Data0 – Codes de commande

| Valeur | Signification                     |
|--------|-----------------------------------|
| 0x00   | Volume                            |
| 0x01   | BALANCE                           |
| 0x02   | FADER                             |
| 0x03   | BASS                              |
| 0x04   | MIDTONE                           |
| 0x05   | TREBLE                            |
| 0x06   | Volume dépendant de la vitesse    |

#### Tableau des paramètres (Data1)

| Index | Commande                     | Plage de paramètres              | Valeur par défaut |
|-------|------------------------------|----------------------------------|-------------------|
| 0     | Volume                       | 0–30 (unsigned char)            | 28                |
| 1     | BALANCE                      | −9 – +9 (signed char)           | 0                 |
| 2     | FADER                        | −9 – +9 (signed char)           | 0                 |
| 3     | BASS                         | −9 – +9 (signed char)           | 0                 |
| 4     | MIDTONE                      | −9 – +9 (signed char)           | 0                 |
| 5     | TREBLE                       | −9 – +9 (signed char)           | 0                 |
| 6     | Volume dépendant vitesse     | 0–7 (unsigned char)             | 0                 |

> En l’absence de commandes de contrôle explicites, le CAN BOX ouvre l’amplificateur avec ces valeurs par défaut.

---

### 15) Source – `0xC0` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC0` | Type de données    |
| Length   | `0x02` | Longueur = 2 octets |

#### Data0 – Source

| Valeur | Source                      |
|--------|-----------------------------|
| 0x00   | OFF                         |
| 0x01   | Tuner                       |
| 0x02   | Disque (CD, DVD)            |
| 0x03   | TV (analogique)            |
| 0x04   | NAVI                        |
| 0x05   | Téléphone                   |
| 0x06   | iPod                        |
| 0x07   | AUX                         |
| 0x08   | USB                         |
| 0x09   | SD                          |
| 0x0A   | DVB‑T                       |
| 0x0B   | Téléphone A2DP              |
| 0x0C   | Autre                       |
| 0x0D   | CDC v1.02                   |

#### Data1 – Type de média

| Valeur | Type de média           |
|--------|-------------------------|
| 0x01   | Tuner                   |
| 0x10   | Média audio simple      |
| 0x11   | Média audio avancé      |
| 0x12   | iPod                    |
| 0x20   | Vidéo basée sur fichier |
| 0x21   | Vidéo DVD               |
| 0x22   | Autre vidéo             |
| 0x30   | AUX / autre             |
| 0x40   | Téléphone               |

---

### 16) ICON – `0xC1` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC1` | Type de données    |
| Length   | `0x01` | Longueur = 1 octet |

#### Data0 – Drapeaux source / mode de lecture

- **Bit7..Bit3** : réservés  
- **Bit2..Bit1** : Mode de lecture (v1.02)  
  - `0x00` : Normal  
  - `0x01` : Scan (CD/DVD/TUNER)  
  - `0x02` : Mix (CD/DVD uniquement)  
  - `0x03` : Repeat (CD/DVD uniquement)
- **Bit0** : réservé

---

### 17) Informations tuner (radio) – `0xC2` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC2` | Type de données    |
| Length   | `0x04` | Longueur = 4 octets |

| Octet | Description                                 |
|-------|---------------------------------------------|
| Data0 | Bande radio                                 |
| Data1 | Fréquence courante LSB                      |
| Data2 | Fréquence courante MSB                      |
| Data3 | Index de présélection                       |

#### Data0 – Bande

| Valeur | Bande |
|--------|-------|
| 0x00   | FM    |
| 0x01   | FM1   |
| 0x02   | FM2   |
| 0x03   | FM3   |
| 0x10   | AM    |
| 0x11   | AM1   |
| 0x12   | AM2   |
| 0x13   | AM3   |

Fréquence :

- FM : `Freq(MHz) = X / 100`  
- AM : `Freq(kHz) = X`

Présélection :

- `Data3 = 0..6`, où 0 signifie *non présélectionné*.

---

### 18) Informations de lecture média – `0xC3` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC3` | Type de données    |
| Length   | `0x06` | Longueur = 6 octets |

| Octet | Description |
|-------|-------------|
| Data0 | Info1       |
| Data1 | Info2       |
| Data2 | Info3       |
| Data3 | Info4       |
| Data4 | Info5       |
| Data5 | Info6       |

L’interprétation de Info1..Info6 dépend du **type de média**.

#### Formats spécifiques par type de média

| Type de média | Description               | Info1                                   | Info2                                   | Info3                                   | Info4                                   | Info5        | Info6        |
|---------------|---------------------------|-----------------------------------------|-----------------------------------------|-----------------------------------------|-----------------------------------------|--------------|--------------|
| `0x01`        | Tuner                     | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé  | Non utilisé  |
| `0x10`        | Média audio simple        | Numéro de disque (`0x00` = pas de disque) | Numéro de piste (`0x01–0xFF`)        | Non utilisé                             | Non utilisé                             | Minute       | Seconde      |
| `0x11`        | Média audio avancé        | Numéro de dossier octet faible          | Numéro de dossier octet fort           | Numéro de fichier octet faible          | Numéro de fichier octet fort            | Minute       | Seconde      |
| `0x12`        | iPod                      | Numéro de morceau courant (octet faible) | Numéro de morceau courant (octet fort) | Nombre total de morceaux (octet faible) | Nombre total de morceaux (octet fort)   | Non utilisé  | Non utilisé  |
| `0x20`        | Vidéo basée fichier       | Numéro de dossier octet faible          | Numéro de dossier octet fort           | Numéro de fichier octet faible          | Numéro de fichier octet fort            | Non utilisé  | Non utilisé  |
| `0x21`        | Vidéo DVD                 | Chapitre courant                        | Nombre total de chapitres              | Non utilisé                             | Non utilisé                             | Minute       | Seconde      |
| `0x22`        | Autre vidéo               | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé  | Non utilisé  |
| `0x30`        | AUX / autre               | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé  | Non utilisé  |
| `0x40`        | Téléphone                 | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé                             | Non utilisé  | Non utilisé  |

---

### 19) Contrôle d’affichage du volume – `0xC4` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC4` | Type de données    |
| Length   | `0x01` | Longueur = 1 octet |

#### Data0 – Affichage du volume

- **Bit7** : Contrôle mute  
  - 1 : muet  
  - 0 : non muet
- **Bit6..Bit0** : Valeur de volume, plage valide 0–127

---

### 20) Contrôle du volume radar – `0xC6` (HOST → SLAVE)

| Champ    | Valeur | Description        |
|----------|--------|--------------------|
| DataType | `0xC6` | Type de données    |
| Length   | `0x02` | Longueur = 2 octets |

| Octet | Description |
|-------|-------------|
| Data0 | Commande    |
| Data1 | Paramètre   |

#### Table des commandes

| Index | Commande | Description                            | Valeurs de paramètre                            |
|-------|----------|----------------------------------------|-------------------------------------------------|
| 1     | 0x00     | Contrôle du son radar de recul         | 0x00 : désactiver le son du radar de recul<br>0x01 : activer le son du radar de recul |
| 2     | 0x02     | Mode de stationnement                  | 0x00 : Mode 1 (standard)<br>0x01 : Mode 2 (stationnement le long du trottoir)<br>Voir le manuel d’utilisation du véhicule pour plus de détails |

