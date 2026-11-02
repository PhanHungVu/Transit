#include "TransitKioskCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Components/BorderSlot.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "InputMappingContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TransitKioskActor.h"
#include "TransitKioskWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Components/ComboBoxString.h"
#include "Components/Throbber.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Serialization/BufferArchive.h"
#include "TimerManager.h"
#include "Misc/AutomationTest.h"
#include "Interfaces/ISlateRHIRendererModule.h"

UTransitKioskCommandlet::UTransitKioskCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

namespace
{
const FLinearColor LightBlue(0.558340f, 0.760525f, 0.871367f);
const FLinearColor DarkBlue(0.074214f, 0.226966f, 0.337164f);
const FLinearColor Ink(0.017642f, 0.048172f, 0.080220f);
const FLinearColor Green(0.025f, 0.32f, 0.12f);
bool SaveAsset(UBlueprint* BP);
const TArray<FName> KioskHeaderNames = {
    TEXT("Border_Header"), TEXT("Border_LanguageHeader"), TEXT("Border_MainMenuHeader"), TEXT("Border_MainMenuHeader_1"),
    TEXT("Border_RouteHeader"), TEXT("Border_ReviewHeader"), TEXT("Border_TopUpSelectionHeader"),
    TEXT("Border_PaymentHeader"), TEXT("Border_ProcessingHeader"), TEXT("Border_SuccessHeader")};

bool NormalizeKioskHeaders(UWidgetTree* Tree)
{
    auto* Pages = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("WS_Pages")));
    auto* Reference = Cast<UBorder>(Tree->FindWidget(KioskHeaderNames[0]));
    auto* ReferenceText = Reference ? Cast<UTextBlock>(Reference->GetContent()) : nullptr;
    auto* ReferencePage = Cast<UBorder>(Tree->FindWidget(TEXT("Page_Welcome")));
    if (!Pages || Pages->GetChildrenCount() != KioskHeaderNames.Num() || !ReferenceText || !ReferencePage) return false;
    // Validate every existing page before editing. Keep the headers and page contents editable,
    // and reuse Welcome's dimensions instead of adding a second header over the original ones.
    for (int32 Index = 0; Index < KioskHeaderNames.Num(); ++Index)
    {
        auto* Header = Cast<UBorder>(Tree->FindWidget(KioskHeaderNames[Index]));
        auto* Page = Cast<UBorder>(Pages->GetChildAt(Index));
        if (!Header || !Cast<UTextBlock>(Header->GetContent()) || !Cast<UVerticalBoxSlot>(Header->Slot) ||
            !Page || !Cast<UWidgetSwitcherSlot>(Page->Slot) || !Page->GetContent() || !Cast<UBorderSlot>(Page->GetContent()->Slot)) return false;
    }
    const FMargin HeaderPadding = Reference->GetPadding();
    const FSlateBrush HeaderBrush = Reference->Background;
    const FSlateFontInfo HeaderFont = ReferenceText->GetFont();
    const FMargin PagePadding = ReferencePage->GetPadding();
    for (int32 Index = 0; Index < KioskHeaderNames.Num(); ++Index)
    {
        auto* Page = CastChecked<UBorder>(Pages->GetChildAt(Index));
        auto* PageSlot = CastChecked<UWidgetSwitcherSlot>(Page->Slot);
        PageSlot->SetPadding(FMargin(0)); PageSlot->SetHorizontalAlignment(HAlign_Fill); PageSlot->SetVerticalAlignment(VAlign_Fill);
        Page->SetPadding(PagePadding); Page->SetHorizontalAlignment(HAlign_Fill); Page->SetVerticalAlignment(VAlign_Fill);
        auto* ContentSlot = CastChecked<UBorderSlot>(Page->GetContent()->Slot);
        ContentSlot->SetPadding(PagePadding); ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Header = CastChecked<UBorder>(Tree->FindWidget(KioskHeaderNames[Index]));
        auto* HeaderSlot = CastChecked<UVerticalBoxSlot>(Header->Slot);
        HeaderSlot->SetPadding(FMargin(0)); HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        HeaderSlot->SetHorizontalAlignment(HAlign_Fill); HeaderSlot->SetVerticalAlignment(VAlign_Fill);
        Header->SetBrush(HeaderBrush); Header->SetBrushColor(DarkBlue); Header->SetPadding(HeaderPadding);
        Header->SetHorizontalAlignment(HAlign_Fill); Header->SetVerticalAlignment(VAlign_Center); Header->SetDesiredSizeScale(FVector2D(1, 1));
        auto* Label = CastChecked<UTextBlock>(Header->GetContent());
        Label->SetText(FText::FromString(TEXT("PUBLIC TRANSIT TICKET KIOSK")));
        Label->SetFont(HeaderFont); Label->SetColorAndOpacity(FLinearColor::White);
        Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(false); Label->SetWrapTextAt(0);
    }
    return true;
}

bool FixKioskHeaders()
{
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    if (!BP || !BP->WidgetTree) return false;
    const FString Source = FPaths::ProjectContentDir() / TEXT("Kiosk/UI/KioskUI.uasset");
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("KioskFixBackup") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) / TEXT("KioskUI.uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return false;
    BP->Modify();
    if (!NormalizeKioskHeaders(BP->WidgetTree)) return false;
    const bool bSaved = SaveAsset(BP);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_HEADERS %s"), bSaved ? TEXT("SUCCESS") : TEXT("FAILED"));
    return bSaved;
}

