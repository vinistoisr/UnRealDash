#include "DashPackageScreen.h"
#include "DashDebugOverlay.h"
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
#include "DashPlayerConfig.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealEngine.h"

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
        Accepted = Builder.Build(Package, Profile, Registry, Theme, State, Fraction, Content, Error, &EventLog);
        // The bindings are resolved once, here, not per frame. A string comparison against every
        // declared signal for every binding every frame is invisible on a workstation and a dropped
        // frame on a head unit.
        if (Accepted) Accepted = Bindings.Build(Package, Builder.UpdatersById(), SignalIds, Error);
        if (Accepted) Rendered.Reserve(Builder.UpdatersById().Num());
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
    Acquisition->SetEventLog(&EventLog);
    return Acquisition->Start();
}
FString UDashPackageScreen::StartConnector(const FDashPlayerConfig& Config)
{
    if (Config.Connector.IsEmpty())
    {
        // No connector is a valid way to look at a document: chunks 11 and 12's gates do exactly
        // that, measuring geometry with every gauge at a pinned deflection.
        UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=none"));
        return FString();
    }
    if (Config.Connector == TEXT("tcp"))
    {
        const FString Failure = StartTcp(Config.ConnectorHost, static_cast<uint16>(Config.ConnectorPort));
        if (Failure.IsEmpty())
            UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=tcp host=%s port=%d"),
                *Config.ConnectorHost, Config.ConnectorPort);
        return Failure;
    }
    if (Config.Connector == TEXT("replay"))
    {
        if (Config.Replay.IsEmpty()) return TEXT("-connector=replay needs -replay-file=<file>");
        const FString Failure = StartReplay(Config.Replay, Config.bReplayLoop);
        if (Failure.IsEmpty())
            UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=replay file=%s loop=%s"),
                *Config.Replay, Config.bReplayLoop ? TEXT("true") : TEXT("false"));
        return Failure;
    }
    // sim, which runs a named signal-core scenario rather than a waveform this project invented.
    if (Config.Scenario.IsEmpty()) return TEXT("-connector=sim needs -scenario=<name>");
    const FString Failure = StartNamedScenario(Config.Scenario, Config.bFreezeScenarioTime);
    if (Failure.IsEmpty())
        UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=sim scenario=%s frozen=%s"),
            *Config.Scenario, Config.bFreezeScenarioTime ? TEXT("true") : TEXT("false"));
    return Failure;
}
FString UDashPackageScreen::StartNamedScenario(const FString& Name, bool bFreezeTime)
{
    if (!Accepted) return TEXT("No package is loaded");
    FString Failure;
    // Seed and cadence are fixed rather than exposed. PLAN 4.7's surface has no flag for either,
    // and a scenario whose shape depends on an unstated default is not reproducible.
    const FString Recording = UnRealDashCore::GenerateNamedScenario(Name, 1, 30.0, 20, Failure);
    if (Recording.IsEmpty()) return Failure.IsEmpty() ? TEXT("The scenario produced no recording") : Failure;
    Acquisition = MakeUnique<UnRealDashCore::FDashAcquisition>(Recording);
    Acquisition->SetEventLog(&EventLog);
    if (bFreezeTime) Acquisition->FreezeScenarioTime();
    // The scenario names its own signals, so the bindings resolve against its numbering rather
    // than the document's ordering, exactly as they do for a definition pack.
    SignalIds = Acquisition->SignalIds();
    if (!Bindings.Build(Package, Builder.UpdatersById(), SignalIds, Error)) return Error.DisplayText();
    return Acquisition->Start();
}
FString UDashPackageScreen::StartReplay(const FString& Path, bool bLoop)
{
    if (!Accepted) return TEXT("No package is loaded");
    UnRealDashCore::FDashTcpOptions Options;
    Options.DefinitionPackText = Package.DefinitionPackText();
    Options.SchemaDirectory = UnRealDashCore::ResolveDashSchemaDirectory();
    Options.ReplayPath = Path;
    Options.bReplayLoop = bLoop;
    Acquisition = MakeUnique<UnRealDashCore::FDashAcquisition>(Options);
    Acquisition->SetEventLog(&EventLog);
    SignalIds = Acquisition->SignalIds();
    if (!Bindings.Build(Package, Builder.UpdatersById(), SignalIds, Error)) return Error.DisplayText();
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
    Acquisition->SetEventLog(&EventLog);

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
void UDashPackageScreen::EnableOverlay()
{
    DebugOverlay = CreateWidget<UDashDebugOverlay>(GetWorld(), UDashDebugOverlay::StaticClass());
    if (!DebugOverlay)
    {
        UE_LOG(LogUnRealDash, Error, TEXT("Debug overlay could not be constructed"));
        return;
    }
    DebugOverlay->InitializeSignals(Bindings.SignalNames(), SignalIds);
    DebugOverlay->AddToViewport(100);
    DebugOverlay->SetPositionInViewport(FVector2D(12.f, 12.f), false);
    DebugOverlay->SetDesiredSizeInViewport(FVector2D(420.f, 240.f + 24.f * Bindings.SignalNames().Num()));
}
void UDashPackageScreen::RemoveOverlay()
{
    DebugOverlay->RemoveFromParent();
    DebugOverlay = nullptr;
}
void UDashPackageScreen::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    // Before the early return: a run with no connector still has frames, and 4.8's per-frame rows
    // are about the renderer rather than about the data.
    EventLog.Tick(DeltaTime);
    if (DebugOverlay)
    {
        uint64 Resident = 0, Texture = 0;
        EventLog.SampledMemory(Resident, Texture);
        DebugOverlay->Advance(DeltaTime, Resident, Texture, Snapshot);
    }
    if (!Acquisition) return;
    // Acquired once per tick and passed down. Acquiring per binding could straddle a publication
    // and hand two components values from different ones, which is exactly what the triple buffer
    // exists to prevent.
    // GFrameCounter, so the acquire, submit, per-frame and present rows all key on one numbering
    // and the render thread's GFrameCounterRenderThread stamps the same frames.
    if (Acquisition->AcquireFrameSnapshot(Snapshot, static_cast<uint64>(GFrameCounter)))
    {
        // Held after the acquire and before anything is applied, so the frame carries a snapshot
        // that is deliberately older than the clock by the time it reaches the screen. That is
        // what makes a frame present a stale reading after the signal has already recovered,
        // which is the only way PLAN's delayed-presentation observation can occur on demand.
        if (HoldFrameMilliseconds > 0.f) FPlatformProcess::Sleep(HoldFrameMilliseconds / 1000.f);
        // The bindings report what they put on screen, which the event log stashes against this
        // frame until the render thread says when the frame presented. That is PLAN 4.8's present
        // row, and the bindings are the only thing that knows which signals a visible component
        // actually drives.
        Bindings.Apply(Snapshot, &Rendered);
        // Suppressing the submit suppresses the presentation record too. A frame the game thread
        // never finished with is a frame that never reached the screen, which is what PLAN's
        // clause is about; suppressing only the row would leave the samples presented.
        if (!bSuppressSubmit) EventLog.WriteRendered(Snapshot.FrameIndex, Rendered);
        // The submit row. Recorded here rather than at a real submit callback because this is
        // where the game thread is done with the snapshot it acquired; the report says so.
        if (!bSuppressSubmit) Acquisition->RecordSubmit(Snapshot.FrameIndex);
    }

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
// A rejected run has to exit non-zero, and RequestExitWithStatus(false, N) does not achieve that.
// WindowsPlatformMisc.cpp:1517 passes N to PostQuitMessage, whose wParam never reaches the process
// exit code: GuardedMain returns its own ErrorLevel, so every rejection here has been exiting 0 and
// looking like a clean run to every script that tests $LASTEXITCODE. Forcing flushes GLog and calls
// TerminateProcess with the code, which is the only path on which the code survives. Measured on
// 2026-09-18 while building the PLAN 4.7 flag gate, which is the first gate to assert an exit code.
static void RejectRun(uint8 Code)
{
    if (GLog) GLog->Flush();
    FPlatformMisc::RequestExitWithStatus(true, Code);
}
void ADashPackageHUD::BeginPlay()
{
    Super::BeginPlay();
    UnRealDashCore::InitializePackageLoader();

    // One resolver for the whole surface, command line over player.json, field by field. A
    // Shipping Android build receives no command line at all, so the file alone has to be enough.
    const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const auto Resolved = ResolveDashPlayerConfig(Saved, FCommandLine::Get(),
        [](const FString& Path)
        {
            FSmokeFileRead Read;
            Read.bExists = FPaths::FileExists(Path);
            if (Read.bExists && !FFileHelper::LoadFileToString(Read.Contents, *Path))
                Read.Error = TEXT("unreadable");
            return Read;
        },
        [](const FString& Message) { UE_LOG(LogUnRealDash, Display, TEXT("%s"), *Message); });
    const FDashPlayerConfig& Config = Resolved.Config;

    if (Resolved.bUnknown)
    {
        // Printed, not swallowed. A mistyped flag that is ignored produces a run that looks like
        // the one you asked for and is not, which is the failure every gate here exists to avoid.
        UE_LOG(LogUnRealDash, Error, TEXT("%s"), *DashPlayerSupportedFlags());
        RejectRun(2);
        return;
    }
    if (!Resolved.bRunnable)
    {
        UE_LOG(LogUnRealDash, Error, TEXT("The configuration is not runnable; see the config error lines above"));
        RejectRun(2);
        return;
    }

    if (Config.Udash.IsEmpty())
        UE_LOG(LogUnRealDash, Display, TEXT("No -udash= package supplied"));
    Screen = CreateWidget<UDashPackageScreen>(GetOwningPlayerController());
    if (!Screen) { UE_LOG(LogUnRealDash, Error, TEXT("Cannot create package screen")); return; }

    FString BatchDirectory;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-batch="), BatchDirectory))
    {
        RunBatch(BatchDirectory);
        return;
    }

    // Hidden gate switches, outside the documented surface and validated by the resolver's own
    // list so a typo in one is still rejected. They force a missing-data state and pin a gauge at
    // a deflection, neither of which means anything to a user of the player.
    UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid;
    FString StateName;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-state="), StateName) &&
        !UnRealDashCore::ParseSignalState(StateName, State))
    {
        UE_LOG(LogUnRealDash, Error, TEXT("Unknown -udash-state=%s"), *StateName);
        RejectRun(2);
        return;
    }
    float Fraction = 0.f;
    FString FractionText;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-fraction="), FractionText))
    {
        if (!FractionText.IsNumeric()) { UE_LOG(LogUnRealDash, Error, TEXT("Unparsable -udash-fraction=%s"), *FractionText); RejectRun(2); return; }
        Fraction = FCString::Atof(*FractionText);
        if (Fraction < 0.f || Fraction > 1.f)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-fraction must be between 0 and 1, got %s"), *FractionText);
            RejectRun(2);
            return;
        }
    }

    UnRealDashCore::EDashProfile Profile = UnRealDashCore::DefaultDashProfile();
    if (Config.Profile == TEXT("mobile")) Profile = UnRealDashCore::EDashProfile::Mobile;
    else if (Config.Profile == TEXT("desktop")) Profile = UnRealDashCore::EDashProfile::Desktop;

    // PLAN 4.8's event log. Always on and always at the same place, so a gate reads the logged
    // path rather than being handed a flag; chunk 16 froze the command-line surface and 4.8 names
    // no flag of its own.
    UnRealDashCore::FDashEventLogOptions LogOptions;
    LogOptions.Path = FPaths::Combine(Saved, TEXT("events"), TEXT("run.jsonl"));
    LogOptions.BuildId = FApp::GetBuildVersion();
    FString LaunchText;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-launch-counter="), LaunchText))
    {
        uint64 Counter = 0;
        bool Valid = !LaunchText.IsEmpty();
        for (TCHAR Digit : LaunchText)
        {
            if (Digit < TEXT('0') || Digit > TEXT('9') ||
                Counter > (MAX_uint64 - static_cast<uint64>(Digit - TEXT('0'))) / 10)
            { Valid = false; break; }
            Counter = Counter * 10 + Digit - TEXT('0');
        }
        if (Valid && Counter > 0) LogOptions.LaunchCounter = Counter;
        else UE_LOG(LogUnRealDash, Error, TEXT("DashEventLog invalid launch counter: %s; launch endpoint unavailable"), *LaunchText);
    }
    FParse::Value(FCommandLine::Get(), TEXT("udash-launch-evidence="), LogOptions.LaunchEvidencePath);
    LogOptions.Rhi = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("unknown");
    // GSystemResolution rather than the viewport: BeginPlay runs before the game viewport has a
    // size, so reading it there wrote 0 by 0 into the header of every run.
    LogOptions.Resolution = FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY);
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Viewport = GEngine->GameViewport->Viewport->GetSizeXY();
        if (Viewport.X > 0 && Viewport.Y > 0) LogOptions.Resolution = Viewport;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogOptions.Path), true);
    FirstFrameCounter = static_cast<uint64>(GFrameCounter);
    const FString LogFailure = Screen->OpenEventLog(LogOptions);
    if (LogFailure.IsEmpty())
    {
        UE_LOG(LogUnRealDash, Display, TEXT("DashEventLog path=%s"), *LogOptions.Path);
    }
    else
    {
        // Not fatal. A dashboard that refuses to start because it could not write telemetry about
        // itself is worse than one that starts without it, so this says so and carries on.
        UE_LOG(LogUnRealDash, Error, TEXT("DashEventLog unavailable: %s"), *LogFailure);
    }
    Screen->Open(Config.Udash, Profile, State, Fraction);
    Screen->AddToViewport();
    UE_LOG(LogUnRealDash, Display, TEXT("DashVerdict accepted=%s code=%s pointer=%s path=%s state=%s"),
        Screen->WasAccepted() ? TEXT("true") : TEXT("false"),
        *Screen->LastError().CodeName, *Screen->LastError().Pointer, *Config.Udash,
        UnRealDashCore::SignalStateName(State));

    // The hidden sweep switch, which is not -scenario= and must not be confused with it. A named
    // signal-core scenario carries speed and temperature; this one synthesises a sweep over the
    // document's OWN signals and their declared ranges, which is the only way the chunk 14 live
    // gate can move a gauge in a fixture that declares neither of those names. It was called
    // -udash-scenario= until chunk 16, where the two meanings became one word apart.
    FString SweepSeconds;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-sweep="), SweepSeconds))
    {
        if (!SweepSeconds.IsNumeric() || FCString::Atod(*SweepSeconds) <= 0.0)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-sweep must be a positive number of seconds, got %s"), *SweepSeconds);
            RejectRun(2);
            return;
        }
        const FString SweepFailure = Screen->StartScenario(FCString::Atod(*SweepSeconds));
        if (!SweepFailure.IsEmpty())
        {
            UE_LOG(LogUnRealDash, Error, TEXT("Sweep failed: %s"), *SweepFailure);
            RejectRun(2);
            return;
        }
        UE_LOG(LogUnRealDash, Display, TEXT("DashConnector kind=sweep seconds=%s"), *SweepSeconds);
    }

    const FString Failure = SweepSeconds.IsEmpty() ? Screen->StartConnector(Config) : FString();
    if (!Failure.IsEmpty())
    {
        UE_LOG(LogUnRealDash, Error, TEXT("Connector failed: %s"), *Failure);
        RejectRun(2);
        return;
    }

    // Gate-only forcing. Read after the connector has started so a suppressed submit applies
    // from the first frame that has anything to submit.
    if (FParse::Param(FCommandLine::Get(), TEXT("udash-suppress-submit")))
    {
        Screen->ForceSuppressSubmit();
        UE_LOG(LogUnRealDash, Display, TEXT("DashForcing suppress-submit"));
    }
    FString HoldText;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-hold-frame-ms="), HoldText))
    {
        if (!HoldText.IsNumeric() || FCString::Atof(*HoldText) < 0.f)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-hold-frame-ms must be a non-negative number, got %s"), *HoldText);
            RejectRun(2);
            return;
        }
        Screen->ForceHoldFrame(FCString::Atof(*HoldText));
        UE_LOG(LogUnRealDash, Display, TEXT("DashForcing hold-frame-ms=%s"), *HoldText);
    }
    FString KillText;
    if (FParse::Value(FCommandLine::Get(), TEXT("udash-kill-at="), KillText))
    {
        if (!KillText.IsNumeric() || FCString::Atof(*KillText) <= 0.f)
        {
            UE_LOG(LogUnRealDash, Error, TEXT("-udash-kill-at must be a positive number of seconds, got %s"), *KillText);
            RejectRun(2);
            return;
        }
        // A hard exit with no EndPlay, so nothing writes run_end cancellations and the armed
        // expiries in flight are left unresolved, which is the state PLAN wants counted.
        FTimerHandle Kill;
        GetWorldTimerManager().SetTimer(Kill, FTimerDelegate::CreateLambda([]()
            { FPlatformMisc::RequestExitWithStatus(true, 9); }), FCString::Atof(*KillText), false);
        UE_LOG(LogUnRealDash, Display, TEXT("DashForcing kill-at=%s"), *KillText);
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("udash-overlay"))) Screen->EnableOverlay();

    if (Config.QuitAfterSeconds > 0)
    {
        FTimerHandle Quit;
        GetWorldTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]()
            { FPlatformMisc::RequestExit(false); }), static_cast<float>(Config.QuitAfterSeconds), false);
    }
    if (!Config.Screenshot.IsEmpty())
    {
        ShotPath = Config.Screenshot;
        FString At;
        float Delay = 0.f;
        if (FParse::Value(FCommandLine::Get(), TEXT("udash-shot-at="), At))
        {
            if (!At.IsNumeric() || FCString::Atof(*At) < 0.f)
            {
                UE_LOG(LogUnRealDash, Error, TEXT("-udash-shot-at must be a non-negative number of seconds, got %s"), *At);
                RejectRun(2);
                return;
            }
            Delay = FCString::Atof(*At);
        }
        ScheduleShot(Delay);
    }
}
void ADashPackageHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Screen)
    {
        const UnRealDashCore::FDashEventLogCounts Counts = Screen->EventLogCounts();
        Screen->CloseEventLog();
        // engine_frames comes from GFrameCounter rather than from the log's own counter, or
        // criterion 1 would be the log checked against itself.
        UE_LOG(LogUnRealDash, Display,
            TEXT("DashEventLog records=%llu frames=%llu samples=%llu dropped=%llu bytes=%llu engine_frames=%llu ")
            TEXT("receives=%llu acquires=%llu submits=%llu presents=%llu applied=%llu"),
            Counts.Records, Counts.Frames, Counts.Samples, Counts.Dropped, Counts.Bytes,
            static_cast<uint64>(GFrameCounter) - FirstFrameCounter,
            Counts.Receives, Counts.Acquires, Counts.Submits, Counts.Presents,
            Screen->SamplesApplied());
    }
    Super::EndPlay(Reason);
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
