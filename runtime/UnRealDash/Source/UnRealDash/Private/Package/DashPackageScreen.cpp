#include "DashPackageScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnRealDashLog.h"

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
    Screen->Open(Path, UnRealDashCore::DefaultDashProfile());
    Screen->AddToViewport();
    // One machine-readable verdict per run. The device half of the PLAN 4.4 gate is driven by
    // launching once per fixture and reading logcat, and an accepted case otherwise produces no
    // output at all, which would make "it worked" indistinguishable from "it died early".
    UE_LOG(LogUnRealDash, Display, TEXT("DashVerdict accepted=%s code=%s pointer=%s path=%s"),
        Screen->WasAccepted() ? TEXT("true") : TEXT("false"),
        *Screen->LastError().CodeName, *Screen->LastError().Pointer, *Path);
}
ADashPackageGameMode::ADashPackageGameMode()
{
    HUDClass = ADashPackageHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
