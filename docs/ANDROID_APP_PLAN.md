# Plan de Développement - Application Android CANBox

> **NOTE IMPORTANTE pour Claude:**
> Ce document a été généré le 2026-01-26. Les versions des dépendances (Compose, Room, Retrofit, etc.)
> listées ci-dessous peuvent être obsolètes. **Vérifie les dernières versions stables** avant de commencer
> l'implémentation. Utilise les sites officiels ou `mvnrepository.com` pour les versions à jour.

## Vue d'ensemble

Application Android pour configurer et monitorer l'ESP32 CANBox via USB série.

**Écran cible:** 1280x720 (720p paysage) - Autoradio Android

**Stack technique proposé:**
- Kotlin
- Jetpack Compose (UI moderne)
- Material Design 3
- usb-serial-for-android (communication USB)
- Retrofit/Ktor (GitHub API pour updates)
- Room (stockage local logs)

---

## Architecture des Écrans

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  [Live] [CAN Config] [Calibration] [Update] [Debug]      🔌 Connecté   v1.7.0│
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                           Contenu de l'écran actif                           │
│                              (1280 x ~650 px)                                │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Layout paysage 720p (1280x720):**
- Navigation horizontale en haut (tabs)
- Status bar: connexion USB + version firmware
- Zone contenu: ~650px de hauteur utilisable

---

## Écran 1: Live Stats (Dashboard)

**But:** Afficher les données véhicule en temps réel

### UI Components (Paysage 1280x720)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌──────┐ │
│   │  2500   │  │   60    │  │  14.2V  │  │  85°C   │  │   30L   │  │ 350  │ │
│   │   RPM   │  │  km/h   │  │ Batterie│  │  Temp   │  │ Essence │  │ DTE  │ │
│   └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └──────┘ │
│                                                                              │
│   Direction: ←━━━━━━━━━━━━━━━━━━━━━━━●━━━━━━━━━━━━━━━━━━━━━━━→   -150°     │
│                                                                              │
│   ┌────────────────────────────────────┐  ┌────────────────────────────────┐│
│   │  Portes                            │  │  Éclairage                     ││
│   │  [■ AV-G] [□ AV-D] [□ AR-G]       │  │  💡 Codes  🔆 Phares  🔵 Veill ││
│   │  [□ AR-D] [□ Coffre]              │  │  ◀️ Cligno G    ▶️ Cligno D     ││
│   └────────────────────────────────────┘  └────────────────────────────────┘│
│                                                                              │
│   Mode: REAL (Nissan Juke F15)                          Frein à main: OFF   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Commandes USB utilisées
- `SYS DATA` - Polling toutes les 200-500ms
- Parse la réponse pour extraire les valeurs

### Data Model
```kotlin
data class VehicleData(
    val rpm: Int,
    val speed: Int,
    val voltage: Float,
    val temperature: Int,
    val fuelLevel: Int,
    val dte: Int,
    val steering: Int,
    val doors: DoorStatus,
    val lights: LightStatus
)
```

---

## Écran 2: CAN Config (Sélection Véhicule)

**But:** Gérer les profils véhicules (JSON configs)

### UI Components (Paysage 1280x720)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   Config active: Nissan Juke F15 ✓                    Mode: REAL            │
│                                                                              │
│   ┌─────────────────────────────────────┐  ┌─────────────────────────────────┐
│   │  ═══ Sur le device ═══              │  │  ═══ Disponibles (GitHub) ═══  │
│   │                                     │  │                                 │
│   │  ┌───────────────────────────────┐  │  │  ┌───────────────────────────┐ │
│   │  │ 📄 NissanJukeF15.json    ✓    │  │  │  │ 📥 NissanQashqai.json     │ │
│   │  │    2048 bytes     [Supprimer] │  │  │  │    [Télécharger]          │ │
│   │  └───────────────────────────────┘  │  │  └───────────────────────────┘ │
│   │                                     │  │                                 │
│   │  ┌───────────────────────────────┐  │  │  ┌───────────────────────────┐ │
│   │  │ 📄 MockDemo.json              │  │  │  │ 📥 NissanLeaf.json        │ │
│   │  │    512 bytes      [Supprimer] │  │  │  │    [Télécharger]          │ │
│   │  └───────────────────────────────┘  │  │  └───────────────────────────┘ │
│   │                                     │  │                                 │
│   │  [+ Importer fichier local]         │  │  [🔄 Rafraîchir]               │
│   │                                     │  │                                 │
│   └─────────────────────────────────────┘  └─────────────────────────────────┘
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Note:** Clic sur une config du device = la charge automatiquement

