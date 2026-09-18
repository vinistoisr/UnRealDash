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
    // TreeCode is empty for a package whose tree must build, and an error code for one that loads
    // but that the runtime refuses to render. A document can be schema-legal and still name
    // something this runtime will not draw; that has to be a readable error naming the pointer, and
    // this is where that is gated rather than in a person's memory.
    auto CheckTree = [&Expect](const FDashPackage& Package, EDashProfile Profile,
        const FString& TreeCode, const FString& TreePointer)
    {
        UWidgetTree* Tree = NewObject<UWidgetTree>();
        FComponentRegistry Registry;
        RegisterStage0Builders(Registry, *Tree);
        FWidgetTreeBuilder Builder;
        UWidget* RootWidget = nullptr;
        FDashLoadError Error;
        FDashTheme Theme;
        Expect(Theme.Load(Package, Error), Package.Path() + TEXT(" theme ") + Error.Message);
        const bool bBuilt = Builder.Build(Package, Profile, Registry, Theme, EDashSignalState::Valid, 0.5f, RootWidget, Error);
        if (TreeCode.IsEmpty())
        {
            Expect(bBuilt, Package.Path() + TEXT(" widget tree ") + Error.Message);
            Expect(Builder.WidgetsById().Num() == Package.Components().Num(), Package.Path() + TEXT(" widget IDs"));
        }
        else
        {
            Expect(!bBuilt, Package.Path() + TEXT(" widget tree should have been refused"));
            Expect(Error.CodeName == TreeCode, Package.Path() + TEXT(" tree code ") + Error.CodeName);
            Expect(Error.Pointer.EndsWith(TreePointer), Package.Path() + TEXT(" tree pointer ") + Error.Pointer);
            Expect(!Error.Message.IsEmpty(), Package.Path() + TEXT(" tree message is empty"));
            Expect(Error.DisplayText().Contains(Error.Pointer), Package.Path() + TEXT(" tree error names its pointer"));
        }
        Tree->RootWidget = RootWidget;
    };
    for (const auto& Case : Cases)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (!Case->TryGetObject(Object) || !Object || !Object->IsValid()) { Expect(false, TEXT("Bad case object")); continue; }
        FString Name, Expected, ProfileName, TreeCode, TreePointer;
        bool OnlyArchive = false;
        if (!(*Object)->TryGetStringField(TEXT("name"), Name) || !(*Object)->TryGetStringField(TEXT("code"), Expected))
        { Expect(false, TEXT("Case lacks name or code")); continue; }
        (*Object)->TryGetBoolField(TEXT("archive_only"), OnlyArchive);
        (*Object)->TryGetStringField(TEXT("profile"), ProfileName);
        (*Object)->TryGetStringField(TEXT("tree_code"), TreeCode);
        (*Object)->TryGetStringField(TEXT("tree_pointer"), TreePointer);
        const EDashProfile Profile = ProfileName == TEXT("desktop") ? EDashProfile::Desktop : EDashProfile::Mobile;
        FDashPackage Packed;
        FDashLoadError PackedError;
        const FString ArchivePath = FPaths::Combine(Root, Name + TEXT(".udash"));
        const bool PackedOk = LoadPackage(ArchivePath, Profile, Packed, PackedError);
        Expected.IsEmpty() ? ++Accepted : ++Rejected;
        Expect(PackedOk == Expected.IsEmpty() && PackedError.CodeName == Expected, Name + TEXT(" archive expected verdict"));
        if (PackedOk) CheckTree(Packed, Profile, TreeCode, TreePointer);
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
        if (DirectoryOk) CheckTree(Directory, Profile, TreeCode, TreePointer);
    }
    // An unregistered type must name the input type and exact component pointer.
    FComponentRegistry EmptyRegistry;
    FDashComponent Unknown;
    Unknown.Type = TEXT("unregistered-test");
    Unknown.Pointer = TEXT("/dashboard/components/unknown");
    FDashPackage EmptyPackage;
    FDashLoadError UnknownError;
    FDashTheme EmptyTheme;
    Expect(!EmptyRegistry.Build({Unknown, EmptyPackage, EDashProfile::Desktop, nullptr, EmptyTheme,
            EDashSignalState::Valid, 0.f}, UnknownError).Widget &&
        UnknownError.Pointer == Unknown.Pointer && UnknownError.Message.Contains(Unknown.Type), TEXT("unknown registry type"));
    // An undeclared theme token must name the token and the caller's pointer. This is checked at
    // the API level rather than through a document because it cannot be reached through one: the
    // semantic pass resolves every token a document carries, component colours and missing_data
    // alike, before LoadPackage returns. See chunk-11 revision 4 C7.
    FDashTheme BareTheme;
    FLinearColor Resolved;
    FDashLoadError TokenError;
    Expect(!BareTheme.Resolve(TEXT("no-such-token"), TEXT("/dashboard/components/x/properties/colour/token"),
            Resolved, TokenError) &&
        TokenError.CodeName == TEXT("E_UNRESOLVED_THEME_TOKEN") &&
        TokenError.Pointer == TEXT("/dashboard/components/x/properties/colour/token") &&
        TokenError.Message.Contains(TEXT("no-such-token")), TEXT("unresolved theme token"));
    // And a colour that is neither a hex string nor a token object is reported, not approximated.
    FDashLoadError ColourError;
    Expect(!BareTheme.ResolveColour(FDashValue(), TEXT("/dashboard/components/x/properties/colour"),
            Resolved, ColourError) && ColourError.CodeName == TEXT("E_SCHEMA") &&
        ColourError.Pointer == TEXT("/dashboard/components/x/properties/colour"), TEXT("malformed colour"));
    UE_LOG(LogTemp, Display, TEXT("Package gate cases=%d accepted=%d rejected=%d both_forms=%d archive_only=%d failures=%d"),
        Cases.Num(), Accepted, Rejected, BothForms, ArchiveOnly, Failures);
    return Failures ? 1 : 0;
}
