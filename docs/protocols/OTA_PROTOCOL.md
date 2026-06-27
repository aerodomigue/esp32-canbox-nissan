# OTA Firmware Update Protocol — v2

Documentation du protocole de mise à jour firmware via USB série pour l'application Android.

> **Application requise** : [ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager)

## Changements par rapport à v1

| Item | v1 | v2 |
|------|----|----|
| Réponse `OTA START` | `OK READY` en premier, puis lignes info | Lignes info d'abord, `OK READY` en **dernier** |
| Écho série | Chaque caractère reçu est renvoyé | **Aucun écho** pendant l'OTA |
| Vérification par chunk | Aucune (seulement MD5 global à la fin) | CRC32 optionnel par chunk (retry safe) |
| Timeout auto-abort | Aucun | **60 secondes** sans `OTA DATA` → abort automatique |
| Pre-flight Android | Aucun | Envoyer `OTA ABORT` avant `OTA START` |
| Retry chunk | Non supporté | CRC mismatch → retry possible (timeout → restart complet) |

---

## Paramètres de connexion

| Paramètre | Valeur |
|-----------|--------|
| Baud rate | 115200 (ignoré par USB CDC) |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Line ending envoyé | `\n` (LF) |
| Line ending reçu | `\n` ou `\r\n` (trim avant parsing) |

---

## Protocole

### Pre-flight (nouveau v2)

Avant chaque session OTA, envoyer `OTA ABORT` pour effacer tout état bloqué.

```
OTA ABORT\n
```

