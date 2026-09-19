#pragma once
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "UnRealDashCore/ComponentRegistry.h"
#include "UnRealDashCore/DashAcquisition.h"
#include "UnRealDashCore/DashBindingTable.h"
#include "UnRealDashCore/DashEventLog.h"
#include "DashPackageScreen.generated.h"

UCLASS()
class UDashPackageScreen : public UUserWidget
{
    GENERATED_BODY()
public:
    void Open(const FString& Path, UnRealDashCore::EDashProfile Profile,
        UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid,
        float Fraction = 0.f);
    // Starts the PLAN 4.9 value source. Returns an empty string on success. It generates bytes and
    // everything downstream of it is the real pipeline, so a signal here goes stale because a
    // deadline passed rather than because something decided to say so.
    // One entry point for all three connectors. Which one is a document-independent choice the
    // configuration makes, so the screen does not need three call sites that can drift.
    FString StartConnector(const struct FDashPlayerConfig& Config);
    FString StartScenario(double DurationSeconds);
    // PLAN 4.8. Empty on success; a failure is reported and the run continues.
    FString OpenEventLog(const UnRealDashCore::FDashEventLogOptions& Options) { return EventLog.Open(Options); }
    void CloseEventLog()
    {
        // Detached first. The acquisition thread writes receive rows through this pointer and the
        // log is about to stop existing under it.
        if (Acquisition) Acquisition->SetEventLog(nullptr);
        EventLog.Close();
    }
    UnRealDashCore::FDashEventLogCounts EventLogCounts() const { return EventLog.Counts(); }
    // The pipeline's own count of samples Apply accepted. PLAN 4.8's partition sums to this, and
    // comparing it against the receive row count is what stops the denominator being the log's
    // own account of what it managed to write.
    uint64 SamplesApplied() const { return Acquisition ? Acquisition->SamplesApplied() : 0; }
    // A named signal-core scenario, run through the real pipeline like every other source.
    FString StartNamedScenario(const FString& Name, bool bFreezeTime);
    // Recorded bytes from a file, through the same framer and decoder a socket uses.
    FString StartReplay(const FString& Path, bool bLoop);
    // PLAN 4.9's third connector: a receive-only binary-telemetry-v1 client. Signals and their
    // numeric ids come from the package's definition pack, so the binding table has to be built
    // after this rather than before, which is why it takes the package path again.
    FString StartTcp(const FString& Host, uint16 Port);
    // Health, for the gate to read reconnects and backoff out of the log.
    UnRealDashCore::FConnectionHealth GetHealth() const;
    bool HasAcquisition() const { return Acquisition.IsValid(); }
    // Read by the HUD to emit one machine-readable verdict line per run; the device half of the
    // PLAN 4.4 gate parses it out of logcat, where an accepted case otherwise logs nothing at all.
    bool WasAccepted() const { return Accepted; }
    const UnRealDashCore::FDashLoadError& LastError() const { return Error; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    // A native UUserWidget subclass ticks: UpdateCanTick sets bCanTick when the class is not a
    // UWidgetBlueprintGeneratedClass, which a C++ class never is (UserWidget.cpp, UpdateCanTick).
    // The DisableNativeTick metadata on UUserWidget governs Blueprint-generated subclasses.
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UWidget* BuildDocumentRoot(UWidget* Content);
    // Walks the built tree. Used only for the two logged counts, so its cost lands on two frames
    // out of six hundred rather than on every one.
    static int32 CountWidgets(UWidget* Root);
    UWidget* BuildErrorRoot();

    UnRealDashCore::FDashPackage Package;
    UnRealDashCore::FDashTheme Theme;
    UnRealDashCore::FDashLoadError Error;
    UnRealDashCore::EDashProfile Profile = UnRealDashCore::EDashProfile::Desktop;
    UnRealDashCore::EDashSignalState State = UnRealDashCore::EDashSignalState::Valid;
    float Fraction = 0.f;
    UnRealDashCore::FWidgetTreeBuilder Builder;
    UnRealDashCore::FDashBindingTable Bindings;
    // PLAN 4.8's raw event log. Opened by the HUD, ticked here because this is what already has a
    // per-frame callback, and closed by the HUD on exit.
    UnRealDashCore::FDashEventLog EventLog;
    // Reused every frame so the present row costs no allocation on the frame path.
    TArray<UnRealDashCore::FDashRenderedSignal> Rendered;
    // Filled before the tree is built when a connector numbers signals its own way; empty for the
    // scenario source, which numbers them by document position.
    TMap<FString, uint32> SignalIds;
    TUniquePtr<UnRealDashCore::FDashAcquisition> Acquisition;
    UnRealDashCore::FFrameSnapshot Snapshot;
    uint64 Ticks = 0;
    float HealthSeconds = 0.f, ElapsedSeconds = 0.f;
    // Logged at the first tick and again at the six hundredth. An updater sets properties and never
    // adds a child, and this is what stands behind that rather than the code reading as though it
    // does. A capture cannot measure it: a screenshot records pixels, not a widget tree.
    int32 FirstTickWidgets = 0;
    bool Accepted = false;
};
UCLASS()
class ADashPackageHUD : public AHUD
{
    GENERATED_BODY()
    // Gate-only; see the notes on the definitions. Neither is part of the PLAN 4.7 runtime flag
    // surface, and both exist because a gate has to be runnable without a person at the keyboard.
    void RunBatch(const FString& Directory);
    void ScheduleShot(float DelaySeconds);
    void TickShot();
public:
    virtual void BeginPlay() override;
    // The event log's last flush and its counts. A gate reads the counts out of the player log
    // rather than recomputing them, so the two cannot disagree about what was written.
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UDashPackageScreen> Screen;
    // GFrameCounter when the log opened, so the run's frame count is the engine's own and not the
    // log's count of what it managed to write.
    uint64 FirstFrameCounter = 0;
    FString ShotPath;
    FTimerHandle ShotTimer;
    // Frames to let the scene settle before capturing. The capture conditions switch off temporal
    // antialiasing, bloom, auto exposure and motion blur, so nothing here is converging over time;
    // the wait is for the widget tree to have been laid out and drawn at least once.
    int32 ShotFramesRemaining = 0;
    // Seconds to wait before capturing, so a gate can take two shots at two points of a scenario
    // and compare them. Zero keeps the original behaviour of capturing as soon as the tree has been
    // laid out and drawn.
    float ShotDelaySeconds = 0.f;
};
UCLASS()
class ADashPackageGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADashPackageGameMode();
};
