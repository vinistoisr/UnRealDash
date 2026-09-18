#include "DashPackageScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnRealDashLog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UnRealDashCore/DashScenario.h"

void UDashPackageScreen::Open(const FString& Path, UnRealDashCore::EDashProfile InProfile,
    UnRealDashCore::EDashSignalState InState, float InFraction)
{
    Profile = InProfile;
    State = InState;
    Fraction = InFraction;
    if (Path.IsEmpty())
    {
        Error = { TEXT("E_SCHEMA"), 7, TEXT(""), TEXT("No package supplied. Launch with -udash=<path>."), Path };
        Accepted = false;
    }
    else Accepted = UnRealDashCore::LoadPackage(Path, Profile, Package, Error);
    if (Accepted) Accepted = Theme.Load(Package, Error);
    if (!Accepted) UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error.DisplayText());
}
TSharedRef<SWidget> UDashPackageScreen::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("PackageWidgetTree"));
    UWidget* Content = nullptr;
    if (Accepted)
    {
        UnRealDashCore::FComponentRegistry Registry;
        UnRealDashCore::RegisterStage0Builders(Registry, *WidgetTree);
        Accepted = Builder.Build(Package, Profile, Registry, Theme, State, Fraction, Content, Error);
        // The bindings are resolved once, here, not per frame. A string comparison against every
        // declared signal for every binding every frame is invisible on a workstation and a dropped
        // frame on a head unit.
        if (Accepted) Accepted = Bindings.Build(Package, Builder.UpdatersById(), SignalIds, Error);
        if (!Accepted) UE_LOG(LogUnRealDash, Error, TEXT("%s"), *Error.DisplayText());
    }
    WidgetTree->RootWidget = Accepted ? BuildDocumentRoot(Content) : BuildErrorRoot();
    return Super::RebuildWidget();
}
// Document units map one to one onto pixels whenever the capture resolution matches the document's
// reference viewport. That mapping is what the PLAN 4.5 screenshot gate's arithmetic rests on: a
// component declared 320 by 180 has to cover 57,600 pixels of a 1280 by 720 frame, or the mutation
// check can move fewer pixels than the pass threshold and pass the gate it exists to break.
//
// So nothing here adds padding, insets or a second scale. The one scale is the reference viewport
// to the real viewport, and the root component's `scaling` chooses how it behaves when the two
// differ. At equal sizes every option gives exactly 1.0.
int32 UDashPackageScreen::CountWidgets(UWidget* Root)
{
    if (!Root) return 0;
    int32 Total = 1;
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            Total += CountWidgets(Panel->GetChildAt(Index));
    else if (UContentWidget* Content = Cast<UContentWidget>(Root))
        Total += CountWidgets(Content->GetContent());
    return Total;
}
FString UDashPackageScreen::StartScenario(double DurationSeconds)
{
    if (!Accepted) return TEXT("No package is loaded");
    UnRealDashCore::FDashScenarioOptions Options;
    Options.DurationSeconds = DurationSeconds;
    FString Failure;
    const FString Recording = UnRealDashCore::BuildScenarioRecording(Bindings.SignalNames(), Bindings.SignalRanges(), Options, Failure);
    if (Recording.IsEmpty()) return Failure;
    Acquisition = MakeUnique<UnRealDashCore::FDashAcquisition>(Recording);
    return Acquisition->Start();
}
FString UDashPackageScreen::StartTcp(const FString& Host, uint16 Port)
{
    if (!Accepted) return TEXT("No package is loaded");
    UnRealDashCore::FDashTcpOptions Options;
    Options.Host = Host;
    Options.Port = Port;
    Options.DefinitionPackText = Package.DefinitionPackText();
    Options.SchemaDirectory = UnRealDashCore::ResolveDashSchemaDirectory();
    Acquisition = MakeUnique<UnRealDashCore::FDashAcquisition>(Options);

    // The pack numbers the signals, so the bindings are resolved against its numbering rather than
    // the document's ordering. This is the one case where the binding table has to be rebuilt after
    // the tree, because until the pack is parsed nothing knows what the ids are.
    SignalIds = Acquisition->SignalIds();
    if (!Bindings.Build(Package, Builder.UpdatersById(), SignalIds, Error))
        return Error.DisplayText();
    return Acquisition->Start();
}
UnRealDashCore::FConnectionHealth UDashPackageScreen::GetHealth() const
{
    return Acquisition ? Acquisition->GetHealth() : UnRealDashCore::FConnectionHealth{};
}
void UDashPackageScreen::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!Acquisition) return;
    // Acquired once per tick and passed down. Acquiring per binding could straddle a publication
    // and hand two components values from different ones, which is exactly what the triple buffer
    // exists to prevent.
    if (Acquisition->AcquireFrameSnapshot(Snapshot)) Bindings.Apply(Snapshot);

    ++Ticks;
    // One health line a second. PLAN 4.9's criterion 6 is a sequence rather than a state, so a
    // capture cannot show it: the relay is killed, every mapped signal goes stale, the relay comes
    // back, and the connector reconnects with no player restart. The log is what carries that.
    // Accumulated, not ticks multiplied by the current delta. That estimate went backwards
    // between lines whenever a frame ran long, which made the log's own timeline untrustworthy.
    ElapsedSeconds += DeltaTime;
    HealthSeconds += DeltaTime;
    if (HealthSeconds >= 1.0f)
    {
        HealthSeconds = 0.f;
        const UnRealDashCore::FConnectionHealth Health = GetHealth();
        UE_LOG(LogUnRealDash, Display,
            TEXT("DashHealth t=%.1f connected=%s generation=%llu reconnects=%llu bytes=%llu backoff_ms=%llu attempts=%llu"),
            ElapsedSeconds, Health.bConnected ? TEXT("true") : TEXT("false"), Health.Generation,
            Health.Reconnects, Health.Bytes, Health.BackoffMilliseconds, Health.AttemptsSinceConnect);
    }
    const int32 Widgets = WidgetTree ? WidgetTree->RootWidget ? CountWidgets(WidgetTree->RootWidget) : 0 : 0;
    if (Ticks == 1)
    {
        FirstTickWidgets = Widgets;
        UE_LOG(LogUnRealDash, Display, TEXT("DashWidgets tick=1 count=%d"), Widgets);
    }
    else if (Ticks == 600)
    {
        UE_LOG(LogUnRealDash, Display, TEXT("DashWidgets tick=600 count=%d first=%d"), Widgets, FirstTickWidgets);
    }
}
UWidget* UDashPackageScreen::BuildDocumentRoot(UWidget* Content)
{
    const FIntPoint Viewport = Package.ReferenceViewport();
    FString Scaling;
    for (const auto& Node : Package.Components())
        if (Node.ParentId.IsEmpty()) { Node.Node.Member(TEXT("scaling")).String(Scaling); break; }

    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(static_cast<float>(Viewport.X));
    Size->SetHeightOverride(static_cast<float>(Viewport.Y));
    Size->SetContent(Content);

    auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(Scaling == TEXT("stretch") ? EStretch::Fill
        : Scaling == TEXT("none") ? EStretch::None : EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::Both);
    Scale->SetContent(Size);

    // Opaque black behind the document, so any letterboxed area is a constant colour rather than
    // whatever the renderer last left there. A capture gate cannot tolerate an undefined margin.
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor::Black);
    Background->SetPadding(FMargin(0.f));
    Background->SetContent(Scale);
    return Background;
}
UWidget* UDashPackageScreen::BuildErrorRoot()
{
    // The error path keeps its readable padded layout. There is no document geometry to honour
    // here, and a rejection screen is read by a person rather than by a comparator.
    auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Error.DisplayText()));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    auto Font = Text->GetFont();
    Font.Size = 26;
    Text->SetFont(Font);
    Text->SetWrapTextAt(1100.f);
    Text->SetAutoWrapText(true);
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 1.f));
    Background->SetPadding(FMargin(24.f));
    Background->SetContent(Text);
    auto* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(1160.f);
    Width->SetContent(Background);
    auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Scale->SetContent(Width);
    return Scale;
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
    // Gate-only, like -udash-batch= and the smoke commandlet's -stress switch, and deliberately not
    // part of the runtime command-line surface PLAN 4.7 owns. Criterion 4 of PLAN 4.5 needs three
    // captures of one frozen document under three missing-data states, and a frozen document has no
    // way to reach age_unknown or stale on its own: it binds constants, and the capture conditions
    // forbid a running scenario. Values stay frozen; only the presented state changes.
    UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid;
    FString StateName;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-state="), StateName) &&
        !UnRealDashCore::ParseSignalState(StateName, State))
    {
        // A mistyped state must not quietly capture the valid frame and report green.
        UE_LOG(LogUnRealDash, Error, TEXT("Unknown -udash-state=%s"), *StateName);
        FPlatformMisc::RequestExit(false);
        return;
    }
    // The PLAN 4.6 counterpart of -udash-state=, and a gate switch for the same reason: the
    // connectors that supply real values are PLAN 4.9, and a dial cannot be captured at a known
    // deflection before one exists. It supplies one synthetic constant to every bound gauge, as a
    // fraction of that gauge's declared range. It is not data and the reports say so.
    float Fraction = 0.f;
    FString FractionText;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-fraction="), FractionText))
    {
        if (!FractionText.IsNumeric())
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Unparsable -udash-fraction=%s"), *FractionText);
            FPlatformMisc::RequestExit(false);
            return;
        }
        Fraction = FCString::Atof(*FractionText);
        if (Fraction < 0.f || Fraction > 1.f)
        {
            // Clamping silently would let a mistyped gate run capture a different deflection than
            // the one it printed and still look green.
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-fraction must be between 0 and 1, got %s"), *FractionText);
            FPlatformMisc::RequestExit(false);
            return;
        }
    }
    Screen->Open(Path, UnRealDashCore::DefaultDashProfile(), State, Fraction);
    Screen->AddToViewport();
    // The PLAN 4.9 value source, until a connector exists. It generates bytes and the real pipeline
    // consumes them, so a signal goes stale here because a deadline passed. Gate-only, beside
    // -udash-state= and -udash-fraction=, and outside the runtime flag surface PLAN 4.7 owns.
    FString ScenarioSeconds;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-scenario="), ScenarioSeconds))
    {
        const double Duration = ScenarioSeconds.IsNumeric() ? FCString::Atod(*ScenarioSeconds) : 0.0;
        if (Duration <= 0.0)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-scenario must be a positive number of seconds, got %s"),
                *ScenarioSeconds);
            FPlatformMisc::RequestExit(false);
            return;
        }
        const FString Failure = Screen->StartScenario(Duration);
        if (!Failure.IsEmpty())
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Scenario failed: %s"), *Failure);
            FPlatformMisc::RequestExit(false);
            return;
        }
        UE_LOG(LogUnRealDash, Display, TEXT("DashScenario seconds=%.3f"), Duration);
    }

    // PLAN 4.9's connector selection. Only the tcp value is wired here; sim is the scenario switch
    // above and replay is a file, both of which the rest of 4.9 folds in. An unknown value exits
    // rather than falling back, so a mistyped connector cannot look like a working run.
    FString Connector;
    if (FParse::Value(FCommandLine::Get(), TEXT("connector="), Connector))
    {
        if (!Connector.Equals(TEXT("tcp"), ESearchCase::IgnoreCase))
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Unknown -connector=%s; this build wires tcp"), *Connector);
            FPlatformMisc::RequestExit(false);
            return;
        }
        FString Host = TEXT("127.0.0.1");
        FParse::Value(FCommandLine::Get(), TEXT("connector-host="), Host);
        int32 Port = 35000;
        FParse::Value(FCommandLine::Get(), TEXT("connector-port="), Port);
        if (Port <= 0 || Port > 65535)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-connector-port must be 1 through 65535, got %d"), Port);
            FPlatformMisc::RequestExit(false);
            return;
        }
        const FString Failure = Screen->StartTcp(Host, static_cast<uint16>(Port));
        if (!Failure.IsEmpty())
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Connector failed: %s"), *Failure);
            FPlatformMisc::RequestExit(false);
            return;
        }
        UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=tcp host=%s port=%d"), *Host, Port);
    }
    // One machine-readable verdict per run. The device half of the PLAN 4.4 gate is driven by
    // launching once per fixture and reading logcat, and an accepted case otherwise produces no
    // output at all, which would make "it worked" indistinguishable from "it died early".
    UE_LOG(LogUnRealDash, Display, TEXT("DashVerdict accepted=%s code=%s pointer=%s path=%s state=%s"),
        Screen->WasAccepted() ? TEXT("true") : TEXT("false"),
        *Screen->LastError().CodeName, *Screen->LastError().Pointer, *Path,
        UnRealDashCore::SignalStateName(State));
    UE_LOG(LogUnRealDash, Display, TEXT("DashFraction value=%.4f"), Fraction);
    // Gate-only, like the rest. A connector gate needs a run that outlives a relay being killed
    // and restarted, and a capture cannot end it early or the sequence never happens.
    FString QuitAfter;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-quit-after="), QuitAfter) && QuitAfter.IsNumeric())
    {
        FTimerHandle Quit;
        GetWorldTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]()
            { FPlatformMisc::RequestExit(false); }), FCString::Atof(*QuitAfter), false);
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-shot="), ShotPath))
    {
        FString At;
        float Delay = 0.f;
        if (FParse::Value(FCommandLine::Get(), TEXT("udash-shot-at="), At))
        {
            if (!At.IsNumeric() || FCString::Atof(*At) < 0.f)
            {
                UE_LOG(LogUnRealDash, Error, TEXT("-udash-shot-at must be a non-negative number of seconds, got %s"), *At);
                FPlatformMisc::RequestExit(false);
                return;
            }
            Delay = FCString::Atof(*At);
        }
        ScheduleShot(Delay);
    }
}
// Gate-only capture. bShowUI is true and must stay true: everything this project draws is UI, and
// a capture taken with false writes a black image, so a gate comparing two of them would pass for
// every fixture forever. That was found on the device on 2026-09-17 and is recorded in
// docs/reports/2026-09-17-device-package-gate.md.
void ADashPackageHUD::ScheduleShot(float DelaySeconds)
{
    // Eight settling frames is the original behaviour, for a still document. A delay on top of it
    // is what lets a gate capture the same scenario at two different points and compare them, which
    // is the only way to show that a bound widget actually moved rather than merely rendered.
    ShotFramesRemaining = 8;
    ShotDelaySeconds = DelaySeconds;
    GetWorldTimerManager().SetTimer(ShotTimer, this, &ADashPackageHUD::TickShot, 0.05f, true);
}
void ADashPackageHUD::TickShot()
{
    if (--ShotFramesRemaining > 0) return;
    GetWorldTimerManager().ClearTimer(ShotTimer);
    if (ShotDelaySeconds > 0.f)
    {
        const float Delay = ShotDelaySeconds;
        ShotDelaySeconds = 0.f;
        ShotFramesRemaining = 1;
        GetWorldTimerManager().SetTimer(ShotTimer, this, &ADashPackageHUD::TickShot, Delay, false);
        return;
    }
    FScreenshotRequest::RequestScreenshot(ShotPath, true, false);
    UE_LOG(LogUnRealDash, Display, TEXT("DashShot path=%s"), *ShotPath);
    // One more beat so the request is serviced before the process leaves.
    GetWorldTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateLambda([]()
        { FPlatformMisc::RequestExit(false); }), 1.0f, false);
}
// Gate-only batch mode. Launching the packaged player once per fixture costs about 85 seconds of
// cold start on the device, so the 69 runs of the PLAN 4.4 device gate take an hour and a half and
// will not fit in any window the owner is likely to have the phone plugged in. This loads every
// fixture in one run and emits the same DashVerdict line per case, so the runner parses identical
// output either way.
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