Réponse (ignorée — peut arriver ou non selon l'état actuel) :
```
OTA aborted
```

Après envoi, attendre 1 seconde en drainant le buffer RX avant de continuer.

---

### Étape 1 : Démarrer la mise à jour

**Requête :**
```
OTA START <size> [md5]\n
```

| Paramètre | Type | Description |
|-----------|------|-------------|
| `size` | integer | Taille du firmware en bytes |
| `md5` | string (optionnel) | Hash MD5 du firmware (32 hex chars) |

**Réponse (succès) v2 — info lines AVANT OK READY :**
```
OTA starting: expecting <size> bytes\n
MD5: <md5>\n                           ← absent si aucun MD5 fourni
OK READY\n                             ← ligne terminale à détecter
```

**Important :** `OK READY` est la **dernière** ligne de la réponse. Le client Android doit lire jusqu'à trouver une ligne commençant par `OK` ou `ERROR` — les lignes précédentes sont informatives.

**Réponse (erreur) :**
```
ERROR: <message>\n
```

Erreurs possibles :
- `OTA already in progress` — envoyer `OTA ABORT` d'abord
- `Invalid firmware size`
- `Firmware too large`
- `Update.begin failed`

---

### Étape 2 : Envoyer les données

Envoyer le firmware en chunks encodés en base64.

**Requête :**
```
OTA DATA <base64_chunk> [crc32_hex]\n
```

| Paramètre | Type | Description |
|-----------|------|-------------|
| `base64_chunk` | string | Chunk firmware en base64 (max 240 chars) |
| `crc32_hex` | string (optionnel) | CRC32 du chunk binaire, format hex 8 chars |

**Taille de chunk recommandée :** 180 bytes binaires = 240 chars base64

> La commande totale (`OTA DATA ` + 240 base64 + ` ` + 8 CRC32 + `\n`) = 259 chars, dans la limite des 320 bytes du buffer.

**Écho désactivé :** l'ESP32 n'écho plus les caractères reçus pendant l'OTA. La seule réponse est la ligne `OK` ou `ERROR`.

**Réponse (succès) :**
```
OK <received>/<total> (<percent>%)\n
```
Exemple : `OK 180/451488 (0%)`

**Réponse (erreur CRC — chunk non écrit, retry possible) :**
```
ERROR: CRC mismatch chunk (got <actual_hex> expected <expected_hex>)\n
```

**Comportement sur CRC mismatch :**
- Le chunk n'est **pas écrit** en flash
- `otaReceivedSize` est **inchangé**
- L'OTA reste active
- Le client Android peut **retransmettre le même chunk** immédiatement (sûr)

**Autres erreurs (OTA abortée) :**
```
ERROR: <message>\n
```
Si une erreur autre que CRC mismatch survient, l'OTA est abortée automatiquement. Envoyer `OTA ABORT` puis recommencer depuis le début.

---

### Étape 3 : Finaliser la mise à jour

**Requête :**
```
OTA END\n
```

**Réponse (succès) :**
```
MD5 verified OK\n
OK\n
Firmware updated successfully!\n
Rebooting in 2 seconds...\n
```

Le client doit détecter la ligne `OK` (pas la ligne `MD5 verified OK`). L'ESP32 redémarre automatiquement 2 secondes après.

**Réponse (erreur) :**
```
ERROR: <message>\n
```

---

### Commandes auxiliaires

#### Annuler une mise à jour
```
OTA ABORT\n
```
Réponse : `OTA aborted` (pas de préfixe OK/ERROR — à ignorer ou drainer)

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

---

## Timeout auto-abort (nouveau v2)

Si aucune commande `OTA DATA` n'est reçue pendant **60 secondes** après `OTA START` ou le dernier `OTA DATA`, l'ESP32 abandonne automatiquement l'OTA :

```
OTA timeout: no data for 60s, aborting\n
OTA aborted\n
```

Le client Android doit relancer une session complète depuis le pre-flight.

---

## Diagramme de séquence v2

```
┌─────────┐                                        ┌─────────┐
│ Android │                                        │  ESP32  │
└────┬────┘                                        └────┬────┘
     │                                                  │
     │  OTA ABORT\n              (pre-flight)           │
     │─────────────────────────────────────────────────>│
     │  OTA aborted  (ou silence si idle)               │
     │<─────────────────────────────────────────────────│
     │ [drain 1s]                                       │
     │                                                  │
     │  OTA START 451488 abc123...def456\n              │
     │─────────────────────────────────────────────────>│
     │                                                  │ Update.begin()
     │  OTA starting: expecting 451488 bytes\n          │ Flush RX buffer
     │<─────────────────────────────────────────────────│
     │  MD5: abc123...def456\n                          │
     │<─────────────────────────────────────────────────│
     │  OK READY\n                ← ligne terminale     │
     │<─────────────────────────────────────────────────│
     │                                                  │
     │  OTA DATA <240b64> <8crc32>\n  (aucun écho)     │
     │─────────────────────────────────────────────────>│
     │                                                  │ CRC32 check
     │                                                  │ WDT reset
     │                                                  │ Update.write() [20-100ms]
     │                                                  │ WDT reset
     │  OK 180/451488 (0%)\n                            │
     │<─────────────────────────────────────────────────│
     │                                                  │
     │  OTA DATA <240b64> <8crc32>\n  (CRC invalide)   │
     │─────────────────────────────────────────────────>│
     │  ERROR: CRC mismatch chunk ...\n ← rien écrit   │
     │<─────────────────────────────────────────────────│
     │  [retry même chunk]                              │
     │  OTA DATA <240b64> <8crc32>\n  (CRC correct)    │
     │─────────────────────────────────────────────────>│
     │  OK 360/451488 (0%)\n                            │
     │<─────────────────────────────────────────────────│
     │                                                  │
     │           ... (répéter ~2508 chunks) ...         │
     │                                                  │
     │  OTA DATA <240b64> <8crc32>\n                   │
     │─────────────────────────────────────────────────>│
     │  OK 451488/451488 (100%)\n                       │
     │<─────────────────────────────────────────────────│
     │                                                  │
     │  OTA END\n                                       │
     │─────────────────────────────────────────────────>│
     │                                                  │ MD5 final
     │                                                  │ Update.end()
     │                                                  │ esp_ota_mark_valid
     │  MD5 verified OK\n                               │
     │<─────────────────────────────────────────────────│
     │  OK\n                    ← ligne terminale       │
     │<─────────────────────────────────────────────────│
     │                                                  │ delay(2s) → restart
     │  [USB déconnecte ~2s plus tard]                  │
```

---

## Implémentation Kotlin (v2)

```kotlin
import android.util.Base64
import java.io.IOException
import java.security.MessageDigest
import java.util.zip.CRC32

class OtaUpdater(private val serialPort: UsbSerialPort) {

    companion object {
        const val CHUNK_SIZE = 180          // bytes binaires par chunk
        const val START_TIMEOUT_MS = 3000L
        const val CHUNK_TIMEOUT_MS = 5000L
        const val ABORT_DRAIN_MS = 1000L
        const val READ_POLL_MS = 50L
        const val MAX_CRC_RETRIES = 3
        const val CRC_RETRY_PAUSE_MS = 200L
    }

    suspend fun updateFirmware(
        firmware: ByteArray,
        onProgress: (sent: Int, total: Int) -> Unit
    ): Result<Unit> = withContext(Dispatchers.IO) {

        val md5 = firmware.md5Hex()

        // 1. Pre-flight : effacer tout état bloqué
        sendRaw("OTA ABORT\n")
        drainUntilQuiet(ABORT_DRAIN_MS)

        // 2. Démarrer
        sendRaw("OTA START ${firmware.size} $md5\n")
        val startResp = readResponse(START_TIMEOUT_MS)
            ?: return@withContext Result.failure(OtaException("OTA START timeout"))
        if (!startResp.startsWith("OK")) {
            return@withContext Result.failure(OtaException("OTA START failed: $startResp"))
        }

        // 3. Envoi des chunks avec retry sur CRC mismatch
        var sent = 0
        var chunkIdx = 0
        val total = firmware.size

        while (sent < total) {
            val chunk = firmware.copyOfRange(sent, minOf(sent + CHUNK_SIZE, total))
            val b64 = Base64.encodeToString(chunk, Base64.NO_WRAP)
            val crc = chunk.crc32Hex()
            val command = "OTA DATA $b64 $crc\n"

            var chunkAccepted = false

            for (attempt in 1..MAX_CRC_RETRIES) {
                sendRaw(command)
                val resp = readResponse(CHUNK_TIMEOUT_MS)

                when {
                    resp == null -> {
                        // Timeout : le flash a PEUT-ÊTRE été écrit → restart complet
                        sendRaw("OTA ABORT\n")
                        return@withContext Result.failure(
                            OtaException("Chunk $chunkIdx timeout — restart OTA from scratch")
                        )
                    }
                    resp.startsWith("OK") -> {
                        chunkAccepted = true
                        break
                    }
                    resp.contains("CRC mismatch", ignoreCase = true) -> {
                        // Rien n'a été écrit → retry du même chunk est sûr
                        if (attempt == MAX_CRC_RETRIES) {
                            sendRaw("OTA ABORT\n")
                            return@withContext Result.failure(
                                OtaException("Chunk $chunkIdx CRC failed $MAX_CRC_RETRIES times")
                            )
                        }
                        delay(CRC_RETRY_PAUSE_MS)
                    }
                    resp.startsWith("ERROR") -> {
                        sendRaw("OTA ABORT\n")
                        return@withContext Result.failure(
                            OtaException("Chunk $chunkIdx rejected: $resp")
                        )
                    }
                }
            }

            if (!chunkAccepted) {
                sendRaw("OTA ABORT\n")
                return@withContext Result.failure(OtaException("Chunk $chunkIdx failed"))
            }

            sent += chunk.size
            chunkIdx++
            onProgress(sent, total)
        }

        // 4. Finaliser
        sendRaw("OTA END\n")
        val endResp = readResponse(CHUNK_TIMEOUT_MS)
        if (endResp == null || !endResp.startsWith("OK")) {
            return@withContext Result.failure(
                OtaException("OTA END failed: ${endResp ?: "timeout"}")
            )
        }

        Result.success(Unit)
        // L'ESP32 redémarre dans ~2s, l'USB se déconnecte
    }

    /**
     * Lit depuis le port série jusqu'à trouver une ligne commençant par OK ou ERROR.
     * Toutes les lignes précédentes (info lines) sont ignorées.
     * Retourne null en cas de timeout.
     */
    private fun readResponse(timeoutMs: Long): String? {
        val buffer = ByteArray(512)
        val accumulator = StringBuilder()
        val deadline = System.currentTimeMillis() + timeoutMs

        while (System.currentTimeMillis() < deadline) {
            val len = try {
                serialPort.read(buffer, READ_POLL_MS.toInt())
            } catch (e: IOException) {
                return null
            }

            if (len > 0) {
                accumulator.append(String(buffer, 0, len, Charsets.UTF_8))
            }

            // Chercher la ligne terminale dans toutes les lignes accumulées
            for (line in accumulator.toString().split('\n')) {
                val trimmed = line.trim()
                if (trimmed.startsWith("OK") || trimmed.startsWith("ERROR")) {
                    return trimmed
                }
            }
        }

        return null
    }

    private suspend fun drainUntilQuiet(timeoutMs: Long) {
        val buf = ByteArray(256)
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            try { serialPort.read(buf, 100) } catch (_: IOException) { break }
            delay(10)
        }
    }

    private fun sendRaw(cmd: String) {
        serialPort.write(cmd.toByteArray(Charsets.UTF_8), 1000)
    }

    private fun ByteArray.md5Hex(): String {
        val md = MessageDigest.getInstance("MD5")
        return md.digest(this).joinToString("") { "%02x".format(it) }
    }

    private fun ByteArray.crc32Hex(): String {
        val crc = CRC32().also { it.update(this) }
        return "%08x".format(crc.value)
    }
}

class OtaException(message: String) : Exception(message)
```

---

## Gestion des erreurs

| Situation | Retry chunk ? | Action |
|-----------|:---:|-------|
| START timeout | Non | Afficher erreur, proposer retry complet |
| START ERROR | Non | Afficher message d'erreur |
| Chunk CRC mismatch | **Oui (3x max)** | Retransmettre le même chunk — sûr (rien écrit) |
| Chunk timeout | Non | `OTA ABORT` → restart complet |
| Chunk ERROR (autre) | Non | `OTA ABORT` → restart complet |
| END timeout/ERROR | Non | Attendre 5s, reconnecter, vérifier `SYS INFO` |
| IOException USB | Non | Reconnecter → `OTA ABORT` → restart |
| Auto-abort 60s | Non | L'ESP32 a déjà aborté → restart depuis pre-flight |

---

## Vérification du device avant mise à jour

Avant de lancer l'OTA, vérifier le firmware actuel avec `SYS INFO` :

```
>> SYS INFO
<< === System Info ===
<< Firmware: 1.8.0 (2026-06-27)
<< Uptime: 123 sec
<< Free heap: 234567 bytes
<< CPU freq: 160 MHz
<< Chip: ESP32-C3 rev0.4
<< ===================
```

Vérifier que le chip est `ESP32-C3` avant d'envoyer un firmware compilé pour C3.

---

## Tests du protocole

Un script de test d'intégration est disponible dans `tools/test_ota_protocol.py` :

```bash
# Installer la dépendance
pip install pyserial

# Exécuter les tests (sans test de timeout)
python tools/test_ota_protocol.py /dev/ttyACM0 --skip-slow

# Avec le test de timeout (prend ~65s)
python tools/test_ota_protocol.py /dev/ttyACM0
```

---

## Notes importantes

1. **Attendre la réponse** : toujours attendre la ligne terminale (`OK`/`ERROR`) avant d'envoyer le chunk suivant.
2. **CRC32 recommandé** : toujours inclure le CRC32 dans `OTA DATA` pour permettre le retry sur corruption.
3. **MD5 recommandé** : toujours fournir le hash MD5 dans `OTA START` pour vérifier l'intégrité globale.
4. **Reboot automatique** : l'ESP32 redémarre après `OTA END` réussi — la connexion USB sera perdue ~2s plus tard.
5. **Pas de parallélisation** : le protocole est strictement séquentiel (stop-and-wait).
6. **Buffer size** : le buffer de commande est de 320 bytes. Ne pas dépasser 240 chars de base64 + 8 chars CRC32 par `OTA DATA`.
