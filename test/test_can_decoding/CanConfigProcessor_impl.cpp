// Include CanConfigProcessor implementation into the native test build.
// PlatformIO native test mode does not automatically compile src/ files,
// so we pull the implementation in explicitly here.
#include "../../src/CanConfigProcessor.cpp"
