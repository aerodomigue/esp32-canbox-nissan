# CAN Configuration Protocol

Documentation du protocole de gestion des configurations CAN via USB série.

> **Application requise** : [ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager)

## Vue d'ensemble

Le CANBox stocke les configurations véhicule en fichiers JSON sur le filesystem interne (LittleFS). Ce protocole permet de :
- Lister les configurations disponibles
- Charger/activer une configuration
- Uploader de nouvelles configurations
- Supprimer des configurations
- Exporter la configuration active

## Commandes

### CAN STATUS

Affiche l'état actuel de la configuration CAN.

```
>> CAN STATUS
<< === CAN Configuration Status ===
<< Config: MockDemo.json
<< Mode: MOCK (simulated data)
<< Profile: Mock Demo
<< Frames processed: 0
<< Unknown frames: 0
<< ================================
```

| Champ | Description |
|-------|-------------|
| Config | Nom du fichier de configuration actif |
| Mode | `MOCK` (données simulées) ou `REAL` (bus CAN réel) |
| Profile | Nom du profil véhicule |
| Frames processed | Nombre de trames CAN traitées |
| Unknown frames | Trames CAN non reconnues |

---

### CAN LIST

Liste tous les fichiers de configuration disponibles.

```
>> CAN LIST
<< === CAN Config Files ===
<<   MockDemo.json (60 bytes)
<<   NissanJukeF15.json (2048 bytes)
<< Total: 2 file(s)
<< ========================
```

---

### CAN GET

Retourne le contenu JSON de la configuration actuellement chargée.

```
>> CAN GET
<< === MockDemo.json ===
<< {
<<   "name": "Mock Demo",
<<   "isMock": true,
<<   "frames": []
<< }
<< === END ===
```

**Erreurs possibles :**
- `ERROR: No config file loaded`
- `ERROR: Config file not found on filesystem`

---

### CAN LOAD \<filename\>

Charge et active une configuration.

```
>> CAN LOAD NissanJukeF15.json
<< OK
<< Loaded: Nissan Juke F15
<< Mode: REAL
```

**Erreurs possibles :**
- `ERROR: File not found`
- `ERROR: Failed to parse config file`

> Note: La configuration est sauvegardée en NVS et sera restaurée au prochain démarrage.

---

### CAN RELOAD

Recharge la configuration depuis le filesystem (utile après un upload).

```
>> CAN RELOAD
<< Reloading CAN configuration...
<< [CanConfig] Restoring saved config: /MockDemo.json
<< [CanConfig] Loaded: Mock Demo (0 frames) - MOCK mode
<< OK
<< Loaded: Mock Demo (MOCK mode)
```

---

### CAN DELETE \<filename\>

Supprime un fichier de configuration.

```
>> CAN DELETE TestConfig.json
<< OK
<< Deleted: /TestConfig.json
```

**Erreurs possibles :**
- `ERROR: File not found`
- `ERROR: Failed to delete file`

> ⚠️ Ne pas supprimer la configuration actuellement active !

---

### CAN UPLOAD (Protocole multi-étapes)

Upload d'une nouvelle configuration en base64.

#### Étape 1 : Démarrer l'upload

```
>> CAN UPLOAD START <filename> <size>
<< OK READY
<< Awaiting <size> bytes for /<filename>
```

| Paramètre | Description |
|-----------|-------------|
| filename | Nom du fichier (ex: `MyVehicle.json`) |
| size | Taille en bytes du JSON |

#### Étape 2 : Envoyer les données

```
>> CAN UPLOAD DATA <base64_chunk>
<< OK <received>/<total>
```

Envoyer le JSON encodé en base64. Peut être fait en plusieurs chunks.

**Taille de chunk recommandée :** 180 bytes binaires = 240 caractères base64

> Important: La commande totale (`CAN UPLOAD DATA ` + base64 + `\n`) doit tenir dans 320 bytes.

#### Étape 3 : Finaliser

```
>> CAN UPLOAD END
<< OK
<< Saved: /MyVehicle.json (1234 bytes)
<< Use 'CAN RELOAD' to activate
```

**Erreurs possibles :**
- `ERROR: JSON parse failed: <reason>`
- `ERROR: Invalid config: missing 'name' or 'frames'`