struct FKioskDesigner
{
    UWidgetTree* Tree;
    FSlateFontInfo Font;
    explicit FKioskDesigner(UWidgetTree* InTree) : Tree(InTree)
    {
        Font = CastChecked<UTextBlock>(Tree->FindWidget(TEXT("TXT_Header")))->GetFont();
    }
    template<class T> T* Make(const FString& Name)
    {
        T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), FName(*Name));
        if (auto* BP = Cast<UWidgetBlueprint>(Tree->GetOuter())) BP->WidgetVariableNameToGuidMap.FindOrAdd(Widget->GetFName()) = FGuid::NewGuid();
        return Widget;
    }
    UTextBlock* Text(const FString& Name, const FString& Value, float Size = 24.f, FLinearColor Color = Ink)
    {
        auto* Result = Make<UTextBlock>(Name);
        auto TextFont = Font; TextFont.Size = Size;
        Result->SetFont(TextFont); Result->SetText(FText::FromString(Value)); Result->SetColorAndOpacity(Color);
        Result->SetJustification(ETextJustify::Center); Result->SetAutoWrapText(true);
        return Result;
    }
    void Space(UVerticalBox* Box, float Height, const FString& Name)
    {
        auto* Spacer = Make<USpacer>(Name); Spacer->SetSize(FVector2D(1.f, Height)); Box->AddChildToVerticalBox(Spacer);
    }
    UButton* Button(const FString& Name, const FString& Label, FLinearColor Color = Green, FLinearColor TextColor = FLinearColor::White)
    {
        auto* Result = Make<UButton>(Name);
        FButtonStyle Style = CastChecked<UButton>(Tree->FindWidget(TEXT("BTN_RouteContinue")))->GetStyle();
        for (auto* Brush : {&Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled})
        { Brush->SetResourceObject(nullptr); Brush->DrawAs = ESlateBrushDrawType::RoundedBox; Brush->OutlineSettings.CornerRadii = FVector4(8,8,8,8); }
        Style.Normal.TintColor = Color; Style.Hovered.TintColor = Color * 0.85f; Style.Pressed.TintColor = Color * 0.7f;
        Style.Disabled.TintColor = FLinearColor(0.35f, 0.4f, 0.4f);
        Style.NormalPadding = FMargin(12, 8); Style.PressedPadding = FMargin(12, 9, 12, 7);
        Result->SetStyle(Style); Result->SetBackgroundColor(FLinearColor::White);
        auto* LabelWidget = Text(Name.Replace(TEXT("BTN_"), TEXT("TXT_")), Label, 24.f, TextColor);
        LabelWidget->SetAutoWrapText(false);
        Result->AddChild(LabelWidget);
        return Result;
    }
    USizeBox* Sized(UWidget* Child, float Width, float Height, const FString& Name)
    {
        auto* Box = Make<USizeBox>(Name); Box->SetWidthOverride(Width); Box->SetHeightOverride(Height); Box->AddChild(Child); return Box;
    }
    void Center(UVerticalBox* Parent, UWidget* Child, bool bFill = false)
    {
        auto* Slot = Parent->AddChildToVerticalBox(Child); Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
        if (bFill) Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    UVerticalBox* Page(UWidgetSwitcher* Pages, const FString& Id, const FString& Title, const FString& Subtitle)
    {
        auto* Background = Make<UBorder>(TEXT("Page_") + Id); Background->SetBrushColor(LightBlue); Background->SetPadding(FMargin(0));
        auto* Box = Make<UVerticalBox>(TEXT("VB_") + Id); Background->AddChild(Box); Pages->AddChild(Background);
        auto* Header = Make<UBorder>(TEXT("Border_") + Id + TEXT("Header")); Header->SetBrushColor(DarkBlue); Header->SetPadding(FMargin(20, 22));
        Header->AddChild(Text(TEXT("TXT_") + Id + TEXT("Header"), TEXT("PUBLIC TRANSIT TICKET KIOSK"), 30.f, FLinearColor::White));
        Box->AddChildToVerticalBox(Header);
        Space(Box, 22, Id + TEXT("TitleSpace"));
        Center(Box, Text(TEXT("TXT_") + Id + TEXT("Title"), Title, 44.f));
        Center(Box, Text(TEXT("TXT_") + Id + TEXT("Description"), Subtitle, 24.f));
        Space(Box, 16, Id + TEXT("ContentSpace"));
        return Box;
    }
    void Footer(UVerticalBox* Box, const FString& Id, const FString& Label = TEXT("Back"))
    {
        Center(Box, Sized(Button(TEXT("BTN_") + Id, Label), 280, 60, TEXT("SB_") + Id));
        Space(Box, 90, Id + TEXT("FooterSpace"));
    }
};

bool SaveAsset(UBlueprint* BP)
{
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) return false;
    const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Args);
}

void StyleKioskDropdowns(UWidgetTree* Tree)
{
    auto Solid = [](FSlateBrush& Brush, FLinearColor Color)
    {
        Brush.SetResourceObject(nullptr);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.TintColor = Color;
    };
    for (FName Name : {FName(TEXT("CB_Origin")), FName(TEXT("CB_Destination"))})
        if (auto* Combo = Cast<UComboBoxString>(Tree->FindWidget(Name)))
        {
            auto Style = Combo->GetWidgetStyle();
            auto& Button = Style.ComboButtonStyle.ButtonStyle;
            // Keep the closed selector visibly distinct from the white route page.
            Solid(Button.Normal, FLinearColor(0.28f, 0.53f, 0.76f));
            Solid(Button.Hovered, FLinearColor(0.43f, 0.68f, 0.89f));
            Solid(Button.Pressed, FLinearColor(0.24f, 0.48f, 0.70f));
            for (auto* Brush : {&Button.Normal, &Button.Hovered, &Button.Pressed})
            {
                Brush->DrawAs = ESlateBrushDrawType::RoundedBox;
                Brush->OutlineSettings = FSlateBrushOutlineSettings(6.f, FSlateColor(DarkBlue), 2.f);
            }
            Button.NormalPadding = FMargin(2);
            Button.PressedPadding = FMargin(2, 3, 2, 1);
            Button.NormalForeground = Ink; Button.HoveredForeground = Ink; Button.PressedForeground = Ink;
            Style.ComboButtonStyle.DownArrowImage.SetImageSize(FVector2D(20, 14));
            Style.ComboButtonStyle.DownArrowImage.TintColor = FLinearColor::White;
            Style.ComboButtonStyle.DownArrowPadding = FMargin(14, 0, 4, 0);
            Style.ComboButtonStyle.DownArrowAlign = VAlign_Center;
            Solid(Style.ComboButtonStyle.MenuBorderBrush, FLinearColor::White);
            Combo->SetWidgetStyle(Style);
            Combo->SetContentPadding(FMargin(14, 8));
            Combo->SetHasDownArrow(true);
            auto Rows = Combo->GetItemStyle();
            Solid(Rows.EvenRowBackgroundBrush, FLinearColor::White);
            Solid(Rows.OddRowBackgroundBrush, FLinearColor::White);
            for (auto* Brush : {&Rows.EvenRowBackgroundHoveredBrush, &Rows.OddRowBackgroundHoveredBrush,
                &Rows.ActiveBrush, &Rows.ActiveHoveredBrush, &Rows.InactiveBrush, &Rows.InactiveHoveredBrush,
                &Rows.ActiveHighlightedBrush, &Rows.InactiveHighlightedBrush})
                Solid(*Brush, FLinearColor(0.65f, 0.82f, 0.96f));
            Rows.TextColor = Ink; Rows.SelectedTextColor = Ink;
            Combo->SetItemStyle(Rows);
            if (auto* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("ForegroundColor")))
                *Property->ContainerPtrToValuePtr<FSlateColor>(Combo) = FSlateColor(Ink);
        }
}

