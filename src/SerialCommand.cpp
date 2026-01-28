/**
 * @file SerialCommand.cpp
 * @brief Serial command parser implementation
 *
 * Handles USB serial commands for device configuration and monitoring.
 * Supports calibration parameters (NVS), CAN config files (LittleFS),
 * and OTA firmware updates.
 */

#include "SerialCommand.h"
#include "ConfigManager.h"
#include "GlobalData.h"
#include "CanConfigProcessor.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <libb64/cdecode.h>
#include <Update.h>
#include <MD5Builder.h>
#include "soc/rtc_cntl_reg.h"  // For bootloader mode

// External reference to CAN processor (defined in main.cpp)
extern CanConfigProcessor canProcessor;

// =============================================================================
// PRIVATE VARIABLES
// =============================================================================

static char cmdBuffer[CMD_BUFFER_SIZE];
static uint16_t cmdIndex = 0;
static bool canLogEnabled = false;
static unsigned long bootTime = 0;

// =============================================================================
// CAN UPLOAD STATE
// =============================================================================

static bool uploadInProgress = false;
static char uploadFilename[32];
static uint32_t uploadExpectedSize = 0;
static uint32_t uploadReceivedSize = 0;
static uint8_t* uploadBuffer = nullptr;

// =============================================================================
// OTA UPDATE STATE
// =============================================================================

static bool otaInProgress = false;
static uint32_t otaExpectedSize = 0;
static uint32_t otaReceivedSize = 0;
static char otaExpectedMD5[33] = {0};  // 32 hex chars + null
static MD5Builder otaMD5;

// =============================================================================
// PARAMETER DEFINITIONS
// =============================================================================

// Parameter names for CFG commands (must match ConfigManager)
typedef struct {
    const char* name;
    const char* description;
} ParamDef;

static const ParamDef params[] = {
    {"steerOffset",  "Steering center offset (-500 to +500)"},
    {"steerInvert",  "Invert steering (0/1)"},
    {"steerScale",   "Steering scale (1-200, x0.01)"},
    {"indTimeout",   "Indicator timeout ms (100-2000)"},
    {"rpmDiv",       "RPM divisor (1-20)"},
    {"tankCap",      "Tank capacity liters (20-100)"},
    {"dteDiv",       "DTE divisor x100 (100-500)"},
};
static const uint8_t PARAM_COUNT = sizeof(params) / sizeof(params[0]);

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void processCommand(const char* cmd);
static void handleCfgCommand(const char* args);
static void handleCanCommand(const char* args);
static void handleOtaCommand(const char* args);
static void handleLogCommand(const char* args);
static void handleSysCommand(const char* args);
static void handleHelpCommand();

static void cfgGet(const char* param);
static void cfgSet(const char* param, const char* value);
static void cfgList();

static void canStatus();
static void canList();
static void canLoad(const char* filename);
static void canGet();
static void canDelete(const char* filename);
static void canUploadStart(const char* filename, uint32_t size);
static void canUploadData(const char* base64Data);
static void canUploadEnd();
static void canUploadAbort();

static void otaStart(uint32_t size, const char* md5);
static void otaData(const char* base64Data);
static void otaEnd();
static void otaAbort();
static void otaStatus();

static size_t base64Decode(const char* input, uint8_t* output, size_t maxLen);
static void printOK();
static void printError(const char* msg);

// =============================================================================
// PUBLIC API
// =============================================================================

void serialCommandInit() {
    cmdIndex = 0;
    cmdBuffer[0] = '\0';
    bootTime = millis();
    Serial.println("Serial command interface ready. Type HELP for commands.");
}

void serialCommandProcess() {
    while (Serial.available()) {
        char c = Serial.read();

        // Handle backspace
        if (c == '\b' || c == 127) {
            if (cmdIndex > 0) {
                cmdIndex--;
                Serial.print("\b \b"); // Erase character on terminal
            }
            continue;
        }

        // Handle newline (command complete)
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                Serial.println(); // Echo newline
                processCommand(cmdBuffer);
                cmdIndex = 0;
            }
            continue;
        }

        // Add character to buffer (with overflow protection)
        if (cmdIndex < CMD_BUFFER_SIZE - 1) {
            cmdBuffer[cmdIndex++] = c;
            Serial.print(c); // Echo character
        }
    }
}

bool isCanLogEnabled() {
    return canLogEnabled;
}

