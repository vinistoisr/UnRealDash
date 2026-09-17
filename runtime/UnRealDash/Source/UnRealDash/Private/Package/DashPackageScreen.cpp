#include "DashPackageScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnRealDashLog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

void UDashPackageScreen::Open(const FString& Path, UnRealDashCore::EDashProfile InProfile)
{
    Profile = InProfile;
    if (Path.IsEmpty())
    {
        Error = { TEXT("E_SCHEMA"), 7, TEXT(""), TEXT("No package supplied. Launch with -udash=<path>."), Path };
        Accepted = false;
    }
    else Accepted = UnRealDashCore::LoadPackage(Path, Profile, Package, Error);
    if (!Accepted) UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error.DisplayText());
}
TSharedRef<SWidget> UDashPackageScreen::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("PackageWidgetTree"));
    UWidget* Content = nullptr;
    if (Accepted)
    {
        UnRealDashCore::FComponentRegistry Registry;
        UnRealDashCore::RegisterPlaceholderBuilders(Registry, *WidgetTree);
        Accepted = Builder.Build(Package, Profile, Registry, Content, Error);
        if (!Accepted) UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error.DisplayText());
    }
    if (!Accepted)
    {
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Error.DisplayText()));
        Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        auto Font = Text->GetFont();
        Font.Size = 26;
        Text->SetFont(Font);
        Text->SetWrapTextAt(1100.f);
        Text->SetAutoWrapText(true);
        Content = Text;
    }
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 1.f));
    Background->SetPadding(FMargin(24.f));
    Background->SetContent(Content);
    auto* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(1160.f);
    Width->SetContent(Background);
    auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Scale->SetContent(Width);
    WidgetTree->RootWidget = Scale;
    return Super::RebuildWidget();
}
void ADashPackageHUD::BeginPlay()
{
    Super::BeginPlay();
    UnRealDashCore::InitializePackageLoader();
    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("udash="), Path))
        UE_LOG(LogUnRealDash, Display, TEXT("No -udash= package supplied"));
    Screen = CreateWidget<UDashPackageScreen>(GetOwningPlayerController());
    if (!Screen) { UE_LOG(LogUnRealDash, Error, TEXT("Cannot create package screen")); return; }
    FString BatchDirectory;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-batch="), BatchDirectory))
    {
        RunBatch(BatchDirectory);
        return;
    }
    Screen->Open(Path, UnRealDashCore::DefaultDashProfile());
    Screen->AddToViewport();
    // One machine-readable verdict per run. The device half of the PLAN 4.4 gate is driven by
    // launching once per fixture and reading logcat, and an accepted case otherwise produces no
    // output at all, which would make "it worked" indistinguishable from "it died early".
    UE_LOG(LogUnRealDash, Display, TEXT("DashVerdict accepted=%s code=%s pointer=%s path=%s"),
        Screen->WasAccepted() ? TEXT("true") : TEXT("false"),
        *Screen->LastError().CodeName, *Screen->LastError().Pointer, *Path);
}
// Gate-only batch mode. Launching the packaged player once per fixture costs about 85 seconds of
// cold start on the device, so the 69 runs of the PLAN 4.4 device gate take an hour and a half and
// will not fit in any window the owner is likely to have the phone plugged in. This loads every
// fixture in one run and emits the same DashVerdict line per case, so the runner parses identical
// output either way.
//
// Like the smoke commandlet's -stress switch, this is a gate switch and is deliberately not part
// of the runtime command-line surface that PLAN 4.7 owns.
void ADashPackageHUD::RunBatch(const FString& Directory)
{
    // The profile is a launch argument, not something read per fixture, because cases.json is test
    // data and the player must not carry test expectations. The runner launches once per profile
    // and picks the verdict matching each case's profile, which is two launches rather than 87.
    UnRealDashCore::EDashProfile BatchProfile = UnRealDashCore::DefaultDashProfile();
    FString ProfileName;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-profile="), ProfileName))
    {
        if (ProfileName.Equals(TEXT("mobile"), ESearchCase::IgnoreCase)) { BatchProfile = UnRealDashCore::EDashProfile::Mobile; }
        else if (ProfileName.Equals(TEXT("desktop"), ESearchCase::IgnoreCase)) { BatchProfile = UnRealDashCore::EDashProfile::Desktop; }
        else { UE_LOG(LogUnRealDash, Error, TEXT("Unknown -udash-profile=%s"), *ProfileName); FPlatformMisc::RequestExit(false); return; }
    }
    UE_LOG(LogUnRealDash, Display, TEXT("DashBatch profile=%s"),
        BatchProfile == UnRealDashCore::EDashProfile::Mobile ? TEXT("mobile") : TEXT("desktop"));
    TArray<FString> Entries;
    IFileManager::Get().FindFiles(Entries, *(Directory / TEXT("*.udash")), true, false);
    TArray<FString> Directories;
    IFileManager::Get().FindFiles(Directories, *(Directory / TEXT("*")), false, true);
    int32 Accepted = 0, Rejected = 0;
    auto RunOne = [&](const FString& Target)
    {
        UnRealDashCore::FDashPackage Package;
        UnRealDashCore::FDashLoadError Error;
        const bool bOk = UnRealDashCore::LoadPackage(Target, BatchProfile, Package, Error);
        bOk ? ++Accepted : ++Rejected;
        UE_LOG(LogUnRealDash, Display, TEXT("DashVerdict accepted=%s code=%s pointer=%s path=%s"),
            bOk ? TEXT("true") : TEXT("false"), *Error.CodeName, *Error.Pointer, *Target);
    };
    for (const FString& Entry : Entries) { RunOne(Directory / Entry); }
    for (const FString& Entry : Directories) { RunOne(Directory / Entry); }
    UE_LOG(LogUnRealDash, Display, TEXT("DashBatch total=%d accepted=%d rejected=%d"),
        Accepted + Rejected, Accepted, Rejected);
    FPlatformMisc::RequestExit(false);
}
ADashPackageGameMode::ADashPackageGameMode()
{
    HUDClass = ADashPackageHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