void FixKioskReadability(UWidgetTree* Tree)
{
    StyleKioskDropdowns(Tree);
    for (FName Name : {FName(TEXT("TXT_PurchaseTitle")), FName(TEXT("TXT_TopUpTitle"))})
        if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(Name)))
        {
            auto Font = Label->GetFont(); Font.Size = 26; Label->SetFont(Font);
            Label->SetWrapTextAt(280.f); Label->SetAutoWrapText(true); Label->SetJustification(ETextJustify::Center);
        }
}

bool FixReportedIssues(bool bDropdownsOnly = false)
{
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    if (!BP || !BP->WidgetTree) return false;
    for (FName Name : {FName(TEXT("CB_Origin")), FName(TEXT("CB_Destination")), FName(TEXT("TXT_PurchaseTitle")), FName(TEXT("TXT_TopUpTitle"))})
        if (!BP->WidgetTree->FindWidget(Name)) return false;
    const FString Source = FPaths::ProjectContentDir() / TEXT("Kiosk/UI/KioskUI.uasset");
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("KioskFixBackup") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) / TEXT("KioskUI.uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return false;
    if (bDropdownsOnly) StyleKioskDropdowns(BP->WidgetTree);
    else FixKioskReadability(BP->WidgetTree);
    BP->Modify();
    const bool bSaved = SaveAsset(BP);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_READABILITY_FIX %s"), bSaved ? TEXT("SUCCESS") : TEXT("FAILED"));
    return bSaved;
}