bool isOtaInProgress() {
    return otaInProgress;
}

// =============================================================================
// COMMAND PARSER
// =============================================================================

static void processCommand(const char* cmd) {
    // Skip leading whitespace
    while (*cmd == ' ') cmd++;

    // Empty command
    if (*cmd == '\0') return;

    // Convert to uppercase for comparison (first word only)
    char cmdUpper[16];
    uint8_t i = 0;
    while (cmd[i] && cmd[i] != ' ' && i < 15) {
        cmdUpper[i] = toupper(cmd[i]);
        i++;
    }
    cmdUpper[i] = '\0';

    // Find arguments (skip command word and spaces)
    const char* args = cmd + i;
    while (*args == ' ') args++;

    // Route to handler
    if (strcmp(cmdUpper, "CFG") == 0) {
        handleCfgCommand(args);
    }
    else if (strcmp(cmdUpper, "CAN") == 0) {
        handleCanCommand(args);
    }
    else if (strcmp(cmdUpper, "OTA") == 0) {
        handleOtaCommand(args);
    }
    else if (strcmp(cmdUpper, "LOG") == 0) {
        handleLogCommand(args);
    }
    else if (strcmp(cmdUpper, "SYS") == 0) {
        handleSysCommand(args);
    }
    else if (strcmp(cmdUpper, "HELP") == 0 || strcmp(cmdUpper, "?") == 0) {
        handleHelpCommand();
    }
    else {
        printError("Unknown command. Type HELP for list.");
    }
}

// =============================================================================
// CFG COMMAND HANDLER
// =============================================================================

static void handleCfgCommand(const char* args) {
    char subCmd[8];
    char param[20];
    char value[16];

    // Parse subcommand
    int n = sscanf(args, "%7s %19s %15s", subCmd, param, value);

    // Convert subcommand to uppercase
    for (int i = 0; subCmd[i]; i++) subCmd[i] = toupper(subCmd[i]);

    if (n >= 1 && strcmp(subCmd, "LIST") == 0) {
        cfgList();
    }
    else if (n >= 1 && strcmp(subCmd, "SAVE") == 0) {
        configSave();
        printOK();
        Serial.println("Configuration saved to NVS");
    }
    else if (n >= 1 && strcmp(subCmd, "RESET") == 0) {
        configReset();
        printOK();
        Serial.println("Configuration reset to defaults");
    }
    else if (n >= 2 && strcmp(subCmd, "GET") == 0) {
        cfgGet(param);
    }
    else if (n >= 3 && strcmp(subCmd, "SET") == 0) {
        cfgSet(param, value);
    }
    else {
        printError("Usage: CFG <GET|SET|LIST|SAVE|RESET> [param] [value]");
    }
}

static void cfgGet(const char* param) {
    // Convert param to lowercase for matching
    char paramLower[20];
    for (int i = 0; param[i] && i < 19; i++) {
        paramLower[i] = tolower(param[i]);
        paramLower[i+1] = '\0';
    }

    if (strcmp(paramLower, "steeroffset") == 0) {
        Serial.printf("steerOffset = %d\n", configGetSteerOffset());
    }
    else if (strcmp(paramLower, "steerinvert") == 0) {
        Serial.printf("steerInvert = %d\n", configGetSteerInvert() ? 1 : 0);
    }
    else if (strcmp(paramLower, "steerscale") == 0) {
        Serial.printf("steerScale = %d\n", configGetSteerScale());
    }
    else if (strcmp(paramLower, "indtimeout") == 0) {
        Serial.printf("indTimeout = %d\n", configGetIndicatorTimeout());
    }
    else if (strcmp(paramLower, "rpmdiv") == 0) {
        Serial.printf("rpmDiv = %d\n", configGetRpmDivisor());
    }
    else if (strcmp(paramLower, "tankcap") == 0) {
        Serial.printf("tankCap = %d\n", configGetTankCapacity());
    }
    else if (strcmp(paramLower, "dtediv") == 0) {
        Serial.printf("dteDiv = %d\n", configGetDteDivisor());
    }
    else {
        printError("Unknown parameter");
        Serial.println("Valid: steerOffset, steerInvert, steerScale, indTimeout, rpmDiv, tankCap, dteDiv");
    }
}

