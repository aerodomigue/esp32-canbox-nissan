// Stubs for SerialCommand external dependencies
#include "Arduino.h"
#include "Update.h"
#include "CanConfigProcessor.h"
#include "ConfigManager.h"

// Update singleton
UpdateClass Update;

// canProcessor — SerialCommand references extern CanConfigProcessor canProcessor
CanConfigProcessor canProcessor;
