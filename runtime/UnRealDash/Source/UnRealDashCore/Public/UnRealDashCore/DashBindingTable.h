#pragma once
#include "CoreMinimal.h"
#include "UnRealDashCore/ComponentRegistry.h"
#include "UnRealDashCore/DashAcquisition.h"

namespace UnRealDashCore
{
// Joins the document's bindings to the widgets they drive, once, at load.
//
// A binding names a signal by string and the registry works in numbers. The join is done here and
// kept, rather than per frame: a string comparison against every declared signal, for every
// binding, every frame, is the kind of cost that is invisible on a workstation and shows up as a
// dropped frame on a head unit.
//
// The numeric id of a signal is its index in the document's signals array. That is not a convention
// this class invents and hopes others follow: the same array builds the recording the acquisition
// pipeline runs, so the two agree by construction rather than by agreement.
class UNREALDASHCORE_API FDashBindingTable
{
public:
    // Resolves every binding against the package's signals and the tree's updaters. A binding whose
    // signal the document does not declare is an error naming the binding's JSON pointer, rather
    // than a gauge that silently never moves.
    //
    // Unreachable through LoadPackage, because the semantic pass already rejects an unresolved
    // binding signal with E_UNRESOLVED_SIGNAL. Kept for the same reason chunks 11 and 12 kept their
    // unreachable errors: this is callable on a document the validator never saw.
    // NameToId says what numeric id each declared signal has. The two connectors answer that
    // differently and neither answer belongs here: the scenario source numbers signals by their
    // position in the document, because it writes the recording from the same array, while a
    // binary telemetry stream numbers them from the definition pack's fields. Passing it in keeps
    // the join in one place while letting the source own the numbering it actually uses.
    //
    // An empty map means the default, position in the document's signals array.
    bool Build(const FDashPackage& Package, const TMap<FString, FComponentUpdater>& Updaters,
        const TMap<FString, uint32>& NameToId, FDashLoadError& OutError);

    // Applies one snapshot to every bound widget. The caller acquires the snapshot once and passes
    // it here; acquiring per binding could straddle a publication and show two components values
    // from different ones, which is what the triple buffer exists to prevent.
    void Apply(const FFrameSnapshot& Snapshot) const;

    // Names in document order, which is also signal id order. The recording generator walks this so
    // the ids it writes are the ids this resolved against.
    const TArray<FString>& SignalNames() const { return Names; }
    // The range each signal has to sweep to move the gauges bound to it, taken from those gauges'
    // own declared minimum and maximum and unioned where several disagree. Index-aligned with
    // SignalNames.
    //
    // Without this the value source has no idea what numbers mean anything. A sweep over 0 to 1
    // against a dial declared 0 to 100 moves the needle two tenths of one percent, which measures
    // as a two pixel shift and reads as a binding that does not work.
    const TArray<FVector2D>& SignalRanges() const { return Ranges; }
    int32 Num() const { return Entries.Num(); }

private:
    struct FEntry
    {
        uint32 Signal = 0;
        FComponentUpdater Updater;
        // Kept so a later report can say which component a binding drives without re-reading the
        // document.
        FString ComponentId;
    };
    TArray<FEntry> Entries;
    TArray<FString> Names;
    TArray<FVector2D> Ranges;
};

// The four missing-data states plus Valid, from the quality and age a sample carries. Chunk 11's
// four presentations are defined entirely by this mapping, so it lives in one place rather than
// being re-derived by each primitive.
UNREALDASHCORE_API EDashSignalState StateFromSample(const FSignalSample& Sample);
}