bool ExtendKiosk()
{
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    auto* ActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
    if (!BP || !ActorBP || !BP->WidgetTree) return false;
    auto* Tree = BP->WidgetTree.Get();
    auto* Pages = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("WS_Pages")));
    auto* Canvas = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("CanvasPanel_65")));
    if (!Pages || !Canvas || (Pages->GetChildrenCount() != 6 && Pages->GetChildrenCount() != 10)) return false;
    // Never overwrite the first backup. Assets are only serialized by Unreal's package API.
    for (UBlueprint* Asset : {static_cast<UBlueprint*>(BP), ActorBP})
    {
        const FString Relative = Asset == BP ? TEXT("Content/Kiosk/UI/KioskUI.uasset") : TEXT("Content/Kiosk/Blueprint/BP_TransitKiosk.uasset");
        const FString Backup = FPaths::ProjectSavedDir() / TEXT("KioskBackup") / Relative;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *(FPaths::ProjectDir() / Relative)) != COPY_OK) return false;
    }
    FKioskDesigner D(Tree);
    if (Pages->GetChildrenCount() == 6)
    {
        auto* TopUp = D.Page(Pages, TEXT("TopUpSelection"), TEXT("Top Up Your Transit Card"), TEXT("Choose an amount to add to the demonstration card."));
        D.Center(TopUp, D.Text(TEXT("TXT_TopUpBalance"), TEXT("Demo card **** 0421  |  Current balance: SGD 7.50")));
        auto* Amounts = D.Make<UHorizontalBox>(TEXT("HB_TopUpAmounts"));
        for (int32 Index = 0; Index < 3; ++Index)
        {
            auto* Slot = Amounts->AddChildToHorizontalBox(D.Sized(D.Button(FString::Printf(TEXT("BTN_TopUp%d"), Index), TEXT("SGD 5.00"), FLinearColor::White, Ink), 270, 100, FString::Printf(TEXT("SB_TopUp%d"), Index)));
            Slot->SetPadding(FMargin(12, 0));
        }
        D.Center(TopUp, Amounts, true); D.Footer(TopUp, TEXT("TopUpBack"));

        auto* Payment = D.Page(Pages, TEXT("Payment"), TEXT("Pay by Card"), TEXT("Use the button below to simulate tapping a payment card."));
        auto* SummaryCard = D.Make<UBorder>(TEXT("Border_PaymentSummary")); SummaryCard->SetBrushColor(FLinearColor::White); SummaryCard->SetPadding(FMargin(24));
        SummaryCard->AddChild(D.Text(TEXT("TXT_PaymentSummary"), TEXT("Transaction details"), 28));
        D.Center(Payment, D.Sized(SummaryCard, 820, 180, TEXT("SB_PaymentSummary")), true);
        D.Center(Payment, D.Sized(D.Button(TEXT("BTN_PayCard"), TEXT("Simulate Card Tap")), 360, 64, TEXT("SB_PayCard")));
        D.Space(Payment, 12, TEXT("PaymentBackSpace")); D.Footer(Payment, TEXT("PaymentBack"));

        auto* Processing = D.Page(Pages, TEXT("Processing"), TEXT("Processing Payment"), TEXT("Please wait while your demonstration transaction completes."));
        auto* Throbber = D.Make<UThrobber>(TEXT("TH_Processing")); D.Center(Processing, Throbber, true);
        D.Center(Processing, D.Text(TEXT("TXT_ProcessingNotice"), TEXT("No real payment is being made.")));
        D.Space(Processing, 150, TEXT("ProcessingFooterSpace"));

        auto* Success = D.Page(Pages, TEXT("Success"), TEXT("Your Ticket Is Ready!"), TEXT("Payment approved in demonstration mode."));
        D.Center(Success, D.Sized(D.Text(TEXT("TXT_SuccessSummary"), TEXT("Your confirmation will appear here."), 24), 1050, 260, TEXT("SB_SuccessSummary")), true);
        D.Footer(Success, TEXT("SuccessHome"), TEXT("Return to Welcome"));

        auto* Modal = D.Make<UBorder>(TEXT("StaffOverlay")); Modal->SetBrushColor(FLinearColor(0,0,0,0.7f)); Modal->SetHorizontalAlignment(HAlign_Center); Modal->SetVerticalAlignment(VAlign_Center);
        auto* ModalSlot = Canvas->AddChildToCanvas(Modal); ModalSlot->SetAnchors(FAnchors(0,0,1,1)); ModalSlot->SetOffsets(FMargin(0)); ModalSlot->SetZOrder(100);
        auto* Card = D.Make<UBorder>(TEXT("Border_StaffCard")); Card->SetBrushColor(LightBlue); Card->SetPadding(FMargin(30));
        Modal->AddChild(D.Sized(Card, 900, 460, TEXT("SB_StaffCard")));
        auto* Staff = D.Make<UVerticalBox>(TEXT("VB_Staff")); Card->AddChild(Staff);
        D.Center(Staff, D.Text(TEXT("TXT_StaffTitle"), TEXT("Staff Assistance"), 40));
        D.Center(Staff, D.Text(TEXT("TXT_StaffStatus"), TEXT("Request a simulated staff member. Your selections will be kept.")), true);
        D.Center(Staff, D.Sized(D.Button(TEXT("BTN_StaffRequest"), TEXT("Request Staff")), 320, 64, TEXT("SB_StaffRequest")));
        D.Space(Staff, 14, TEXT("StaffButtonSpace"));
        D.Center(Staff, D.Sized(D.Button(TEXT("BTN_StaffClose"), TEXT("Close")), 320, 60, TEXT("SB_StaffClose")));
        Modal->SetVisibility(ESlateVisibility::Collapsed);

        auto* Exit = D.Button(TEXT("BTN_ExitKiosk"), TEXT("Exit"));
        auto* ExitSlot = Canvas->AddChildToCanvas(Exit); ExitSlot->SetAnchors(FAnchors(1,0)); ExitSlot->SetAlignment(FVector2D(1,0)); ExitSlot->SetPosition(FVector2D(-24,12)); ExitSlot->SetSize(FVector2D(150,52)); ExitSlot->SetZOrder(10);
        auto* Language = D.Text(TEXT("TXT_LanguageStatus"), TEXT("Language: English"), 18);
        auto* LanguageSlot = Canvas->AddChildToCanvas(Language); LanguageSlot->SetAnchors(FAnchors(0,1)); LanguageSlot->SetAlignment(FVector2D(0,1)); LanguageSlot->SetPosition(FVector2D(24,-28)); LanguageSlot->SetSize(FVector2D(720,30)); LanguageSlot->SetZOrder(5);
        auto* Demo = D.Text(TEXT("TXT_DemoNotice"), TEXT("DEMONSTRATION ONLY - no real payments or tickets"), 16);
        auto* DemoSlot = Canvas->AddChildToCanvas(Demo); DemoSlot->SetAnchors(FAnchors(0.5f,1)); DemoSlot->SetAlignment(FVector2D(0.5f,1)); DemoSlot->SetPosition(FVector2D(0,-6)); DemoSlot->SetSize(FVector2D(800,24)); DemoSlot->SetZOrder(5);

        // Scale the complete original canvas as a unit; keep every existing page and child.
        auto* Stage = D.Make<USizeBox>(TEXT("SB_KioskDesignSize")); Stage->SetWidthOverride(1280); Stage->SetHeightOverride(720); Stage->AddChild(Canvas);
        auto* Scale = D.Make<UScaleBox>(TEXT("Scale_Kiosk")); Scale->SetStretch(EStretch::ScaleToFit); Scale->AddChild(Stage);
        Tree->RootWidget = Scale;
        for (FName Name : {FName(TEXT("CB_Origin")), FName(TEXT("CB_Destination"))})
            if (auto* Combo = Cast<UComboBoxString>(Tree->FindWidget(Name)))
                if (auto* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("Font")))
                { auto Font = D.Font; Font.Size = 24; *Property->ContainerPtrToValuePtr<FSlateFontInfo>(Combo) = Font; }
    }
    // Fit long labels without changing the existing page hierarchy or controls.
    for (FName Name : {FName(TEXT("TXT_OneWayTitle")), FName(TEXT("TXT_RoundTripTitle")), FName(TEXT("TXT_DayPassTitle"))})
        if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(Name))) { auto Font = Label->GetFont(); Font.Size = 26; Label->SetFont(Font); }
    for (FName Name : {FName(TEXT("TXT_PayCard")), FName(TEXT("TXT_SuccessHome")), FName(TEXT("TXT_StaffRequest")), FName(TEXT("TXT_StaffTitle")), FName(TEXT("TXT_TopUp0")), FName(TEXT("TXT_TopUp1")), FName(TEXT("TXT_TopUp2"))})
        if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(Name))) Label->SetAutoWrapText(false);
    for (FName Name : {FName(TEXT("BTN_PayCard")), FName(TEXT("BTN_SuccessHome")), FName(TEXT("BTN_StaffRequest")), FName(TEXT("BTN_TopUp0")), FName(TEXT("BTN_TopUp1")), FName(TEXT("BTN_TopUp2"))})
        if (auto* Button = Cast<UButton>(Tree->FindWidget(Name))) { auto Style = Button->GetStyle(); Style.NormalPadding = FMargin(12,8); Style.PressedPadding = FMargin(12,9,12,7); Button->SetStyle(Style); }
    if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(TEXT("TextBlock_491")))) Label->SetColorAndOpacity(Ink);
    if (auto* Help = Cast<UButton>(Tree->FindWidget(TEXT("BTN_Help"))))
    {
        auto Style = Help->GetStyle();
        for (auto* Brush : {&Style.Normal, &Style.Hovered, &Style.Pressed})
        { Brush->SetResourceObject(nullptr); Brush->DrawAs = ESlateBrushDrawType::RoundedBox; Brush->OutlineSettings.CornerRadii = FVector4(8,8,8,8); }
        Style.Normal.TintColor = FLinearColor(1.f, 0.8f, 0.05f);
        Style.Hovered.TintColor = FLinearColor(1.f, 0.9f, 0.15f);
        Style.Pressed.TintColor = FLinearColor(0.9f, 0.65f, 0.02f);
        Help->SetStyle(Style); Help->SetBackgroundColor(FLinearColor::White);
    }
    if (auto* Box = Cast<USizeBox>(Tree->FindWidget(TEXT("SB_SuccessHome")))) Box->SetWidthOverride(390);
    FixKioskReadability(Tree);
    if (!NormalizeKioskHeaders(Tree)) return false;
    BP->ParentClass = UTransitKioskWidget::StaticClass();
    ActorBP->ParentClass = ATransitKioskActor::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(BP); FBlueprintEditorUtils::RefreshAllNodes(ActorBP);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ActorBP);
    Pages->SetActiveWidgetIndex(0);
    const bool bResult = SaveAsset(BP) && SaveAsset(ActorBP);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_APPLY %s pages=%d"), bResult ? TEXT("SUCCESS") : TEXT("FAILED"), Pages->GetChildrenCount());
    return bResult;
}