#### Annuler un upload

```
>> CAN UPLOAD ABORT
<< Upload aborted
```

---

## Diagramme : Upload d'une configuration

```
┌─────────┐                              ┌─────────┐
│   App   │                              │  ESP32  │
└────┬────┘                              └────┬────┘
     │                                        │
     │  CAN UPLOAD START MyVehicle.json 256   │
     │───────────────────────────────────────>│
     │                                        │
     │  OK READY                              │
     │<───────────────────────────────────────│
     │                                        │
     │  CAN UPLOAD DATA eyJuYW1lIjoi...       │
     │───────────────────────────────────────>│
     │                                        │
     │  OK 256/256                            │
     │<───────────────────────────────────────│
     │                                        │
     │  CAN UPLOAD END                        │
     │───────────────────────────────────────>│
     │                                        │
     │  OK                                    │
     │  Saved: /MyVehicle.json                │
     │<───────────────────────────────────────│
     │                                        │
     │  CAN LOAD MyVehicle.json               │
     │───────────────────────────────────────>│
     │                                        │
     │  OK                                    │
     │  Loaded: My Vehicle                    │
     │<───────────────────────────────────────│
```

---

## Format JSON des configurations

```json
{
  "name": "Nissan Juke F15",
  "isMock": false,
  "frames": [
    {
      "canId": "0x002",
      "fields": [
        {
          "target": "STEERING",
          "startByte": 0,
          "byteCount": 2,
          "byteOrder": "BE",
          "dataType": "INT16",
          "formula": "SCALE",
          "params": [1, 10, 0]
        }
      ]
    }
  ]
}
```

Voir la documentation complète du format dans [USB_SERIAL_PROTOCOL.md](USB_SERIAL_PROTOCOL.md).

---

## Exemple d'implémentation (Kotlin)

```kotlin
class CanConfigManager(private val serial: UsbSerialPort) {

    fun listConfigs(): List<String> {
        sendCommand("CAN LIST")
        val response = readUntil("========================")
        return parseFileList(response)
    }

    fun loadConfig(filename: String): Boolean {
        sendCommand("CAN LOAD $filename")
        val response = readResponse()
        return response.startsWith("OK")
    }

    fun getCurrentConfig(): String? {
        sendCommand("CAN GET")
        val response = readUntil("=== END ===")
        return extractJson(response)
    }

    fun uploadConfig(filename: String, json: String): Boolean {
        val bytes = json.toByteArray()
        val base64 = Base64.encodeToString(bytes, Base64.NO_WRAP)

        // Start
        sendCommand("CAN UPLOAD START $filename ${bytes.size}")
        if (!readResponse().startsWith("OK READY")) return false

        // Data
        sendCommand("CAN UPLOAD DATA $base64")
        if (!readResponse().startsWith("OK")) return false

        // End
        sendCommand("CAN UPLOAD END")
        if (!readResponse().startsWith("OK")) return false

        return true
    }

    fun deleteConfig(filename: String): Boolean {
        sendCommand("CAN DELETE $filename")
        return readResponse().startsWith("OK")
    }

    private fun sendCommand(cmd: String) {
        serial.write("$cmd\n".toByteArray(), 1000)
    }

    private fun readResponse(): String {
        // Read until OK or ERROR line
        val buffer = ByteArray(512)
        val result = StringBuilder()
        val deadline = System.currentTimeMillis() + 2000

        while (System.currentTimeMillis() < deadline) {
            val len = serial.read(buffer, 100)
            if (len > 0) {
                result.append(String(buffer, 0, len))
                val lines = result.toString().lines()
                for (line in lines) {
                    if (line.startsWith("OK") || line.startsWith("ERROR")) {
                        return line
                    }
                }
            }
        }
        return result.toString()
    }
}
```

---

## Notes importantes

1. **Taille max** : Les fichiers de configuration sont limités à 8 KB.

2. **Persistence** : La configuration active est sauvegardée en NVS et restaurée au boot.

3. **Validation** : Le JSON est validé lors de l'upload. Les champs `name` et `frames` sont obligatoires.

4. **Mode Mock** : Utilisez `"isMock": true` pour tester sans véhicule connecté.