### Commandes USB utilisées
- `CAN STATUS` - Config active et mode (REAL/MOCK)
- `CAN LIST` - Liste des fichiers sur device
- `CAN LOAD <file>` - Charger une config
- `CAN DELETE <file>` - Supprimer une config
- `CAN UPLOAD START/DATA/END` - Upload nouvelle config
- `CAN GET` - Télécharger config actuelle (backup)

### Fonctionnalités
- [ ] Liste des configs sur le device
- [ ] Charger/activer une config
- [ ] Supprimer une config
- [ ] Upload depuis fichier local
- [ ] Download depuis GitHub (repo configs communautaires)
- [ ] Backup config actuelle vers fichier

---

## Écran 3: Calibration

**But:** Ajuster les paramètres de calibration (NVS)

### UI Components (Paysage 1280x720)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   ┌─────────────────────────────────────┐  ┌─────────────────────────────────┐
│   │  ═══ Direction ═══                  │  │  ═══ Moteur & Carburant ═══    │
│   │                                     │  │                                 │
│   │  Steering Offset         [  100  ]  │  │  RPM Divisor          [   7  ] │
│   │  ←━━━━━━━━━━━●━━━━━━━━━━━→         │  │  ←━━━●━━━━━━━━━━━━━━━━━→        │
│   │  -500                      +500     │  │  1                         20   │
│   │                                     │  │                                 │
│   │  Steering Scale (x0.0001)[  300  ]  │  │  Tank Capacity (L)    [  45  ] │
│   │  ←━━●━━━━━━━━━━━━━━━━━━━━→         │  │  ←━━━━━━━━━━━●━━━━━━━━━→        │
│   │  1                        20000     │  │  20                       100   │
│   │                                     │  │                                 │
│   │  Steering Invert    [ OFF | ON ✓]   │  │  DTE Divisor (x100)   [ 283  ] │
│   │                                     │  │  ←━━━━━━━━●━━━━━━━━━━━━→        │
│   │  Indicator Timeout (ms)  [  500  ]  │  │  100                       500  │
│   │  ←━━━━━━━━●━━━━━━━━━━━━━━→         │  │                                 │
│   │  100                       2000     │  │                                 │
│   │                                     │  │                                 │
│   └─────────────────────────────────────┘  └─────────────────────────────────┘
│                                                                              │
│                        [💾 Enregistrer]    [🔄 Réinitialiser]                │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Commandes USB utilisées
- `CFG LIST` - Charger toutes les valeurs actuelles
- `CFG SET <param> <value>` - Modifier une valeur
- `CFG SAVE` - Sauvegarder en NVS
- `CFG RESET` - Reset aux valeurs par défaut

### Fonctionnalités
- [ ] Charger valeurs actuelles au démarrage
- [ ] Sliders avec preview en temps réel
- [ ] Validation des ranges
- [ ] Bouton "Test" (applique sans sauvegarder)
- [ ] Bouton "Sauvegarder" (CFG SAVE)
- [ ] Bouton "Reset" avec confirmation

---

## Écran 4: Update (Firmware)

**But:** Mettre à jour le firmware ESP32

