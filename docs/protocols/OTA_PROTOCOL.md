# OTA Firmware Update Protocol

Documentation du protocole de mise à jour firmware via USB série pour l'application Android.

## Vue d'ensemble

Le protocole OTA permet de mettre à jour le firmware de l'ESP32 via la connexion USB série. Il utilise l'encodage **base64** pour transférer les données binaires de manière fiable.

**Performance** : ~40 KB/s (firmware de 450KB en ~11 secondes)

## Paramètres de connexion

| Paramètre | Valeur |
|-----------|--------|
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |

> Note: L'ESP32-C3 utilise USB CDC, donc le baud rate est ignoré (USB Full Speed). Mais configurez 115200 pour compatibilité.

## Protocole

### Étape 1 : Démarrer la mise à jour

**Requête :**
```
OTA START <size> [md5]\n
```

| Paramètre | Type | Description |
|-----------|------|-------------|
| `size` | integer | Taille du firmware en bytes |
| `md5` | string (optionnel) | Hash MD5 du firmware (32 caractères hex) |

**Réponse (succès) :**
```
OK READY
OTA started: expecting <size> bytes
MD5 verification: <md5>
```

**Réponse (erreur) :**
```
ERROR: <message>
```

Erreurs possibles :
- `OTA already in progress` - Faire `OTA ABORT` d'abord
- `Invalid firmware size`
- `Firmware too large`

### Étape 2 : Envoyer les données

Envoyer le firmware en chunks encodés en base64.

**Requête :**
```
OTA DATA <base64_chunk>\n
```

| Paramètre | Type | Description |
|-----------|------|-------------|
| `base64_chunk` | string | Chunk de firmware encodé en base64 |

**Taille de chunk recommandée :** 180 bytes binaires = 240 caractères base64

> Important: La commande totale (`OTA DATA ` + base64 + `\n`) doit tenir dans 256 bytes.

**Réponse (succès) :**
```
OK <received>/<total> (<percent>%)
```

Exemple : `OK 1800/451488 (0%)`

**Réponse (erreur) :**
```
ERROR: <message>
```

Erreurs possibles :
- `No OTA in progress` - Refaire `OTA START`
- `Base64 decode failed`
- `Data exceeds expected size`
- `Write failed`

### Étape 3 : Finaliser la mise à jour

**Requête :**
```
OTA END\n
```

**Réponse (succès) :**
```
MD5 verified OK
OK
Firmware updated successfully!
Rebooting in 2 seconds...
```

L'ESP32 redémarre automatiquement après 2 secondes.

**Réponse (erreur) :**
```
ERROR: <message>
```

Erreurs possibles :
- `Incomplete data` - Tous les bytes n'ont pas été reçus
- `MD5 mismatch` - Le firmware est corrompu

### Commandes auxiliaires

#### Annuler une mise à jour
```
OTA ABORT\n
```
Réponse : `OTA aborted`

#### Vérifier le statut
```
OTA STATUS\n
```
Réponse :
```
=== OTA Status ===
Update in progress: YES/NO
Progress: <received> / <total> bytes (<percent>%)
Expected MD5: <md5>
Free sketch space: <bytes> bytes
Current firmware size: <bytes> bytes
==================
```

## Diagramme de séquence

```
┌─────────┐                              ┌─────────┐
│   App   │                              │  ESP32  │
└────┬────┘                              └────┬────┘
     │                                        │
     │  OTA START 451488 abc123...def456      │
     │───────────────────────────────────────>│
     │                                        │
     │  OK READY                              │
     │<───────────────────────────────────────│
     │                                        │
     │  OTA DATA 6QYCL+AHOEDuAAA...           │
     │───────────────────────────────────────>│
     │                                        │
     │  OK 180/451488 (0%)                    │
     │<───────────────────────────────────────│
     │                                        │
     │  OTA DATA U3YaB0rzSmI5WwG...           │
     │───────────────────────────────────────>│
     │                                        │
     │  OK 360/451488 (0%)                    │
     │<───────────────────────────────────────│
     │                                        │
     │           ... (répéter) ...            │
     │                                        │
     │  OTA DATA AAAAAAAANYjkUzl...           │
     │───────────────────────────────────────>│
     │                                        │
     │  OK 451488/451488 (100%)               │
     │<───────────────────────────────────────│
     │                                        │
     │  OTA END                               │
     │───────────────────────────────────────>│
     │                                        │
     │  MD5 verified OK                       │
     │  OK                                    │
     │<───────────────────────────────────────│
     │                                        │
     │         [ESP32 reboot]                 │
     │                                        │
```

