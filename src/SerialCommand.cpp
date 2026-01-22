/**
 * @file SerialCommand.cpp
 * @brief Serial command parser implementation
 */

#include "SerialCommand.h"
#include "ConfigManager.h"
#include "GlobalData.h"

// =============================================================================
// PRIVATE VARIABLES
// =============================================================================

static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdIndex = 0;
static bool canLogEnabled = false;
static unsigned long bootTime = 0;

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
static void handleLogCommand(const char* args);
static void handleSysCommand(const char* args);
static void handleHelpCommand();

static void cfgGet(const char* param);
static void cfgSet(const char* param, const char* value);
static void cfgList();

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
    else {
        printError("Usage: SYS <INFO|REBOOT|DATA>");
    }
}

// =============================================================================
// HELP COMMAND
// =============================================================================

static void handleHelpCommand() {
    Serial.println("=== ESP32 CANBox Command Interface ===");
    Serial.println();
    Serial.println("CFG GET <param>       Read config value");
    Serial.println("CFG SET <param> <val> Set config value");
    Serial.println("CFG LIST              Show all config");
    Serial.println("CFG SAVE              Save to flash");
    Serial.println("CFG RESET             Reset to defaults");
    Serial.println();
    Serial.println("LOG ON|OFF            CAN frame logging");
    Serial.println();
    Serial.println("SYS INFO              System information");
    Serial.println("SYS DATA              Live vehicle data");
    Serial.println("SYS REBOOT            Restart device");
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