### UI Components (Paysage 1280x720)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   Version actuelle: 1.7.0 (2026-01-26)                                       │
│                                                                              │
│   ┌─────────────────────────────────────┐  ┌─────────────────────────────────┐
│   │  ═══ GitHub Releases ═══            │  │  ═══ Détails / Installation ═══│
│   │                                     │  │                                 │
│   │  ┌───────────────────────────────┐  │  │  v1.8.0 (Latest)               │
│   │  │ ⬇️ v1.8.0 (Latest)       NEW  │  │  │                                 │
│   │  └───────────────────────────────┘  │  │  Changelog:                     │
│   │  ┌───────────────────────────────┐  │  │  - Fix temperature display     │
│   │  │ ✓ v1.7.0 (Installée)          │  │  │  - Add new CAN frames          │
│   │  └───────────────────────────────┘  │  │  - Improve steering accuracy   │
│   │  ┌───────────────────────────────┐  │  │                                 │
│   │  │   v1.6.0                      │  │  │  Taille: 413 KB                │
│   │  └───────────────────────────────┘  │  │  Date: 2026-01-28              │
│   │                                     │  │                                 │
│   │  [🔄 Rafraîchir]                    │  │  [⬇️ Télécharger & Installer]   │
│   │                                     │  │                                 │
│   └─────────────────────────────────────┘  └─────────────────────────────────┘
│                                                                              │
│   [📁 Installer depuis fichier local]                                        │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│   Installation: ████████████████░░░░░░░░░░░░░░  45%    185 / 413 KB          │
│   Méthode: esptool (rapide)                                       [Annuler]  │
└──────────────────────────────────────────────────────────────────────────────┘
```

### GitHub API Integration
```kotlin
// Fetch releases from GitHub
interface GitHubApi {
    @GET("repos/{owner}/{repo}/releases")
    suspend fun getReleases(
        @Path("owner") owner: String,
        @Path("repo") repo: String
    ): List<Release>
}

data class Release(
    val tag_name: String,      // "v1.8.0"
    val name: String,          // "Version 1.8.0"
    val body: String,          // Release notes (markdown)
    val published_at: String,
    val assets: List<Asset>
)

data class Asset(
    val name: String,          // "firmware.bin"
    val browser_download_url: String,
    val size: Int
)
```

### Commandes USB utilisées
- `SYS INFO` - Version actuelle
- `SYS BOOTLOADER` - Mode esptool (recommandé)
- `OTA START/DATA/END` - Fallback base64

### Workflow Update
```
1. User sélectionne version
       ↓
2. Télécharge .bin depuis GitHub (ou local)
       ↓
3. Calcule MD5 du fichier
       ↓
4. Essaie méthode esptool:
   a. Envoie "SYS BOOTLOADER"
   b. Attend reconnexion USB
   c. Flash via protocole esptool
       ↓
   Si échec esptool → Fallback OTA (protocole v2):
   a. Pre-flight: "OTA ABORT" + drain 1s
   b. "OTA START <size> <md5>"
   c. Pour chaque chunk (180 bytes):
      - Calcule CRC32 du chunk binaire
      - Envoie "OTA DATA <base64> <crc32>"
      - Attend OK: si CRC mismatch → retry chunk (3x max)
      - Si timeout → OTA ABORT + restart complet
   d. "OTA END"
       ↓
5. Device redémarre
       ↓
