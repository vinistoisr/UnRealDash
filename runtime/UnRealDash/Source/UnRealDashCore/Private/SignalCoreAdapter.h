#pragma once
#include "UnRealDashCore/SignalCoreAdapter.h"
#include "SignalCore/Sample.h"
namespace UnRealDashCore {
FSignalSample ToEngineSample(const signal_core::Sample& Sample);
}