static void cfgSet(const char* param, const char* value) {
    int32_t val = atoi(value);

    // Convert param to lowercase for matching
    char paramLower[20];
    for (int i = 0; param[i] && i < 19; i++) {
        paramLower[i] = tolower(param[i]);
        paramLower[i+1] = '\0';
    }

    bool valid = true;

    if (strcmp(paramLower, "steeroffset") == 0) {
        if (val < -500 || val > 500) {
            printError("Value must be -500 to +500");
            return;
        }
        configSetSteerOffset((int16_t)val);
    }
    else if (strcmp(paramLower, "steerinvert") == 0) {
        configSetSteerInvert(val != 0);
    }
    else if (strcmp(paramLower, "steerscale") == 0) {
        if (val < 1 || val > 200) {
            printError("Value must be 1 to 200");
            return;
        }
        configSetSteerScale((uint8_t)val);
    }
    else if (strcmp(paramLower, "indtimeout") == 0) {
        if (val < 100 || val > 2000) {
            printError("Value must be 100 to 2000");
            return;
        }
        configSetIndicatorTimeout((uint16_t)val);
    }
    else if (strcmp(paramLower, "rpmdiv") == 0) {
        if (val < 1 || val > 20) {
            printError("Value must be 1 to 20");
            return;
        }
        configSetRpmDivisor((uint8_t)val);
    }
    else if (strcmp(paramLower, "tankcap") == 0) {
        if (val < 20 || val > 100) {
            printError("Value must be 20 to 100");
            return;
        }
        configSetTankCapacity((uint8_t)val);
    }
    else if (strcmp(paramLower, "dtediv") == 0) {
        if (val < 100 || val > 500) {
            printError("Value must be 100 to 500");
            return;
        }
        configSetDteDivisor((uint16_t)val);
    }
    else {
        printError("Unknown parameter");
        return;
    }

    printOK();
    Serial.println("Value set (use CFG SAVE to persist)");
}

static void cfgList() {
    Serial.println("=== Current Configuration ===");
    Serial.printf("steerOffset  = %d    (center offset)\n", configGetSteerOffset());
    Serial.printf("steerInvert  = %d    (invert direction)\n", configGetSteerInvert() ? 1 : 0);
    Serial.printf("steerScale   = %d    (scale x0.01)\n", configGetSteerScale());
    Serial.printf("indTimeout   = %d    (indicator ms)\n", configGetIndicatorTimeout());
    Serial.printf("rpmDiv       = %d    (RPM divisor)\n", configGetRpmDivisor());
    Serial.printf("tankCap      = %d    (tank liters)\n", configGetTankCapacity());
    Serial.printf("dteDiv       = %d    (DTE divisor x100)\n", configGetDteDivisor());
    Serial.println("=============================");
}

// =============================================================================
// CAN COMMAND HANDLER
// =============================================================================

/**
 * @brief Handle CAN configuration commands
 *
 * Subcommands:
 * - STATUS: Show current CAN mode and profile
 * - LIST: List available config files
 * - LOAD <file>: Load a config file
 * - GET: Output current config as JSON
 * - DELETE <file>: Delete a config file
 * - UPLOAD START <file> <size>: Begin upload
 * - UPLOAD DATA <base64>: Send data chunk
 * - UPLOAD END: Finalize upload
 * - UPLOAD ABORT: Cancel upload
 * - RELOAD: Reload current config
 */