6. Vérifie nouvelle version avec "SYS INFO"
```

> **Implémentation OTA:** voir `docs/protocols/OTA_PROTOCOL.md` pour le protocole v2 complet
> et l'exemple Kotlin (`OtaUpdater` class) à utiliser dans `UpdateViewModel.kt`.

### Fonctionnalités
- [ ] Fetch releases depuis GitHub API
- [ ] Afficher release notes
- [ ] Télécharger firmware
- [ ] Flash via esptool (rapide)
- [ ] Fallback OTA serial (lent mais fiable)
- [ ] Progress bar
- [ ] Sélection fichier local
- [ ] Vérification MD5

---

## Écran 5: Debug (Logs CAN)

**But:** Visualiser et enregistrer les trames CAN

### UI Components (Paysage 1280x720)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   [▶ Start] [⏸ Pause] [🗑 Clear]    💾 Log: OFF     Stats: 1,234 | 45/sec   │
│                                                                              │
│   ┌────────────────────────────────────────────────────┐  ┌─────────────────┐│
│   │  Timestamp     ID      DLC   Data                  │  │  Filtres        ││
│   │  ─────────────────────────────────────────────────│  │                 ││
│   │  12:34:56.123  0x002   [8]  00 0B 60 00 00 00 00  │  │  [x] 0x002 Dir  ││
│   │  12:34:56.156  0x180   [8]  45 E0 00 00 00 00 00  │  │  [x] 0x180 RPM  ││
│   │  12:34:56.189  0x284   [8]  00 3C 00 00 00 00 00  │  │  [x] 0x284 Spd  ││
│   │  12:34:56.223  0x5C5   [8]  1E 00 C3 50 00 00 00  │  │  [x] 0x5C5 Fuel ││
│   │  12:34:56.256  0x002   [8]  00 0B 61 00 00 00 00  │  │  [ ] 0x60D Door ││
│   │  12:34:56.289  0x6F6   [8]  8E 00 00 00 00 00 00  │  │  [x] 0x6F6 Volt ││
│   │  12:34:56.323  0x180   [8]  45 E1 00 00 00 00 00  │  │                 ││
│   │  12:34:56.356  0x551   [8]  7D 00 00 00 00 00 00  │  │  [+ Ajouter ID] ││
│   │  ...                                              │  │                 ││
│   │                                                   │  │  ───────────────││
│   │                                                   │  │  Décodage:      ││
│   │                                                   │  │  [ ] Afficher   ││
│   │                                                   │  │      valeurs    ││
│   └────────────────────────────────────────────────────┘  └─────────────────┘│
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Note:** Tab "CAN Poste" (Toyota RAV4 TX) sera ajouté plus tard côté ESP32

### Commandes USB utilisées
- `LOG ON` - Active le logging CAN
- `LOG OFF` - Désactive le logging
- Parse les lignes `RX 0xXXX [N]: XX XX XX...`

### Data Model
```kotlin
data class CanFrame(
    val timestamp: Long,
    val direction: Direction,  // RX or TX
    val canId: Int,
    val data: ByteArray,
    val source: Source  // VEHICLE or HEADUNIT
)

enum class Direction { RX, TX }
enum class Source { VEHICLE, HEADUNIT }
```

### Fonctionnalités
- [ ] Affichage temps réel des trames
- [ ] Filtrage par CAN ID
- [ ] Pause/Resume
- [ ] Clear buffer
- [ ] Export vers fichier (CSV ou raw)
- [ ] Fichiers séparés: `vehicle_can_YYYYMMDD_HHMMSS.log`
- [ ] Stats: frames/sec, total frames
- [ ] Décodage intelligent (afficher "Steering: -150" au lieu de raw bytes)

### Logging vers fichier
```kotlin
// Fichier vehicle CAN
/storage/emulated/0/CANBox/logs/vehicle_can_20260126_143052.log

// Format:
// timestamp,direction,can_id,dlc,data
1706277052123,RX,0x002,8,00 0B 60 00 00 00 00 00
1706277052156,RX,0x180,8,45 E0 00 00 00 00 00 00
```

---

## Services & Architecture

### USB Serial Service
```kotlin
class UsbSerialService : Service() {
    private var connection: UsbSerialPort?

    fun connect(): Boolean
    fun disconnect()
    fun sendCommand(cmd: String): String
    fun startStreaming(callback: (String) -> Unit)
    fun stopStreaming()
}
```

### Repository Pattern
```kotlin
interface CanBoxRepository {
    // Live data
    suspend fun getVehicleData(): VehicleData
    fun observeVehicleData(): Flow<VehicleData>

    // Config
    suspend fun getCalibration(): CalibrationConfig
    suspend fun setCalibration(param: String, value: Int)
    suspend fun saveCalibration()

    // CAN configs
    suspend fun listCanConfigs(): List<CanConfigFile>
    suspend fun loadCanConfig(filename: String)
    suspend fun uploadCanConfig(filename: String, content: ByteArray)

    // Firmware
    suspend fun getFirmwareInfo(): FirmwareInfo
    suspend fun flashFirmware(firmware: ByteArray, onProgress: (Int) -> Unit)

