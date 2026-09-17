// Only file in SignalCore that may include an Unreal header. The CMake build in packages/signal-core excludes it.
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, SignalCore)