bool VerifyKiosk(bool bRender)
{
    FString Report;
    int32 Failures = 0;
    auto Check = [&Report, &Failures](bool bPass, const FString& Name)
    {
        Report += (bPass ? TEXT("PASS ") : TEXT("FAIL ")) + Name + TEXT("\n");
        if (!bPass) ++Failures;
    };
#if WITH_DEV_AUTOMATION_TESTS
    FAutomationTestFramework::Get().StartTestByName(TEXT("FTransitKioskTransactionTest"), 0);
    FAutomationTestExecutionInfo TestInfo;
    Check(GIsAutomationTesting && FAutomationTestFramework::Get().StopTest(TestInfo), TEXT("Transaction state, fares, validation, duplicate payments, top-ups and midnight regression tests"));
#endif
    auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    auto* ActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
    if (!BP || !ActorBP) return false;
    FKismetEditorUtilities::CompileBlueprint(BP); FKismetEditorUtilities::CompileBlueprint(ActorBP);
    Check(BP->Status != BS_Error && ActorBP->Status != BS_Error, TEXT("Saved Widget Blueprint and kiosk Blueprint compile"));
    Check(BP->ParentClass == UTransitKioskWidget::StaticClass(), TEXT("Existing widget uses native behavior"));
    Check(ActorBP->ParentClass == ATransitKioskActor::StaticClass(), TEXT("Existing physical kiosk uses native actor"));
    const TArray<FString> PageNames = {TEXT("Page_Welcome"), TEXT("Page_Language"), TEXT("Page_MainMenu"), TEXT("Page_TicketType"), TEXT("Page_RouteSelection"),
        TEXT("Page_TransactionReview"), TEXT("Page_TopUpSelection"), TEXT("Page_Payment"), TEXT("Page_Processing"), TEXT("Page_Success")};
    auto* Pages = Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("WS_Pages")));
    Check(Pages && Pages->GetChildrenCount() == 10, TEXT("Ten editable UMG pages"));
    if (!Pages || Pages->GetChildrenCount() != 10) return false;
    for (int32 Index = 0; Index < 10; ++Index) Check(Pages->GetChildAt(Index)->GetName() == PageNames[Index], FString::Printf(TEXT("Page %d %s"), Index, *PageNames[Index]));
    TSet<FName> Components;
    for (auto* Node : ActorBP->SimpleConstructionScript->GetAllNodes()) Components.Add(Node->GetVariableName());
    for (FName Name : {FName(TEXT("MachineBase")), FName(TEXT("ScreenFrame")), FName(TEXT("ScreenDisplay")), FName(TEXT("PaymentReader")), FName(TEXT("HelpButton"))})
        Check(Components.Contains(Name), TEXT("Preserved physical component ") + Name.ToString());
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (!World) return false;
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto* Widget = CreateWidget<UTransitKioskWidget>(World, BP->GeneratedClass.Get());
    if (!Widget) return false;
    Widget->AddToRoot();
    Widget->InitializeKiosk(FTransitKioskConfig(), 750);
    auto SlateWidget = Widget->TakeWidget();
    auto* Session = Widget->GetSession();
    auto* RuntimePages = Cast<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("WS_Pages")));
    auto Click = [Widget, &Check](FName Name)
    {
        auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        Check(Button && Button->OnClicked.IsBound(), TEXT("Bound button ") + Name.ToString());
        if (Button && Button->GetIsEnabled()) Button->OnClicked.Broadcast();
    };
    auto PageIs = [Session, RuntimePages, &Check](EKioskPage Page, const FString& Step)
    {
        Check(Session->Page == Page && RuntimePages->GetActiveWidgetIndex() == static_cast<int32>(Page), Step);
    };
    auto TickTimers = [World](float Seconds)
    {
        ++GFrameCounter; World->GetTimerManager().Tick(0.001f);
        ++GFrameCounter; World->GetTimerManager().Tick(Seconds);
    };
    auto Reset = [Widget, Session]() { Session->Reset(); Widget->Refresh(); };
    for (FName Language : {FName(TEXT("BTN_LanguageEnglish")), FName(TEXT("BTN_LanguageEnglish_1")), FName(TEXT("BTN_LanguageEnglish_2")), FName(TEXT("BTN_LanguageEnglish_3"))})
    {
        Reset(); Click(TEXT("BTN_Start")); PageIs(EKioskPage::Language, TEXT("Welcome to Language"));
        Click(Language); PageIs(EKioskPage::MainMenu, TEXT("Language to Main Menu"));
        Click(TEXT("BTN_MainMenuBack")); PageIs(EKioskPage::Language, TEXT("Main Menu Back"));
        Click(TEXT("BTN_LanguageBack")); PageIs(EKioskPage::Welcome, TEXT("Language Back"));
    }
    for (FName Product : {FName(TEXT("BTN_OneWay")), FName(TEXT("BTN_RoundTrip")), FName(TEXT("BTN_DayPass"))})
    {
        Reset(); Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase"));
        PageIs(EKioskPage::TicketType, TEXT("Main Menu to Ticket Type"));
        Click(TEXT("BTN_MainMenuBack_1")); PageIs(EKioskPage::MainMenu, TEXT("Ticket Type Back"));
        Click(TEXT("BTN_Purchase")); Click(Product);
        if (Product != TEXT("BTN_DayPass"))
        {
            PageIs(EKioskPage::Route, TEXT("Ticket Type to Route"));
            Click(TEXT("BTN_RouteBack")); PageIs(EKioskPage::TicketType, TEXT("Route Back")); Click(Product);
            CastChecked<UComboBoxString>(Widget->GetWidgetFromName(TEXT("CB_Destination")))->SetSelectedIndex(3);
            Check(Session->Destination == 3, TEXT("UMG station dropdown updates transaction"));
            Click(TEXT("BTN_RouteContinue"));
        }
        PageIs(EKioskPage::Review, TEXT("Product to Review"));
        Click(TEXT("BTN_ReviewBack"));
        PageIs(Product == TEXT("BTN_DayPass") ? EKioskPage::TicketType : EKioskPage::Route, TEXT("Review Back returns correct page"));
        Click(Product == TEXT("BTN_DayPass") ? TEXT("BTN_DayPass") : TEXT("BTN_RouteContinue"));
        Click(TEXT("BTN_ReviewProceed")); PageIs(EKioskPage::Payment, TEXT("Review to Payment"));
        Click(TEXT("BTN_PaymentBack")); PageIs(EKioskPage::Review, TEXT("Payment Back")); Click(TEXT("BTN_ReviewProceed"));
        Click(TEXT("BTN_PayCard")); PageIs(EKioskPage::Processing, TEXT("Payment to Processing"));
        TickTimers(2.1f); PageIs(EKioskPage::Success, TEXT("Timer completes ticket payment"));
        Check(!Session->ReceiptId.IsEmpty(), TEXT("Confirmation receipt populated"));
        Click(TEXT("BTN_SuccessHome")); PageIs(EKioskPage::Welcome, TEXT("Success to Welcome"));
    }
    Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase"));
    Check(!CastChecked<UButton>(Widget->GetWidgetFromName(TEXT("BTN_DayPass")))->GetIsEnabled(), TEXT("Purchased day pass stays disabled after Return to Welcome"));
    Widget->HandleAction(TEXT("DayPass"));
    PageIs(EKioskPage::TicketType, TEXT("Direct day-pass action cannot start a duplicate purchase"));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Reset(); Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_TopUp"));
        PageIs(EKioskPage::TopUp, TEXT("Main Menu to Top-Up"));
        Click(TEXT("BTN_TopUpBack")); PageIs(EKioskPage::MainMenu, TEXT("Top-Up Back")); Click(TEXT("BTN_TopUp"));
        const FName Amount(*FString::Printf(TEXT("BTN_TopUp%d"), Index));
        Click(Amount); PageIs(EKioskPage::Review, TEXT("Amount to Review"));
        Click(TEXT("BTN_ReviewBack")); PageIs(EKioskPage::TopUp, TEXT("Top-Up review Back")); Click(Amount);
        const int32 OldBalance = Session->CardBalanceCents;
        Click(TEXT("BTN_ReviewProceed")); Click(TEXT("BTN_PayCard")); TickTimers(2.1f);
        PageIs(EKioskPage::Success, TEXT("Top-Up payment completes"));
        Check(Session->CardBalanceCents == OldBalance + Session->TopUpCents, TEXT("Top-Up credits balance"));
    }
    Reset(); Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase")); Click(TEXT("BTN_OneWay"));
    Click(TEXT("BTN_Help"));
    Check(Widget->GetWidgetFromName(TEXT("StaffOverlay"))->GetVisibility() == ESlateVisibility::Visible && !RuntimePages->GetIsEnabled(), TEXT("Staff popup blocks pages"));
    Click(TEXT("BTN_StaffRequest")); TickTimers(2.1f);
    Check(CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("TXT_StaffStatus")))->GetText().ToString().Contains(TEXT("ready to assist")), TEXT("Staff request timer confirms simulation"));
    Click(TEXT("BTN_StaffClose")); PageIs(EKioskPage::Route, TEXT("Closing Help preserves transaction and page"));
    Click(TEXT("BTN_Help"));
    auto* Request = CastChecked<UButton>(Widget->GetWidgetFromName(TEXT("BTN_StaffRequest")));
    auto* StaffStatus = CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("TXT_StaffStatus")));
    Check(Request->GetIsEnabled() && StaffStatus->GetText().ToString().Contains(TEXT("Need a hand?")), TEXT("Reopened Help resets a completed request"));
    Click(TEXT("BTN_StaffRequest")); Click(TEXT("BTN_StaffClose")); TickTimers(2.1f); Click(TEXT("BTN_Help"));
    Check(Request->GetIsEnabled() && StaffStatus->GetText().ToString().Contains(TEXT("Need a hand?")), TEXT("Closing a pending request cancels its timer and resets Help"));
    Click(TEXT("BTN_StaffRequest")); TickTimers(2.1f);
    Check(StaffStatus->GetText().ToString().Contains(TEXT("ready to assist")), TEXT("Second staff request completes without another transaction"));
    Click(TEXT("BTN_StaffClose"));
    Click(TEXT("BTN_RouteContinue")); Click(TEXT("BTN_ReviewProceed")); Click(TEXT("BTN_PayCard"));
    Widget->ShutdownKiosk(); TickTimers(2.1f);
    Check(Session->Page == EKioskPage::Processing, TEXT("Closing cancels pending payment timer"));

    if (bRender)
    {
        Widget->InitializeKiosk(FTransitKioskConfig(), 750);
        Session = Widget->GetSession(); Session->Start(); Session->SelectLanguage(TEXT("English")); Session->BeginPurchase(); Session->SelectProduct(EKioskProduct::DayPass);
        Session->ProceedToPayment(); Session->StartPayment(); Session->CompletePayment(FDateTime::Now());
        FWidgetRenderer Renderer(false);
        const FString Folder = FPaths::ProjectSavedDir() / TEXT("KioskPreview"); IFileManager::Get().MakeDirectory(*Folder, true);
        FVector2D ReferenceHeaderSize;
        for (int32 Width : {1280, 1920})
            for (int32 Index = 0; Index < 11; ++Index)
            {
                Session->Configure(FTransitKioskConfig(), 750);
                Session->Product = Index == 4 ? EKioskProduct::OneWay : EKioskProduct::DayPass;
                if (Index >= 9) { Session->Page = EKioskPage::Processing; Session->CompletePayment(FDateTime::Now()); }
                Session->Page = static_cast<EKioskPage>(FMath::Min(Index, 9)); Widget->Refresh();
                Widget->GetWidgetFromName(TEXT("StaffOverlay"))->SetVisibility(Index == 10 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
                ++GFrameCounter;
                FSlateApplication::Get().Tick();
                Widget->InvalidateLayoutAndVolatility(); Widget->ForceLayoutPrepass();
                const FVector2D Size(Width, Width * 9 / 16);
                auto* Target = Renderer.DrawWidget(SlateWidget, Size); FlushRenderingCommands();
                ++GFrameCounter; Widget->ForceLayoutPrepass();
                Renderer.DrawWidget(Target, SlateWidget, Size, 0.f); FlushRenderingCommands();
                if (Index < KioskHeaderNames.Num())
                {
                    // Check actual arranged bounds, not just Designer properties: a centered
                    // parent can still shrink or inset an otherwise correctly styled header.
                    const auto& CanvasGeometry = Widget->GetWidgetFromName(TEXT("CanvasPanel_65"))->GetCachedGeometry();
                    auto* Header = CastChecked<UBorder>(Widget->GetWidgetFromName(KioskHeaderNames[Index]));
                    const auto& Geometry = Header->GetCachedGeometry();
                    const FVector2D TopLeft = CanvasGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
                    const FVector2D BottomRight = CanvasGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
                    const FVector2D HeaderSize = BottomRight - TopLeft;
                    if (Index == 0) ReferenceHeaderSize = HeaderSize;
                    Check(TopLeft.Equals(FVector2D::ZeroVector, 0.1) && FMath::IsNearlyEqual(HeaderSize.X, 1280.0, 0.1) &&
                        HeaderSize.Equals(ReferenceHeaderSize, 0.1) && Header->GetBrushColor().Equals(DarkBlue),
                        FString::Printf(TEXT("Header page %d at %d matches Welcome: position=%s size=%s"), Index, Width, *TopLeft.ToString(), *HeaderSize.ToString()));
                }
                FBufferArchive Buffer;
                bool bSaved = Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, Buffer);
                const FString Filename = Folder / FString::Printf(TEXT("Kiosk_%02d_%d.png"), Index, Width);
                if (bSaved) bSaved = FFileHelper::SaveArrayToFile(Buffer, *Filename);
                Check(bSaved, TEXT("Render ") + Filename);
            }
        // Exercise expanded menus too: their row styles differ from the collapsed control.
        for (int32 Width : {1280, 1920})
        {
            Session->Configure(FTransitKioskConfig(), 750);
            Session->Product = EKioskProduct::OneWay; Session->Page = EKioskPage::Route; Widget->Refresh();
            Widget->GetWidgetFromName(TEXT("StaffOverlay"))->SetVisibility(ESlateVisibility::Collapsed);
            const FVector2D Size(Width, Width * 9 / 16);
            auto Window = SNew(SWindow).ClientSize(Size).SizingRule(ESizingRule::FixedSize)
                .SupportsMaximize(false).SupportsMinimize(false).CreateTitleBar(false);
            FSlateApplication::Get().AddWindow(Window, false);
            Window->SetContent(SlateWidget);
            auto* HostTarget = FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, false);
            ++GFrameCounter; Window->SlatePrepass();
            Renderer.DrawWindow(HostTarget, Window->GetHittestGrid(), Window, 1.f, Size, 0.f); FlushRenderingCommands();
            for (FName Name : {FName(TEXT("CB_Origin")), FName(TEXT("CB_Destination"))})
            {
                auto* Combo = CastChecked<UComboBoxString>(Widget->GetWidgetFromName(Name));
                auto SlateCombo = StaticCastSharedPtr<SComboBox<TSharedPtr<FString>>>(Combo->GetCachedWidget());
                SlateCombo->SetIsOpen(true, false);
                ++GFrameCounter; FSlateApplication::Get().Tick();
                auto PopupWindow = SlateCombo->GetMenuWindow();
                Check(Combo->IsOpen() && PopupWindow.IsValid(), TEXT("Expanded dropdown ") + Name.ToString());
                if (PopupWindow.IsValid())
                {
                    // Native popup windows start hidden in offscreen runs. Render their
                    // content on a virtual surface with enough room to generate list rows.
                    FVector2D PopupSize = PopupWindow->GetSizeInScreen();
                    PopupSize.Y = FMath::Max(PopupSize.Y, 220.0);
                    auto* Target = FWidgetRenderer::CreateTargetFor(PopupSize, TF_Bilinear, false);
                    for (int32 Frame = 0; Frame < 4; ++Frame)
                    {
                        ++GFrameCounter; FSlateApplication::Get().Tick();
                        PopupWindow->GetContent()->SlatePrepass();
                        Renderer.DrawWidget(Target, PopupWindow->GetContent(), PopupSize, 1.f / 60.f);
                        FlushRenderingCommands();
                    }
                    TArray<FColor> Pixels;
                    Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
                    TSet<uint32> Colors;
                    for (const FColor& Pixel : Pixels) Colors.Add(Pixel.DWColor());
                    Check(Colors.Num() > 10, TEXT("Expanded menu contains rendered row text ") + Name.ToString());
                    FBufferArchive Buffer;
                    const FString Filename = Folder / FString::Printf(TEXT("%s_Expanded_%d.png"), *Name.ToString(), Width);
                    Check(FImageUtils::ExportRenderTarget2DAsPNG(Target, Buffer) && FFileHelper::SaveArrayToFile(Buffer, *Filename), TEXT("Render expanded menu ") + Filename);
                }
                SlateCombo->SetIsOpen(false);
            }
            Window->SetContent(SNullWidget::NullWidget);
            FSlateApplication::Get().RequestDestroyWindow(Window);
        }
    }
    Widget->ShutdownKiosk(); Widget->RemoveFromRoot();
    Report += FString::Printf(TEXT("\nFailures: %d\n"), Failures);
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("KioskVerification.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_VERIFY %s failures=%d"), Failures ? TEXT("FAILED") : TEXT("SUCCESS"), Failures);
    return Failures == 0;
}