    // Debug
    fun observeCanFrames(): Flow<CanFrame>
    suspend fun setCanLogging(enabled: Boolean)
}
```

---

## Structure du Projet

```
app/
├── src/main/java/com/example/canbox/
│   ├── MainActivity.kt
│   ├── ui/
│   │   ├── theme/
│   │   ├── navigation/
│   │   │   └── BottomNavigation.kt
│   │   ├── screens/
│   │   │   ├── live/
│   │   │   │   ├── LiveScreen.kt
│   │   │   │   └── LiveViewModel.kt
│   │   │   ├── canconfig/
│   │   │   │   ├── CanConfigScreen.kt
│   │   │   │   └── CanConfigViewModel.kt
│   │   │   ├── calibration/
│   │   │   │   ├── CalibrationScreen.kt
│   │   │   │   └── CalibrationViewModel.kt
│   │   │   ├── update/
│   │   │   │   ├── UpdateScreen.kt
│   │   │   │   └── UpdateViewModel.kt
│   │   │   └── debug/
│   │   │       ├── DebugScreen.kt
│   │   │       └── DebugViewModel.kt
│   │   └── components/
│   │       ├── GaugeWidget.kt
│   │       ├── SteeringIndicator.kt
│   │       ├── DoorStatusView.kt
│   │       └── CanFrameRow.kt
│   ├── data/
│   │   ├── repository/
│   │   │   └── CanBoxRepository.kt
│   │   ├── usb/
│   │   │   ├── UsbSerialService.kt
│   │   │   └── CommandParser.kt
│   │   ├── github/
│   │   │   └── GitHubApi.kt
│   │   └── local/
│   │       └── LogDatabase.kt
│   ├── domain/
│   │   └── model/
│   │       ├── VehicleData.kt
│   │       ├── CalibrationConfig.kt
│   │       ├── CanFrame.kt
│   │       └── FirmwareInfo.kt
│   └── di/
│       └── AppModule.kt (Hilt/Koin)
├── src/main/res/
└── build.gradle.kts
```

---

## Dépendances

```kotlin
// build.gradle.kts
dependencies {
    // Core
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")

    // Compose
    implementation(platform("androidx.compose:compose-bom:2024.01.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.navigation:navigation-compose:2.7.6")

    // USB Serial
    implementation("com.github.mik3y:usb-serial-for-android:3.7.0")

    // Networking (GitHub API)
    implementation("com.squareup.retrofit2:retrofit:2.9.0")
    implementation("com.squareup.retrofit2:converter-gson:2.9.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // Local DB (logs)
    implementation("androidx.room:room-runtime:2.6.1")
    implementation("androidx.room:room-ktx:2.6.1")
    kapt("androidx.room:room-compiler:2.6.1")

    // DI
    implementation("io.insert-koin:koin-android:3.5.0")
    implementation("io.insert-koin:koin-androidx-compose:3.5.0")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
}
```

---

## Ordre d'Implémentation

### Phase 1: Setup & Connexion USB
- [ ] Créer projet Android Studio
- [ ] Setup Jetpack Compose + Navigation
- [ ] Intégrer usb-serial-for-android
- [ ] Tester connexion basique (envoyer HELP, recevoir réponse)
- [ ] Service USB avec reconnexion automatique

### Phase 2: Écran Live Stats
- [ ] UI avec gauges
- [ ] Parsing SYS DATA
- [ ] Polling automatique
- [ ] Affichage temps réel

### Phase 3: Écran Calibration
- [ ] UI avec sliders
- [ ] CFG GET/SET/SAVE
- [ ] Validation ranges

### Phase 4: Écran CAN Config
- [ ] Liste fichiers (CAN LIST)
- [ ] Charger config (CAN LOAD)
- [ ] Upload config (CAN UPLOAD)
- [ ] Import fichier local

### Phase 5: Écran Update
- [ ] GitHub API integration
- [ ] Téléchargement firmware
- [ ] SYS BOOTLOADER + esptool
- [ ] Fallback OTA serial
- [ ] Progress UI

### Phase 6: Écran Debug
- [ ] LOG ON/OFF
- [ ] Parsing frames CAN
- [ ] Filtres
- [ ] Export fichier

### Phase 7: Polish
- [ ] Gestion erreurs
- [ ] Tests
- [ ] UI polish
- [ ] Release APK

---

## Décisions Techniques

| Sujet | Décision |
|-------|----------|
| **Logs CAN poste (TX)** | Pas prioritaire, à implémenter plus tard côté ESP32 |
| **esptool Android** | Utiliser une lib existante (ex: esptool-android) |
| **Sources firmware** | Local (fichier .bin) OU distant (GitHub releases) |
| **Sources CAN config** | Local (fichier .json) OU distant (GitHub) |
