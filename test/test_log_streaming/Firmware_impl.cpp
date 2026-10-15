// Execute production setup()/loop() and its production dependencies on the host.
// Persistent storage, USB CDC, TWAI, watchdog and scheduling are simulated.
// The source filter for this suite must be -<*> to avoid compiling these twice.
#include "../../src/main.cpp"
#include "../../src/CanCapture.cpp"
#include "../../src/CanLogFormatter.cpp"
#include "../../src/ObdCalibration.cpp"
#include "../../src/SerialCommand.cpp"
#include "../../src/CanConfigProcessor.cpp"
#include "../../src/GlobalData.cpp"
#include "../../src/MockDataGenerator.cpp"
#include "../../src/RadioSend.cpp"
#include "../../src/UsbConsole.cpp"
