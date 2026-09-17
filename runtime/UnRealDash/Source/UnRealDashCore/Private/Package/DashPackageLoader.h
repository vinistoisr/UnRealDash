#pragma once
#include "UnRealDashCore/DashPackageLoader.h"
#include "dashboard_spec/PackageReader.h"
namespace UnRealDashCore
{
FString ResolveSchemaDirectory();
void TranslateError(const dashboard_spec::Error& Error, const FString& Path, FDashLoadError& Out);
}
