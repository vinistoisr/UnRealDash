#include "DashPackageTestCommandlet.h"
#include "UnRealDashCore/ComponentRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDashPackageTestCommandlet::UDashPackageTestCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}
int32 UDashPackageTestCommandlet::Main(const FString& Params)
{
    using namespace UnRealDashCore;
    FString Root;
    if (!FParse::Value(*Params, TEXT("PackageFixtures="), Root))
    {
        UE_LOG(LogTemp, Error, TEXT("Supply -PackageFixtures=<tests/fixtures/packages>"));
        return 1;
    }
    InitializePackageLoader();
    FString Text;
    TArray<TSharedPtr<FJsonValue>> Cases;
    if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, TEXT("cases.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Cases)) return 1;
    int32 Failures = 0, Accepted = 0, Rejected = 0, BothForms = 0, ArchiveOnly = 0;
    auto Expect = [&Failures](bool Passed, const FString& Name)
    {
        if (!Passed) { ++Failures; UE_LOG(LogTemp, Error, TEXT("Package gate: %s"), *Name); }
    };
    auto CheckTree = [&Expect](const FDashPackage& Package, EDashProfile Profile)
    {
        UWidgetTree* Tree = NewObject<UWidgetTree>();
        FComponentRegistry Registry;
        RegisterPlaceholderBuilders(Registry, *Tree);
        FWidgetTreeBuilder Builder;
        UWidget* RootWidget = nullptr;
        FDashLoadError Error;
        Expect(Builder.Build(Package, Profile, Registry, RootWidget, Error), Package.Path() + TEXT(" widget tree ") + Error.Message);
        Expect(Builder.WidgetsById().Num() == Package.Components().Num(), Package.Path() + TEXT(" widget IDs"));
        Tree->RootWidget = RootWidget;
    };
    for (const auto& Case : Cases)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (!Case->TryGetObject(Object) || !Object || !Object->IsValid()) { Expect(false, TEXT("Bad case object")); continue; }
        FString Name, Expected, ProfileName;
        bool OnlyArchive = false;
        if (!(*Object)->TryGetStringField(TEXT("name"), Name) || !(*Object)->TryGetStringField(TEXT("code"), Expected))
        { Expect(false, TEXT("Case lacks name or code")); continue; }
        (*Object)->TryGetBoolField(TEXT("archive_only"), OnlyArchive);
        (*Object)->TryGetStringField(TEXT("profile"), ProfileName);
        const EDashProfile Profile = ProfileName == TEXT("desktop") ? EDashProfile::Desktop : EDashProfile::Mobile;
        FDashPackage Packed;
        FDashLoadError PackedError;
        const FString ArchivePath = FPaths::Combine(Root, Name + TEXT(".udash"));
        const bool PackedOk = LoadPackage(ArchivePath, Profile, Packed, PackedError);
        Expected.IsEmpty() ? ++Accepted : ++Rejected;
        Expect(PackedOk == Expected.IsEmpty() && PackedError.CodeName == Expected, Name + TEXT(" archive expected verdict"));
        if (PackedOk) CheckTree(Packed, Profile);
        else
        {
            Expect(PackedError.Path == ArchivePath, Name + TEXT(" rejection path"));
            Expect(PackedError.DisplayText().Contains(PackedError.Pointer) &&
                PackedError.DisplayText().Contains(PackedError.CodeName) &&
                PackedError.DisplayText().Contains(FString::FromInt(PackedError.Code)), Name + TEXT(" rejection fields"));
        }
        if (OnlyArchive) { ++ArchiveOnly; continue; }
        ++BothForms;
        FDashPackage Directory;
        FDashLoadError DirectoryError;
        const bool DirectoryOk = LoadPackage(FPaths::Combine(Root, Name), Profile, Directory, DirectoryError);
        Expect(DirectoryOk == Expected.IsEmpty(), Name + TEXT(" directory expected verdict"));
        Expect(PackedOk == DirectoryOk && PackedError.Code == DirectoryError.Code &&
            PackedError.Pointer == DirectoryError.Pointer, Name + TEXT(" archive-directory parity"));
        if (DirectoryOk) CheckTree(Directory, Profile);
    }
    // An unregistered type must name the input type and exact component pointer.
    FComponentRegistry EmptyRegistry;
    FDashComponent Unknown;
    Unknown.Type = TEXT("unregistered-test");
    Unknown.Pointer = TEXT("/dashboard/components/unknown");
    FDashPackage EmptyPackage;
    FDashLoadError UnknownError;
    Expect(!EmptyRegistry.Build({Unknown, EmptyPackage, EDashProfile::Desktop, nullptr}, UnknownError) &&
        UnknownError.Pointer == Unknown.Pointer && UnknownError.Message.Contains(Unknown.Type), TEXT("unknown registry type"));
    UE_LOG(LogTemp, Display, TEXT("Package gate cases=%d accepted=%d rejected=%d both_forms=%d archive_only=%d failures=%d"),
        Cases.Num(), Accepted, Rejected, BothForms, ArchiveOnly, Failures);
    return Failures ? 1 : 0;
}
