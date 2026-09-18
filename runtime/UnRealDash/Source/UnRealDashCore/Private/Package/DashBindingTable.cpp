#include "UnRealDashCore/DashBindingTable.h"

namespace UnRealDashCore
{
EDashSignalState StateFromSample(const FSignalSample& Sample)
{
    switch (Sample.Quality)
    {
    case ESignalQuality::Stale: return EDashSignalState::Stale;
    case ESignalQuality::Invalid: return EDashSignalState::Invalid;
    case ESignalQuality::Unavailable: return EDashSignalState::Unavailable;
    default: break;
    }
    // A valid reading whose age is not evidence of freshness. This is the case PLAN 4.5 named
    // specifically: a signal fed by a held definition-pack field carries a real measurement whose
    // age is unknown, so it is neither valid nor stale, and chunk 11 renders it as structurally
    // distinct from both.
    return Sample.AgeEvidence == ESignalAge::Unknown ? EDashSignalState::AgeUnknown : EDashSignalState::Valid;
}

bool FDashBindingTable::Build(const FDashPackage& Package, const TMap<FString, FComponentUpdater>& Updaters,
    FDashLoadError& OutError)
{
    Entries.Reset();
    Names.Reset();
    Ranges.Reset();

    // Document order is signal id order; see the note on the class.
    const FDashValue Declared = Package.Signals().Member(TEXT("signals"));
    for (int32 Index = 0; Index < Declared.Num(); ++Index)
    {
        FString Name;
        Declared.Element(Index).Member(TEXT("id")).String(Name);
        Names.Add(Name);
        // A signal nothing gauges still needs a range to sweep, and 0 to 1 is the honest default:
        // it is what a fraction means when nothing says otherwise.
        Ranges.Add(FVector2D(0.0, 1.0));
    }
    TArray<bool> RangeSeen;
    RangeSeen.Init(false, Ranges.Num());

    const FDashValue Bindings = Package.Bindings();
    for (int32 Index = 0; Index < Bindings.Num(); ++Index)
    {
        const FString ComponentId = Bindings.Key(Index);
        FString SignalName;
        Bindings.Member(ComponentId).Member(TEXT("signal")).String(SignalName);

        const int32 Signal = Names.IndexOfByKey(SignalName);
        if (Signal == INDEX_NONE)
        {
            OutError = { TEXT("E_UNRESOLVED_SIGNAL"), 9,
                FString::Printf(TEXT("/dashboard/bindings/%s/signal"), *ComponentId),
                FString::Printf(TEXT("Binding names a signal the document does not declare: %s"), *SignalName),
                Package.Path() };
            return false;
        }
        // A binding on a component that produced no updater is not an error. container, shape and
        // page_switch have nothing to show when a value changes, and a document may bind one
        // without meaning anything by it.
        if (const FComponentUpdater* Updater = Updaters.Find(ComponentId))
            Entries.Add({static_cast<uint32>(Signal), *Updater, ComponentId});

        // Union the declared ranges of every gauge bound to this signal. A readout or an image
        // declares none and contributes nothing, which is why the default survives for a signal
        // that only feeds text.
        for (const auto& Node : Package.Components())
        {
            if (Node.Id != ComponentId) continue;
            double Minimum = 0, Maximum = 0;
            if (Node.Properties.Member(TEXT("minimum")).Number(Minimum) &&
                Node.Properties.Member(TEXT("maximum")).Number(Maximum) && Maximum > Minimum)
            {
                if (!RangeSeen[Signal]) { Ranges[Signal] = FVector2D(Minimum, Maximum); RangeSeen[Signal] = true; }
                else
                {
                    Ranges[Signal].X = FMath::Min(Ranges[Signal].X, Minimum);
                    Ranges[Signal].Y = FMath::Max(Ranges[Signal].Y, Maximum);
                }
            }
            break;
        }
    }
    OutError = {};
    return true;
}

void FDashBindingTable::Apply(const FFrameSnapshot& Snapshot) const
{
    for (const FEntry& Entry : Entries)
    {
        // SignalIds is aligned with Samples, one entry per registry slot. Present is a compacted
        // list of what arrived this publication and cannot be indexed alongside Samples, which is
        // why the snapshot carries both.
        const int32 Slot = Snapshot.SignalIds.IndexOfByKey(Entry.Signal);
        FDashSignalValue Reading;
        if (Slot != INDEX_NONE && Snapshot.Samples.IsValidIndex(Slot))
        {
            const FSignalSample& Sample = Snapshot.Samples[Slot];
            Reading.State = StateFromSample(Sample);
            // A sample the registry has never received reads as Unavailable and carries a NaN, so
            // a value is offered only when there is one. A primitive told bHasValue false rests at
            // its minimum rather than drawing a number nobody measured.
            Reading.bHasValue = Reading.State != EDashSignalState::Unavailable && FMath::IsFinite(Sample.Value);
            Reading.Value = Reading.bHasValue ? Sample.Value : 0.0;
        }
        Entry.Updater(Reading);
    }
}
}