void DescribeWidget(UWidget* Widget, FString& Out, int32 Depth = 0)
{
    Out += FString::ChrN(Depth * 2, ' ') + Widget->GetName() + TEXT(" : ") + Widget->GetClass()->GetName();
    if (auto* Text = Cast<UTextBlock>(Widget))
        Out += FString::Printf(TEXT(" text=[%s] font=%.0f color=%s"), *Text->GetText().ToString(), Text->GetFont().Size, *Text->GetColorAndOpacity().GetSpecifiedColor().ToString());
    if (auto* Border = Cast<UBorder>(Widget)) Out += TEXT(" background=") + Border->GetBrushColor().ToString();
    if (auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot)) Out += FString::Printf(TEXT(" position=%s size=%s"), *Slot->GetPosition().ToString(), *Slot->GetSize().ToString());
    Out += TEXT("\n");
    if (auto* Panel = Cast<UPanelWidget>(Widget))
        for (auto* Child : Panel->GetAllChildren()) DescribeWidget(Child, Out, Depth + 1);
}
}

int32 UTransitKioskCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("NormalizeHeaders"))) return FixKioskHeaders() ? 0 : 1;
    if (FParse::Param(*Params, TEXT("RestyleDropdowns"))) return FixReportedIssues(true) ? 0 : 1;
    if (FParse::Param(*Params, TEXT("FixReportedIssues"))) return FixReportedIssues() ? 0 : 1;
    if (FParse::Param(*Params, TEXT("Apply"))) return ExtendKiosk() ? 0 : 1;
    if (FParse::Param(*Params, TEXT("Verify"))) return VerifyKiosk(FParse::Param(*Params, TEXT("Render"))) ? 0 : 1;
    FString Out;
    const TArray<FString> Paths = {
        TEXT("/Game/Kiosk/UI/KioskUI"), TEXT("/Game/Kiosk/UI/InteractPrompt"),
        TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"),
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"),
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"),
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode")};
    for (const auto& Path : Paths)
    {
        auto* BP = LoadObject<UBlueprint>(nullptr, *Path);
        if (!BP) return 1;
        Out += TEXT("\nASSET ") + Path + TEXT(" parent=") + GetPathNameSafe(BP->ParentClass) + TEXT("\n");
        if (auto* WBP = Cast<UWidgetBlueprint>(BP)) DescribeWidget(WBP->WidgetTree->RootWidget, Out);
        if (BP->SimpleConstructionScript)
            for (auto* Node : BP->SimpleConstructionScript->GetAllNodes())
                Out += TEXT("COMPONENT ") + Node->GetVariableName().ToString() + TEXT(" : ") + GetNameSafe(Node->ComponentClass) + TEXT("\n");
        TArray<UEdGraph*> Graphs;
        BP->GetAllGraphs(Graphs);
        for (auto* Graph : Graphs)
        {
            Out += TEXT("GRAPH ") + Graph->GetName() + TEXT("\n");
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                Out += TEXT(" NODE ") + Node->GetName() + TEXT(" ") + Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" ")) + TEXT("\n");
                for (auto* Pin : Node->Pins)
                {
                    Out += TEXT("  ") + Pin->PinName.ToString() + TEXT("=") + Pin->DefaultValue;
                    for (auto* Linked : Pin->LinkedTo) Out += TEXT(" -> ") + Linked->GetOwningNode()->GetName() + TEXT(".") + Linked->PinName.ToString();
                    Out += TEXT("\n");
                }
            }
        }
    }
    auto* IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default"));
    for (const auto& Mapping : IMC->GetMappings())
        Out += FString::Printf(TEXT("INPUT %s = %s\n"), *GetNameSafe(Mapping.Action), *Mapping.Key.ToString());
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (World)
        for (TActorIterator<AActor> It(World); It; ++It)
            Out += FString::Printf(TEXT("ACTOR %s %s at %s\n"), *It->GetActorLabel(), *It->GetClass()->GetPathName(), *It->GetActorLocation().ToString());
    FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("KioskInspection.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_INSPECTION_COMPLETE"));
    return 0;
}
