# Tools

Outils de développement et test pour le bridge CAN ESP32.

## can_inject.py

Script Python pour injecter des trames CAN via le port série USB, permettant de tester le firmware sans connexion au véhicule.

### Installation

```bash
pip install pyserial
```

### Format des fichiers de capture

Le script lit les fichiers au format export du firmware :

```
RX ID: 0x182 | DLC: 8 | Data: 00 00 00 00 00 35 00 CE
RX ID: 0x355 | DLC: 7 | Data: 00 00 FF FF 00 00 40
RX ID: 0x5C5 | DLC: 8 | Data: 40 03 A9 48 00 0C 00 7F
```

### Usage

```bash
# Lister les ports série disponibles
python can_inject.py --list-ports

# Injecter toutes les trames (délai 10ms par défaut)
python can_inject.py /dev/ttyUSB0 data/nissan\ trame\ export.txt

# Délai personnalisé (50ms entre chaque trame)
python can_inject.py /dev/ttyUSB0 data/nissan\ trame\ export.txt --delay 50

# Filtrer par IDs CAN (hex)
python can_inject.py /dev/ttyUSB0 data/nissan\ trame\ export.txt --filter 180,5C5,60D,002

# Boucle continue (simule un vrai bus CAN)
python can_inject.py /dev/ttyUSB0 data/nissan\ trame\ export.txt --loop

# Mode silencieux (pas d'affichage trame par trame)
python can_inject.py /dev/ttyUSB0 data/nissan\ trame\ export.txt --quiet
```

### Options

| Option | Description |
|--------|-------------|
| `--delay`, `-d` | Délai entre trames en ms (défaut: 10) |
| `--filter`, `-f` | IDs CAN à injecter, séparés par virgule (ex: `180,5C5,60D`) |
| `--loop`, `-l` | Boucle infinie sur le fichier |
| `--quiet`, `-q` | Supprime l'affichage des trames |
| `--list-ports` | Liste les ports série disponibles |

### Injection manuelle via terminal

Tu peux aussi envoyer des trames manuellement via un terminal série (115200 baud) :

```
>180 04 FB 36 23 4C 00 36 00
>5C5 40 03 A9 48 00 0C 00 7F
>002 00 0B 60
```

Format : `>ID DATA...` où ID est en hex et DATA sont les octets en hex séparés par des espaces.

## data/

Captures CAN pour les tests.

| Fichier | Description |
|---------|-------------|
| `nissan trame export.txt` | Capture complète du bus CAN Nissan Juke F15 |