static void handleCanCommand(const char* args) {
    char subCmd[16];
    char param1[32];
    char param2[16];

    // Parse subcommand and parameters
    int n = sscanf(args, "%15s %31s %15s", subCmd, param1, param2);

    if (n < 1) {
        printError("Usage: CAN <STATUS|LIST|LOAD|GET|DELETE|UPLOAD|RELOAD>");
        return;
    }

    // Convert subcommand to uppercase
    for (int i = 0; subCmd[i]; i++) subCmd[i] = toupper(subCmd[i]);

    if (strcmp(subCmd, "STATUS") == 0) {
        canStatus();
    }
    else if (strcmp(subCmd, "LIST") == 0) {
        canList();
    }
    else if (strcmp(subCmd, "LOAD") == 0 && n >= 2) {
        canLoad(param1);
    }
    else if (strcmp(subCmd, "GET") == 0) {
        canGet();
    }
    else if (strcmp(subCmd, "DELETE") == 0 && n >= 2) {
        canDelete(param1);
    }
    else if (strcmp(subCmd, "UPLOAD") == 0 && n >= 2) {
        // Handle UPLOAD subcommands
        for (int i = 0; param1[i]; i++) param1[i] = toupper(param1[i]);

        if (strcmp(param1, "START") == 0) {
            // Parse: UPLOAD START <filename> <size>
            char filename[32];
            uint32_t size;
            if (sscanf(args + 13, "%31s %u", filename, &size) == 2) {
                canUploadStart(filename, size);
            } else {
                printError("Usage: CAN UPLOAD START <filename> <size>");
            }
        }
        else if (strcmp(param1, "DATA") == 0) {
            // Find the base64 data after "UPLOAD DATA "
            const char* dataStart = strstr(args, "DATA ");
            if (dataStart) {
                dataStart += 5;  // Skip "DATA "
                while (*dataStart == ' ') dataStart++;
                canUploadData(dataStart);
            } else {
                printError("Usage: CAN UPLOAD DATA <base64_data>");
            }
        }
        else if (strcmp(param1, "END") == 0) {
            canUploadEnd();
        }
        else if (strcmp(param1, "ABORT") == 0) {
            canUploadAbort();
        }
        else {
            printError("Usage: CAN UPLOAD <START|DATA|END|ABORT>");
        }
    }
    else if (strcmp(subCmd, "RELOAD") == 0) {
        Serial.println("Reloading CAN configuration...");
        if (canProcessor.begin()) {
            // Reset vehicle data to clear stale values
            resetVehicleData();

            printOK();
            Serial.printf("Loaded: %s (%s mode)\n",
                          canProcessor.getProfileName(),
                          canProcessor.isMockMode() ? "MOCK" : "REAL");
        } else {
            Serial.println("No config found - MOCK mode active");
        }
    }
    else {
        printError("Usage: CAN <STATUS|LIST|LOAD|GET|DELETE|UPLOAD|RELOAD>");
    }
}

/**
 * @brief Display current CAN configuration status
 */
static void canStatus() {
    Serial.println("=== CAN Configuration Status ===");
    Serial.printf("Config: %s\n", configGetVehicleFile());
    Serial.printf("Mode: %s\n", canProcessor.isMockMode() ? "MOCK (simulated data)" : "REAL (CAN bus)");
    Serial.printf("Profile: %s\n", canProcessor.getProfileName());
    Serial.printf("Frames processed: %lu\n", canProcessor.getFramesProcessed());
    Serial.printf("Unknown frames: %lu\n", canProcessor.getUnknownFrames());
    if (uploadInProgress) {
        Serial.printf("Upload in progress: %s (%lu/%lu bytes)\n",
                      uploadFilename, uploadReceivedSize, uploadExpectedSize);
    }
    Serial.println("================================");
}

/**
 * @brief List all JSON config files on LittleFS
 */
static void canList() {
    Serial.println("=== CAN Config Files ===");

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        printError("Failed to open filesystem");
        return;
    }

    int count = 0;
    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        if (name.endsWith(".json")) {
            Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
            count++;
        }
        file = root.openNextFile();
    }

    if (count == 0) {
        Serial.println("  (no config files found)");
    }
    Serial.printf("Total: %d file(s)\n", count);
    Serial.println("========================");
}

/**
 * @brief Load a specific config file
 */