## Exemple d'implémentation (Kotlin)

```kotlin
class OtaUpdater(private val serialPort: UsbSerialPort) {

    private val CHUNK_SIZE = 180  // bytes binaires

    suspend fun updateFirmware(firmware: ByteArray): Result<Unit> {
        val md5 = firmware.md5Hex()

        // 1. Start OTA
        sendCommand("OTA START ${firmware.size} $md5")
        val startResponse = readResponse()
        if (!startResponse.startsWith("OK")) {
            return Result.failure(OtaException("Start failed: $startResponse"))
        }

        // 2. Send data chunks
        var sent = 0
        while (sent < firmware.size) {
            val chunk = firmware.copyOfRange(sent, minOf(sent + CHUNK_SIZE, firmware.size))
            val base64 = Base64.encodeToString(chunk, Base64.NO_WRAP)

            sendCommand("OTA DATA $base64")
            val dataResponse = readResponse()

            if (!dataResponse.startsWith("OK")) {
                sendCommand("OTA ABORT")
                return Result.failure(OtaException("Data failed: $dataResponse"))
            }

            sent += chunk.size
            onProgress(sent, firmware.size)
        }

        // 3. Finalize
        sendCommand("OTA END")
        val endResponse = readResponse()

        if (!endResponse.contains("OK")) {
            return Result.failure(OtaException("End failed: $endResponse"))
        }

        return Result.success(Unit)
    }

    private fun sendCommand(cmd: String) {
        serialPort.write("$cmd\n".toByteArray(), 1000)
    }

    private fun readResponse(timeout: Long = 5000): String {
        val buffer = ByteArray(512)
        val result = StringBuilder()
        val deadline = System.currentTimeMillis() + timeout

        while (System.currentTimeMillis() < deadline) {
            val len = serialPort.read(buffer, 100)
            if (len > 0) {
                val text = String(buffer, 0, len)
                result.append(text)

                // Check for complete response
                val lines = result.toString().lines()
                for (line in lines) {
                    if (line.startsWith("OK") || line.startsWith("ERROR")) {
                        return line
                    }
                }
            }
        }
        throw TimeoutException("No response within ${timeout}ms")
    }

    private fun ByteArray.md5Hex(): String {
        val md = MessageDigest.getInstance("MD5")
        return md.digest(this).joinToString("") { "%02x".format(it) }
    }
}
```

## Gestion des erreurs

### Timeout
Si aucune réponse n'est reçue dans les 10 secondes après une commande `OTA DATA`, considérez le transfert comme échoué et envoyez `OTA ABORT`.

### Déconnexion USB
Si la connexion USB est perdue pendant le transfert, l'OTA restera en attente. Utilisez `OTA ABORT` pour annuler manuellement, ou redémarrez le device.

### Reprise après échec
Il n'y a pas de mécanisme de reprise. En cas d'échec, il faut recommencer depuis le début avec `OTA START`.

## Vérification du device

Avant de lancer une mise à jour, utilisez `SYS INFO` pour vérifier le type de device :

```
>> SYS INFO
<< === System Info ===
<< Firmware: 1.7.1 (2026-01-28)
<< Uptime: 123 sec
<< Free heap: 234567 bytes
<< CPU freq: 160 MHz
<< Chip: ESP32-C3 rev0.4
<< ===================
```

Cela permet de s'assurer qu'on envoie le bon firmware pour le bon chip.

## Notes importantes

1. **Attendre la réponse** : Toujours attendre la réponse `OK` avant d'envoyer le chunk suivant.

2. **Pas de parallélisation** : Le protocole est strictement séquentiel.

3. **MD5 recommandé** : Toujours fournir le hash MD5 pour vérifier l'intégrité.

4. **Reboot automatique** : L'ESP32 redémarre automatiquement après `OTA END`. La connexion USB sera temporairement perdue.

5. **Buffer size** : Le buffer de commande est de 256 bytes. Ne pas dépasser cette limite.

6. **Chip unique** : Actuellement seul l'ESP32-C3 est supporté.