static void canLoad(const char* filename) {
    // Ensure filename starts with /
    char path[40];
    if (filename[0] != '/') {
        snprintf(path, sizeof(path), "/%s", filename);
    } else {
        strncpy(path, filename, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    // Check file exists
    if (!LittleFS.exists(path)) {
        printError("File not found");
        return;
    }

    // Try to load the config
    if (canProcessor.loadFromJson(path)) {
        // Reset vehicle data to clear stale values from previous config
        resetVehicleData();

        printOK();
        Serial.printf("Loaded: %s\n", canProcessor.getProfileName());
        Serial.printf("Mode: %s\n", canProcessor.isMockMode() ? "MOCK" : "REAL");
    } else {
        printError("Failed to parse config file");
    }
}

/**
 * @brief Output current config file content
 */
static void canGet() {
    // Get the currently loaded config file
    const char* currentFile = configGetVehicleFile();
    if (currentFile[0] == '\0') {
        printError("No config file loaded");
        return;
    }

    // Build path with leading /
    char path[48];
    snprintf(path, sizeof(path), "/%s", currentFile);

    if (!LittleFS.exists(path)) {
        printError("Config file not found on filesystem");
        return;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        printError("Failed to open config file");
        return;
    }

    Serial.printf("=== %s ===\n", currentFile);
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();
    Serial.println("=== END ===");
    file.close();
}

/**
 * @brief Delete a config file
 */
static void canDelete(const char* filename) {
    char path[40];
    if (filename[0] != '/') {
        snprintf(path, sizeof(path), "/%s", filename);
    } else {
        strncpy(path, filename, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    if (!LittleFS.exists(path)) {
        printError("File not found");
        return;
    }

    if (LittleFS.remove(path)) {
        printOK();
        Serial.printf("Deleted: %s\n", path);
    } else {
        printError("Failed to delete file");
    }
}

/**
 * @brief Start a new config file upload
 */
static void canUploadStart(const char* filename, uint32_t size) {
    // Cancel any previous upload
    if (uploadBuffer) {
        free(uploadBuffer);
        uploadBuffer = nullptr;
    }

    // Validate size
    if (size == 0 || size > CAN_UPLOAD_MAX_SIZE) {
        printError("Invalid size (max 8KB)");
        return;
    }

    // Allocate buffer
    uploadBuffer = (uint8_t*)malloc(size + 1);
    if (!uploadBuffer) {
        printError("Out of memory");
        return;
    }

    // Store upload state
    if (filename[0] != '/') {
        snprintf(uploadFilename, sizeof(uploadFilename), "/%s", filename);
    } else {
        strncpy(uploadFilename, filename, sizeof(uploadFilename) - 1);
        uploadFilename[sizeof(uploadFilename) - 1] = '\0';
    }

    uploadExpectedSize = size;
    uploadReceivedSize = 0;
    uploadInProgress = true;

    Serial.println("OK READY");
    Serial.printf("Awaiting %lu bytes for %s\n", size, uploadFilename);
}

/**
 * @brief Receive a chunk of base64-encoded data
 */
static void canUploadData(const char* base64Data) {
    if (!uploadInProgress || !uploadBuffer) {
        printError("No upload in progress");
        return;
    }

    // Decode base64 data directly into buffer
    size_t remaining = uploadExpectedSize - uploadReceivedSize;
    size_t decoded = base64Decode(base64Data, uploadBuffer + uploadReceivedSize, remaining);

    if (decoded == 0) {
        printError("Base64 decode failed");
        return;
    }

    uploadReceivedSize += decoded;

    Serial.printf("OK %lu/%lu\n", uploadReceivedSize, uploadExpectedSize);
    Serial.flush();  // Ensure ACK is sent before processing next chunk
    delay(1);        // Give USB CDC stack time to transmit
}

/**
 * @brief Finalize upload and validate JSON
 */
static void canUploadEnd() {
    if (!uploadInProgress || !uploadBuffer) {
        printError("No upload in progress");
        return;
    }

    // Null-terminate the buffer
    uploadBuffer[uploadReceivedSize] = '\0';

    // Validate JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)uploadBuffer);

    if (error) {
        Serial.printf("ERROR: JSON parse failed: %s\n", error.c_str());
        canUploadAbort();
        return;
    }

    // Check required fields
    if (!doc["name"].is<const char*>() || !doc["frames"].is<JsonArray>()) {
        printError("Invalid config: missing 'name' or 'frames'");
        canUploadAbort();
        return;
    }

    // Write to file
    File file = LittleFS.open(uploadFilename, "w");
    if (!file) {
        printError("Failed to create file");
        canUploadAbort();
        return;
    }

    file.write(uploadBuffer, uploadReceivedSize);
    file.close();

    // Cleanup
    free(uploadBuffer);
    uploadBuffer = nullptr;
    uploadInProgress = false;

    printOK();
    Serial.printf("Saved: %s (%lu bytes)\n", uploadFilename, uploadReceivedSize);
    Serial.println("Use 'CAN RELOAD' to activate");
}

/**
 * @brief Abort current upload
 */
static void canUploadAbort() {
    if (uploadBuffer) {
        free(uploadBuffer);
        uploadBuffer = nullptr;
    }
    uploadInProgress = false;
    uploadReceivedSize = 0;
    uploadExpectedSize = 0;
    Serial.println("Upload aborted");
}

/**
 * @brief Decode base64 string to binary
 * @return Number of bytes decoded
 */
static size_t base64Decode(const char* input, uint8_t* output, size_t maxLen) {
    base64_decodestate state;
    base64_init_decodestate(&state);

    size_t inputLen = strlen(input);
    int decoded = base64_decode_block(input, inputLen, (char*)output, &state);

    return (decoded > 0 && (size_t)decoded <= maxLen) ? decoded : 0;
}

// =============================================================================
// OTA COMMAND HANDLER
// =============================================================================

/**
 * @brief Handle OTA firmware update commands
 *
 * Subcommands:
 * - START <size> [md5]: Begin firmware update
 * - DATA <base64>: Send firmware chunk
 * - END: Finalize update and reboot
 * - ABORT: Cancel update
 * - STATUS: Show update status
 *
 * Protocol:
 * 1. OTA START <size_bytes> [md5_hash]
 * 2. OTA DATA <base64_chunk> (repeat)
 * 3. OTA END (validates and reboots)
 */
static void handleOtaCommand(const char* args) {
    char subCmd[16];
    char param1[40];
    char param2[40];

    int n = sscanf(args, "%15s %39s %39s", subCmd, param1, param2);

    if (n < 1) {
        printError("Usage: OTA <START|DATA|END|ABORT|STATUS>");
        return;
    }

    // Convert subcommand to uppercase
    for (int i = 0; subCmd[i]; i++) subCmd[i] = toupper(subCmd[i]);

    if (strcmp(subCmd, "START") == 0 && n >= 2) {
        uint32_t size = strtoul(param1, nullptr, 10);
        const char* md5 = (n >= 3) ? param2 : nullptr;
        otaStart(size, md5);
    }
    else if (strcmp(subCmd, "DATA") == 0) {
        // Find the base64 data after "DATA "
        const char* dataStart = strstr(args, "DATA ");
        if (!dataStart) dataStart = strstr(args, "data ");
        if (dataStart) {
            dataStart += 5;  // Skip "DATA "
            while (*dataStart == ' ') dataStart++;
            otaData(dataStart);
        } else {
            printError("Usage: OTA DATA <base64_data>");
        }
    }
    else if (strcmp(subCmd, "END") == 0) {
        otaEnd();
    }
    else if (strcmp(subCmd, "ABORT") == 0) {
        otaAbort();
    }
    else if (strcmp(subCmd, "STATUS") == 0) {
        otaStatus();
    }
    else {
        printError("Usage: OTA <START|DATA|END|ABORT|STATUS>");
    }
}

/**
 * @brief Start OTA firmware update
 * @param size Expected firmware size in bytes
 * @param md5 Optional MD5 hash for verification (32 hex chars)
 */
static void otaStart(uint32_t size, const char* md5) {
    // Check if another update is in progress
    if (otaInProgress) {
        printError("OTA already in progress. Use OTA ABORT first.");
        return;
    }

    // Validate size
    if (size == 0) {
        printError("Invalid firmware size");
        return;
    }

    // Check available space
    if (size > (ESP.getFreeSketchSpace() - 0x1000)) {
        Serial.printf("ERROR: Firmware too large (%lu bytes, max %lu)\n",
                      size, ESP.getFreeSketchSpace() - 0x1000);
        return;
    }

    // Store expected MD5 if provided
    if (md5 && strlen(md5) == 32) {
        strncpy(otaExpectedMD5, md5, 32);
        otaExpectedMD5[32] = '\0';
        // Convert to lowercase for comparison
        for (int i = 0; i < 32; i++) {
            otaExpectedMD5[i] = tolower(otaExpectedMD5[i]);
        }
    } else {
        otaExpectedMD5[0] = '\0';  // No MD5 verification
    }

    // Begin update
    if (!Update.begin(size)) {
        Serial.printf("ERROR: Update.begin failed: %s\n", Update.errorString());
        return;
    }

    // Initialize MD5 calculation
    otaMD5.begin();

    otaExpectedSize = size;
    otaReceivedSize = 0;
    otaInProgress = true;

    Serial.println("OK READY");
    Serial.printf("OTA started: expecting %lu bytes\n", size);
    if (otaExpectedMD5[0]) {
        Serial.printf("MD5 verification: %s\n", otaExpectedMD5);
    }
}

/**
 * @brief Receive a chunk of base64-encoded firmware data
 * @param base64Data Base64 encoded firmware chunk
 */
static void otaData(const char* base64Data) {
    if (!otaInProgress) {
        printError("No OTA in progress. Use OTA START first.");
        return;
    }

    // Decode base64 to binary
    static uint8_t decodeBuffer[256];  // Decode buffer
    size_t decoded = base64Decode(base64Data, decodeBuffer, sizeof(decodeBuffer));

    if (decoded == 0) {
        printError("Base64 decode failed");
        return;
    }

    // Check for overflow
    if (otaReceivedSize + decoded > otaExpectedSize) {
        Serial.printf("ERROR: Data exceeds expected size (%lu + %u > %lu)\n",
                      otaReceivedSize, decoded, otaExpectedSize);
        otaAbort();
        return;
    }

    // Write to flash
    size_t written = Update.write(decodeBuffer, decoded);
    if (written != decoded) {
        Serial.printf("ERROR: Write failed (wrote %u of %u): %s\n",
                      written, decoded, Update.errorString());
        otaAbort();
        return;
    }

    // Update MD5
    otaMD5.add(decodeBuffer, decoded);

    otaReceivedSize += decoded;

    // Progress response
    uint8_t percent = (otaReceivedSize * 100) / otaExpectedSize;
    Serial.printf("OK %lu/%lu (%d%%)\n", otaReceivedSize, otaExpectedSize, percent);
}

/**
 * @brief Finalize OTA update, verify, and reboot
 */
static void otaEnd() {
    if (!otaInProgress) {
        printError("No OTA in progress");
        return;
    }

    // Check if all data received
    if (otaReceivedSize != otaExpectedSize) {
        Serial.printf("ERROR: Incomplete data (%lu of %lu bytes)\n",
                      otaReceivedSize, otaExpectedSize);
        otaAbort();
        return;
    }

    // Verify MD5 if provided
    if (otaExpectedMD5[0]) {
        otaMD5.calculate();
        String calculatedMD5 = otaMD5.toString();

        if (calculatedMD5 != otaExpectedMD5) {
            Serial.printf("ERROR: MD5 mismatch!\n");
            Serial.printf("  Expected: %s\n", otaExpectedMD5);
            Serial.printf("  Got:      %s\n", calculatedMD5.c_str());
            otaAbort();
            return;
        }
        Serial.println("MD5 verified OK");
    }

    // Finalize update
    if (!Update.end(true)) {
        Serial.printf("ERROR: Update.end failed: %s\n", Update.errorString());
        otaAbort();
        return;
    }

    // Success!
    otaInProgress = false;
    printOK();
    Serial.println("Firmware updated successfully!");
    Serial.println("Rebooting in 2 seconds...");
    delay(2000);
    ESP.restart();
}

/**
 * @brief Abort OTA update
 */
static void otaAbort() {
    if (otaInProgress) {
        Update.abort();
    }

    otaInProgress = false;
    otaReceivedSize = 0;
    otaExpectedSize = 0;
    otaExpectedMD5[0] = '\0';

    Serial.println("OTA aborted");
}

/**
 * @brief Show OTA status
 */
static void otaStatus() {
    Serial.println("=== OTA Status ===");
    Serial.printf("Update in progress: %s\n", otaInProgress ? "YES" : "NO");
    if (otaInProgress) {
        uint8_t percent = (otaReceivedSize * 100) / otaExpectedSize;
        Serial.printf("Progress: %lu / %lu bytes (%d%%)\n",
                      otaReceivedSize, otaExpectedSize, percent);
        if (otaExpectedMD5[0]) {
            Serial.printf("Expected MD5: %s\n", otaExpectedMD5);
        }
    }
    Serial.printf("Free sketch space: %lu bytes\n", ESP.getFreeSketchSpace());
    Serial.printf("Current firmware size: %lu bytes\n", ESP.getSketchSize());
    Serial.println("==================");
}

// =============================================================================
// LOG COMMAND HANDLER
// =============================================================================

static void handleLogCommand(const char* args) {
    char subCmd[8];

    if (sscanf(args, "%7s", subCmd) != 1) {
        printError("Usage: LOG <ON|OFF>");
        return;
    }

    for (int i = 0; subCmd[i]; i++) subCmd[i] = toupper(subCmd[i]);

    if (strcmp(subCmd, "ON") == 0) {
        canLogEnabled = true;
        printOK();
        Serial.println("CAN logging enabled");
    }
    else if (strcmp(subCmd, "OFF") == 0) {
        canLogEnabled = false;
        printOK();
        Serial.println("CAN logging disabled");
    }
    else {
        printError("Usage: LOG <ON|OFF>");
    }
}

// =============================================================================
// SYS COMMAND HANDLER
// =============================================================================

static void handleSysCommand(const char* args) {
    char subCmd[8];

    if (sscanf(args, "%7s", subCmd) != 1) {
        printError("Usage: SYS <INFO|REBOOT|DATA>");
        return;
    }

    for (int i = 0; subCmd[i]; i++) subCmd[i] = toupper(subCmd[i]);

    if (strcmp(subCmd, "INFO") == 0) {
        unsigned long uptime = (millis() - bootTime) / 1000;
        Serial.println("=== System Info ===");
        Serial.printf("Firmware: %s (%s)\n", FIRMWARE_VERSION, FIRMWARE_DATE);
        Serial.printf("Uptime: %lu sec\n", uptime);
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz());
        Serial.printf("Chip: %s rev%d\n", ESP.getChipModel(), ESP.getChipRevision());
        Serial.println("===================");
    }
    else if (strcmp(subCmd, "DATA") == 0) {
        Serial.println("=== Live Vehicle Data ===");
        Serial.printf("Config:   %s\n", configGetVehicleFile());
        Serial.printf("Mode:     %s (%s)\n",
            canProcessor.isMockMode() ? "MOCK" : "REAL",
            canProcessor.getProfileName());
        Serial.printf("RPM:      %d\n", engineRPM);
        Serial.printf("Speed:    %d km/h\n", vehicleSpeed);
        Serial.printf("Steering: %d\n", currentSteer);
        Serial.printf("Fuel:     %d L\n", fuelLevel);
        Serial.printf("Battery:  %.1f V\n", voltBat);
        Serial.printf("DTE:      %d km\n", dteValue);
        Serial.printf("Temp:     %d C\n", tempExt);
        Serial.printf("Doors:    0x%02X\n", currentDoors);
        Serial.printf("Lights:   H=%d P=%d HB=%d L=%d R=%d\n",
            headlightsOn, parkingLightsOn, highBeamOn, indicatorLeft, indicatorRight);
        Serial.println("=========================");
    }
    else if (strcmp(subCmd, "REBOOT") == 0) {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    }
    else if (strcmp(subCmd, "BOOTLOADER") == 0) {
        Serial.println("Entering bootloader mode for esptool...");
        Serial.println("Use esptool.py to flash firmware.");
        Serial.flush();
        delay(100);

        // Force download boot mode on next restart
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
    }
    else {
        printError("Usage: SYS <INFO|DATA|REBOOT|BOOTLOADER>");
    }
}

// =============================================================================
// HELP COMMAND
// =============================================================================

static void handleHelpCommand() {
    Serial.println("=== ESP32 CANBox Command Interface ===");
    Serial.println();
    Serial.println("CFG GET <param>       Read calibration value");
    Serial.println("CFG SET <param> <val> Set calibration value");
    Serial.println("CFG LIST              Show all calibration");
    Serial.println("CFG SAVE              Save to flash (NVS)");
    Serial.println("CFG RESET             Reset to defaults");
    Serial.println();
    Serial.println("CAN STATUS            CAN config status");
    Serial.println("CAN LIST              List config files");
    Serial.println("CAN LOAD <file>       Load config file");
    Serial.println("CAN GET               Output current config");
    Serial.println("CAN DELETE <file>     Delete config file");
    Serial.println("CAN UPLOAD START/DATA/END  Upload config");
    Serial.println("CAN RELOAD            Reload configuration");
    Serial.println();
    Serial.println("OTA START <size> [md5]  Start firmware update");
    Serial.println("OTA DATA <base64>       Send firmware chunk");
    Serial.println("OTA END                 Finalize and reboot");
    Serial.println("OTA ABORT               Cancel update");
    Serial.println("OTA STATUS              Update status");
    Serial.println();
    Serial.println("LOG ON|OFF            CAN frame logging");
    Serial.println();
    Serial.println("SYS INFO              System information");
    Serial.println("SYS DATA              Live vehicle data");
    Serial.println("SYS REBOOT            Restart device");
    Serial.println("SYS BOOTLOADER        Enter esptool flash mode");
    Serial.println();
    Serial.println("HELP                  This message");
    Serial.println("======================================");
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static void printOK() {
    Serial.println("OK");
}

static void printError(const char* msg) {
    Serial.print("ERROR: ");
    Serial.println(msg);
}
