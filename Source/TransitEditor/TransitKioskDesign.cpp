#include "TransitKioskDesign.h"
#include "TransitKioskWidget.h"
#include "TransitKioskActor.h"
#include "TransitKioskText.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "EditorReimportHandler.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"

namespace
{
const FLinearColor Purple = FLinearColor::FromSRGBColor(FColor(81, 48, 218));
const FLinearColor Green = FLinearColor::FromSRGBColor(FColor(0, 240, 0));
const FLinearColor Black = FLinearColor::Black;
const FLinearColor White = FLinearColor::White;

bool SaveDesignObject(UObject *Object)
{
    Object->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
                                 *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                          FPackageName::GetAssetPackageExtension()),
                                 Args);
}
UTexture2D *ImportArt(const FString &Name)
{
    const FString Path = TEXT("/Game/Kiosk/Design/T_") + Name;
    if (auto *Existing = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        if (Name == TEXT("Help"))
        {
            const FString Source = FPaths::ProjectContentDir() / TEXT("Kiosk/DesignSource/Help.png");
            if (!FReimportManager::Instance()->Reimport(Existing, false, false, Source, nullptr, INDEX_NONE, true, true)
                || !SaveDesignObject(Existing)) return nullptr;
        }
        return Existing;
    }
    auto *Factory = NewObject<UTextureFactory>();
    Factory->SuppressImportOverwriteDialog();
    bool Cancelled = false;
    auto *Texture = Cast<UTexture2D>(Factory->FactoryCreateFile(
        UTexture2D::StaticClass(), CreatePackage(*Path), *FPaths::GetBaseFilename(Path), RF_Public | RF_Standalone,
        FPaths::ProjectContentDir() / TEXT("Kiosk/DesignSource") / (Name + TEXT(".png")), nullptr, GWarn, Cancelled));
    if (!Texture)
        return nullptr;
    Texture->LODGroup = TEXTUREGROUP_UI;
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->SRGB = true;
    Texture->NeverStream = true;
    Texture->PostEditChange();
    FAssetRegistryModule::AssetCreated(Texture);
    return SaveDesignObject(Texture) ? Texture : nullptr;
}
UFontFace *ImportFace(const FString &Name, const FString &Filename)
{
    const FString Path = TEXT("/Game/Kiosk/Fonts/") + Name;
    if (auto *Existing = LoadObject<UFontFace>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        return Existing;
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
        return nullptr;
    auto *Face = NewObject<UFontFace>(CreatePackage(*Path), *Name, RF_Public | RF_Standalone);
    Face->LoadingPolicy = EFontLoadingPolicy::Inline;
    Face->SourceFilename = Filename;
    Face->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
    Face->CacheSubFaces();
    FAssetRegistryModule::AssetCreated(Face);
    return SaveDesignObject(Face) ? Face : nullptr;
}
UFont *DesignFont()
{
    const FString Path = TEXT("/Game/Kiosk/Fonts/KioskMultilingual");
    if (auto *Existing = LoadObject<UFont>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        return Existing;
    auto *Latin = ImportFace(TEXT("KioskLatin"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"));
    auto *Bold = ImportFace(TEXT("KioskLatinBold"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"));
    auto *CJK = ImportFace(TEXT("KioskCJK"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"));
    auto *Tamil = LoadObject<UFontFace>(nullptr, TEXT("/Game/Kiosk/Fonts/NotoSansTamil-Regular"));
    if (!Latin || !Bold || !CJK || !Tamil)
        return nullptr;
    auto *Font = NewObject<UFont>(CreatePackage(*Path), TEXT("KioskMultilingual"), RF_Public | RF_Standalone);
    Font->FontCacheType = EFontCacheType::Runtime;
    auto Entry = [](UFontFace *Face)
    {
        FTypefaceEntry E(TEXT("Regular"));
        E.Font = FFontData(Face);
        return E;
    };
    auto &Composite = Font->GetMutableInternalCompositeFont();
    Composite.DefaultTypeface.Fonts.Add(Entry(Latin));
    auto BoldEntry = Entry(Bold);
    BoldEntry.Name = TEXT("Bold");
    Composite.DefaultTypeface.Fonts.Add(BoldEntry);
    FCompositeSubFont Chinese;
    Chinese.CharacterRanges.Add(FInt32Range(0x2E80, 0x9FFF));
    Chinese.CharacterRanges.Add(FInt32Range(0xFF00, 0xFFEF));
    Chinese.Typeface.Fonts.Add(Entry(CJK));
    Composite.SubTypefaces.Add(Chinese);
    FCompositeSubFont TamilSub;
    TamilSub.CharacterRanges.Add(FInt32Range(0x0B80, 0x0BFF));
    TamilSub.Typeface.Fonts.Add(Entry(Tamil));
    Composite.SubTypefaces.Add(TamilSub);
    Composite.FallbackTypeface.Typeface.Fonts.Add(Entry(CJK));
    FAssetRegistryModule::AssetCreated(Font);
    return SaveDesignObject(Font) ? Font : nullptr;
}
struct FDesigner
{
    UWidgetTree *Tree;
    UWidgetBlueprint *BP;
    UFont *Font;
    TMap<FString, UTexture2D *> Art;
    template <class T> T *Make(const FString &Name)
    {
        auto *W = Tree->ConstructWidget<T>(T::StaticClass(), *Name);
        BP->WidgetVariableNameToGuidMap.FindOrAdd(W->GetFName()) = FGuid::NewGuid();
        return W;
    }
    UCanvasPanelSlot *At(UCanvasPanel *Parent, UWidget *Child, FVector2D Pos, FVector2D Size, int32 Z = 0)
    {
        auto *Slot = Parent->AddChildToCanvas(Child);
        Slot->SetPosition(Pos);
        Slot->SetSize(Size);
        Slot->SetZOrder(Z);
        return Slot;
    }
    void Fill(UCanvasPanel *Parent, UWidget *Child, int32 Z = 0)
    {
        auto *Slot = Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(FAnchors(0, 0, 1, 1));
        Slot->SetOffsets(FMargin(0));
        Slot->SetZOrder(Z);
    }
    UTextBlock *Text(const FString &Name, const FString &Value, float Size = 30, FLinearColor Color = Black)
    {
        // PDF sizes describe a 2x pixel design surface; Slate font sizes are points,
        // and the composite font's script metrics are taller than Latin alone.
        auto *T = Make<UTextBlock>(Name);
        T->SetFont(FSlateFontInfo(Font, Size * .70f, Size >= 28 ? TEXT("Bold") : TEXT("Regular")));
        T->SetText(FText::FromString(Value));
        T->SetColorAndOpacity(Color);
        T->SetJustification(ETextJustify::Center);
        T->SetAutoWrapText(true);
        T->SetVisibility(ESlateVisibility::HitTestInvisible);
        return T;
    }
    UTextBlock *Local(const FString &Key, float Size = 30, FLinearColor Color = Black, const FString &Suffix = TEXT(""))
    {
        return Text(TEXT("LOC_") + Key + Suffix, KioskText(TEXT("English"), *Key), Size, Color);
    }
    UImage *Image(const FString &Name, const FString &ImageName)
    {
        auto *I = Make<UImage>(Name);
        I->SetBrushFromTexture(Art[ImageName]);
        I->SetVisibility(ESlateVisibility::HitTestInvisible);
        return I;
    }
    UBorder *Border(const FString &Name, FLinearColor Color, float Radius = 0)
    {
        auto *B = Make<UBorder>(Name);
        FSlateBrush Brush;
        Brush.DrawAs = Radius > 0 ? ESlateBrushDrawType::RoundedBox : ESlateBrushDrawType::Image;
        Brush.TintColor = White;
        Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, White, 0);
        B->SetBrush(Brush);
        B->SetBrushColor(Color);
        B->SetPadding(FMargin(0));
        return B;
    }
    UButton *Button(const FString &Name, FLinearColor Color = White, bool Transparent = false)
    {
        auto *B = Make<UButton>(Name);
        FButtonStyle Style;
        for (auto *Brush : {&Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled})
        {
            Brush->DrawAs = Transparent ? ESlateBrushDrawType::NoDrawType : ESlateBrushDrawType::RoundedBox;
            Brush->TintColor = Color;
            Brush->OutlineSettings = FSlateBrushOutlineSettings(8.f, Color, 0.f);
        }
        Style.Hovered.TintColor = Color * .85f;
        Style.Pressed.TintColor = Color * .65f;
        Style.Disabled.TintColor = FLinearColor(.55f, .55f, .55f, .35f);
        Style.NormalPadding = Style.PressedPadding = FMargin(0);
        B->SetStyle(Style);
        B->SetBackgroundColor(White);
        return B;
    }
    UButton *LabelButton(const FString &Name, const FString &Key, FLinearColor Color = Green, float Size = 26,
                         const FString &Suffix = TEXT(""))
    {
        auto *B = Button(Name, Color);
        B->AddChild(Local(Key, Size, Black, Suffix));
        return B;
    }
    UCanvasPanel *Page(UWidgetSwitcher *Pages, const FString &Name)
    {
        auto *P = Make<UCanvasPanel>(Name);
        Pages->AddChild(P);
        auto *Slot = CastChecked<UWidgetSwitcherSlot>(P->Slot);
        Slot->SetPadding(FMargin(0));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
        return P;
    }
    void Option(UCanvasPanel *Parent, const FString &Name, const FString &ArtName, const FString &Key, FVector2D Pos,
                const FString &Suffix)
    {
        auto *B = Button(Name, White, true);
        auto *C = Make<UCanvasPanel>(Name + TEXT("Content"));
        B->AddChild(C);
        Fill(C, Image(Name + TEXT("Image"), ArtName));
        At(C, Local(Key, 28, Black, Suffix), FVector2D(12, 418), FVector2D(326, 74));
        At(Parent, B, Pos, FVector2D(350, 494));
    }
    UCanvasPanel *Modal(UWidgetSwitcher *Popups, const FString &Name, FVector2D Size)
    {
        auto *PageCanvas = Page(Popups, Name);
        auto *Card = Border(Name + TEXT("Card"), White, 24);
        At(PageCanvas, Card, (FVector2D(960, 700) - Size) / 2, Size);
        auto *Content = Make<UCanvasPanel>(Name + TEXT("Content"));
        Card->AddChild(Content);
        return Content;
    }
};

bool EnsureCollectionWarning(UWidgetBlueprint* BP)
{
    if (BP->WidgetTree->FindWidget(TEXT("CollectionWarning"))) return true;
    auto* Canvas=Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("CanvasPanel_65")));
    auto* Font=DesignFont();
    if (!Canvas || !Font) return false;
    BP->Modify();
    FDesigner D{BP->WidgetTree,BP,Font,{}};
    auto* Layer=D.Make<UCanvasPanel>(TEXT("CollectionWarning"));
    D.Fill(Canvas,Layer,100);
    D.Fill(Layer,D.Border(TEXT("CollectionWarningDim"),FLinearColor(0,0,0,.65f)));
    auto* Card=D.Border(TEXT("CollectionWarningCard"),White,24);
    D.At(Layer,Card,{80,110},{800,480});
    auto* Contents=D.Make<UCanvasPanel>(TEXT("CollectionWarningContents"));
    Card->AddChild(Contents);
    D.At(Contents,D.Local(TEXT("CollectTitle"),34),{30,24},{740,88});
    D.At(Contents,D.Local(TEXT("CollectInstruction"),25),{30,125},{740,220});
    D.At(Contents,D.Local(TEXT("CollectHelp"),23,FLinearColor(.12f,.25f,.4f)),{30,370},{740,70});
    Layer->SetVisibility(ESlateVisibility::Collapsed);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status!=BS_Error && SaveDesignObject(BP);
}

bool EnsureQuantityPopup(UWidgetBlueprint* BP)
{
    if (BP->WidgetTree->FindWidget(TEXT("Popup_Quantity"))) return true;
    auto* Popups=Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("WS_Popups")));
    auto* Font=DesignFont();
    if (!Popups || Popups->GetChildrenCount()!=5 || !Font) return false;
    BP->Modify();
    FDesigner D{BP->WidgetTree,BP,Font,{}};
    auto* Quantity=D.Modal(Popups,TEXT("Popup_Quantity"),{700,520});
    D.At(Quantity,D.Local(TEXT("QuantityTitle"),36),{30,22},{640,64});
    D.At(Quantity,D.Local(TEXT("QuantityInstruction"),25),{30,88},{640,85});
    for (int32 I=0;I<2;++I)
    {
        auto* Button=D.Button(I ? TEXT("BTN_QuantityPlus") : TEXT("BTN_QuantityMinus"),FLinearColor(.94f,.92f,1));
        Button->AddChild(D.Text(I ? TEXT("TXT_QuantityPlus") : TEXT("TXT_QuantityMinus"),I ? TEXT("+") : TEXT("−"),64));
        D.At(Quantity,Button,{I ? 500. : 100.,185},{100,85});
    }
    D.At(Quantity,D.Text(TEXT("TXT_TicketQuantity"),TEXT("1"),72),{240,175},{220,100});
    D.At(Quantity,D.Text(TEXT("TXT_QuantitySummary"),TEXT(""),25),{30,300},{640,85});
    D.At(Quantity,D.LabelButton(TEXT("BTN_QuantityCancel"),TEXT("Cancel"),FLinearColor(.9f,.9f,.9f),25),{60,424},{260,58});
    D.At(Quantity,D.LabelButton(TEXT("BTN_QuantityConfirm"),TEXT("Continue"),Green,25),{380,424},{260,58});
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status!=BS_Error && SaveDesignObject(BP);
}

bool BuildDesignWidget(UWidgetBlueprint *BP, UFont *Font, TMap<FString, UTexture2D *> &Art)
{
    BP->Modify();
    auto *Tree = NewObject<UWidgetTree>(
        BP, MakeUniqueObjectName(BP, UWidgetTree::StaticClass(), TEXT("DesignWidgetTree")), RF_Transactional);
    BP->WidgetTree = Tree;
    BP->WidgetVariableNameToGuidMap.Reset();
    FDesigner D{Tree, BP, Font, Art};
    auto *Root = D.Make<UCanvasPanel>(TEXT("KioskRoot"));
    Tree->RootWidget = Root;
    auto *Scale = D.Make<UScaleBox>(TEXT("Scale_Kiosk"));
    Scale->SetStretch(EStretch::ScaleToFit);
    D.Fill(Root, Scale);
    auto *Size = D.Make<USizeBox>(TEXT("SB_KioskDesignSize"));
    Size->SetWidthOverride(960);
    Size->SetHeightOverride(700);
    Scale->AddChild(Size);
    auto *Canvas = D.Make<UCanvasPanel>(TEXT("CanvasPanel_65"));
    Canvas->SetClipping(EWidgetClipping::ClipToBounds);
    Size->AddChild(Canvas);
    D.Fill(Canvas, D.Image(TEXT("IMG_Train"), TEXT("Train")));
    D.Fill(Canvas, D.Border(TEXT("TrainWash"), FLinearColor(1, 1, 1, .72f)));
    auto *Pages = D.Make<UWidgetSwitcher>(TEXT("WS_Pages"));
    D.Fill(Canvas, Pages);
    auto *Welcome = D.Page(Pages, TEXT("Page_Welcome"));
    auto *Start = D.Button(TEXT("BTN_Start"), White, true);
    D.Fill(Welcome, Start);
    D.At(Welcome,
         D.Text(TEXT("TXT_Welcome"),
                TEXT("Welcome to Ticket Kiosk\n欢迎使用售票机\nSelamat datang ke kiosk tiket\nடிக்கெட் வழங்கும் "
                     "இயந்திரத்திற்கு\nவரவேற்கிறோம்"),
                31),
         {80, 165}, {800, 190});
    D.At(Welcome,
         D.Text(TEXT("TXT_Tap"),
                TEXT("Tap Screen to continue\n点击屏幕继续\nSentuh skrin untuk meneruskan\nதொடர திரையைத் தொடவும்"), 30),
         {80, 400}, {800, 160});
    auto *Language = D.Page(Pages, TEXT("Page_Language"));
    const FString Languages[] = {TEXT("English"), TEXT("Chinese"), TEXT("Malay"), TEXT("Tamil")};
    const FString Labels[] = {TEXT("English"), TEXT("中文"), TEXT("Melayu"), TEXT("தமிழ்")};
    for (int32 I = 0; I < 4; ++I)
    {
        auto *B = D.Button(TEXT("BTN_Language") + Languages[I], FLinearColor(1, 1, 1, .8f));
        B->AddChild(D.Text(TEXT("TXT_Language") + Languages[I], Labels[I], 32));
        D.At(Language, B, {60. + I % 2 * 460, 180. + I / 2 * 230}, {400, 200});
    }
    auto *Menu = D.Page(Pages, TEXT("Page_MainMenu"));
    D.Option(Menu, TEXT("BTN_Purchase"), TEXT("Purchase"), TEXT("Purchase"), {90, 150}, TEXT("_Service"));
    D.Option(Menu, TEXT("BTN_TopUp"), TEXT("TopUp"), TEXT("TopUp"), {520, 150}, TEXT("_Service"));
    auto *Tickets = D.Page(Pages, TEXT("Page_TicketType"));
    D.Option(Tickets, TEXT("BTN_Destination"), TEXT("Destination"), TEXT("Destination"), {90, 150}, TEXT("_Option"));
    D.Option(Tickets, TEXT("BTN_DayPass"), TEXT("DayPass"), TEXT("DayPass"), {520, 150}, TEXT("_Option"));
    // Reserve separate rows for the day-pass title and validity text inside its card.
    auto *DayTitleSlot = CastChecked<UCanvasPanelSlot>(Tree->FindWidget(TEXT("LOC_DayPass_Option"))->Slot);
    DayTitleSlot->SetPosition({12, 390});
    DayTitleSlot->SetSize({326, 50});
    D.At(Tickets, D.Text(TEXT("TXT_DayValidity"), TEXT(""), 18), {535, 596}, {320, 46});
    auto *Route = D.Page(Pages, TEXT("Page_RouteSelection"));
    auto *Map = D.Make<UCanvasPanel>(TEXT("MapCanvas"));
    D.At(Route, Map, {88, 150}, {768, 512});
    D.Fill(Map, D.Image(TEXT("IMG_Map"), TEXT("Map")));
    const FVector2D Origin(338, 209);
    const FTransitKioskConfig Config;
    const TArray<TPair<int32, int32>> Edges = {{-1, 0}, {-1, 1}, {-1, 2}, {2, 3}, {3, 4}, {2, 5}, {5, 6}, {-1, 7}};
    for (int32 I = 0; I < Edges.Num(); ++I)
    {
        auto Position = [&](int32 Index)
        { return Index < 0 ? Origin : Config.Stations[Index].MapPosition * FVector2D(768, 512); };
        const FVector2D A = Position(Edges[I].Key), B = Position(Edges[I].Value), Delta = B - A;
        auto *Line = D.Make<UImage>(FString::Printf(TEXT("RouteLine%d"), I));
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.TintColor = (I == 1 || I == 7)   ? FLinearColor::Blue
                          : (I == 5 || I == 6) ? FLinearColor(.45f, 0, 1)
                                               : FLinearColor::Red;
        Line->SetBrush(Brush);
        Line->SetVisibility(ESlateVisibility::HitTestInvisible);
        D.At(Map, Line, A - FVector2D(0, 3), {Delta.Size(), 6});
        Line->SetRenderTransformPivot({0, .5});
        Line->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
    }
    auto *Interchange = D.Border(TEXT("MapOrigin"), White, 14);
    auto Brush = Interchange->Background;
    Brush.OutlineSettings = FSlateBrushOutlineSettings(14.f, FSlateColor(Green), 5.f);
    Interchange->SetBrush(Brush);
    D.At(Map, Interchange, Origin - FVector2D(14, 14), {28, 28});
    for (int32 I = 0; I < 8; ++I)
    {
        auto *B = D.Button(FString::Printf(TEXT("BTN_Station%d"), I));
        auto Style = B->GetStyle();
        for (auto *BrushPart : {&Style.Normal, &Style.Hovered, &Style.Pressed})
            BrushPart->OutlineSettings = FSlateBrushOutlineSettings(16.f, FSlateColor(Black), 4.f);
        B->SetStyle(Style);
        B->SetToolTipText(FText::FromString(TEXT("Station ") + Config.Stations[I].Code));
        D.At(Map, B, Config.Stations[I].MapPosition * FVector2D(768, 512) - FVector2D(16, 16), {32, 32});
    }
    auto *StationLabel = D.Border(TEXT("StationLabel"), Black, 12);
    StationLabel->AddChild(D.Text(TEXT("TXT_StationName"), TEXT(""), 20, White));
    D.At(Map, StationLabel, {0, 0}, {180, 40}, 5);
    auto *Footer = D.Make<UCanvasPanel>(TEXT("MapSelection"));
    D.At(Route, Footer, {88, 590}, {768, 50}, 10);
    D.Fill(Footer, D.Border(TEXT("MapFooterBackground"), FLinearColor(1, 1, 1, .95f)));
    D.At(Footer, D.Text(TEXT("TXT_SelectedFare"), TEXT(""), 20), {8, 10}, {550, 40});
    D.At(Footer, D.LabelButton(TEXT("BTN_Confirm"), TEXT("Confirm"), Green, 22), {570, 8}, {185, 36});
    auto *CardPage = D.Page(Pages, TEXT("Page_CardPlacement"));
    auto *Card = D.Border(TEXT("CardPlacementCard"), FLinearColor(1, 1, 1, .9f), 24);
    D.At(CardPage, Card, {150, 265}, {660, 300});
    auto *Instructions = D.Make<UCanvasPanel>(TEXT("CardInstructions"));
    Card->AddChild(Instructions);
    D.At(Instructions, D.Local(TEXT("PlaceCard"), 30), {20, 45}, {620, 80});
    D.At(Instructions, D.Local(TEXT("ReaderBelow"), 24, FLinearColor(.2f, .2f, .2f)), {30, 125}, {600, 65});
    D.At(Instructions, D.Text(TEXT("TXT_DownArrow"), TEXT("↓"), 70, Purple), {280, 185}, {100, 100});
    auto *Amounts = D.Page(Pages, TEXT("Page_TopUpSelection"));
    D.At(Amounts, D.Text(TEXT("TXT_Balance"), TEXT(""), 28, Purple), {120, 118}, {720, 65});
    for (int32 I = 0; I < 6; ++I)
    {
        auto *B = D.Button(FString::Printf(TEXT("BTN_TopUp%d"), I), FLinearColor(1, 1, 1, .85f));
        B->AddChild(D.Text(FString::Printf(TEXT("TXT_TopUp%d"), I), TEXT(""), 30));
        D.At(Amounts, B, {90. + I % 2 * 460, 205. + I / 2 * 145}, {350, 110});
    }
    auto *Header = D.Make<UCanvasPanel>(TEXT("Header"));
    D.Fill(Canvas, Header, 5);
    Header->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    D.At(Header, D.LabelButton(TEXT("BTN_Back"), TEXT("Back"), Green, 22), {14, 38}, {150, 42});
    D.At(Header, D.Text(TEXT("TXT_Title"), TEXT(""), 32), {180, 40}, {720, 88});
    auto *Layer = D.Border(TEXT("PopupLayer"), FLinearColor(0, 0, 0, .55f));
    D.Fill(Canvas, Layer, 20);
    auto *Popups = D.Make<UWidgetSwitcher>(TEXT("WS_Popups"));
    Layer->AddChild(Popups);
    auto *Trip = D.Modal(Popups, TEXT("Popup_Trip"), {620, 480});
    D.At(Trip, D.Local(TEXT("TripTitle"), 32), {30, 24}, {560, 75});
    D.At(Trip, D.Local(TEXT("TripQuestion"), 22, FLinearColor(.35f, .35f, .35f)), {30, 102}, {560, 50});
    for (int32 I = 0; I < 2; ++I)
    {
        auto *B = D.Button(I ? TEXT("BTN_RoundTrip") : TEXT("BTN_OneWay"), FLinearColor(.94f, .92f, 1));
        auto Style = B->GetStyle();
        for (auto *BrushPart : {&Style.Normal, &Style.Hovered, &Style.Pressed})
            BrushPart->OutlineSettings = FSlateBrushOutlineSettings(14.f, FSlateColor(Purple), 2.f);
        B->SetStyle(Style);
        auto *Content = D.Make<UCanvasPanel>(I ? TEXT("RoundContent") : TEXT("OneContent"));
        B->AddChild(Content);
        D.At(Content, D.Local(I ? TEXT("RoundTrip") : TEXT("OneWay"), 28), {5, 14}, {250, 48});
        D.At(Content, D.Text(I ? TEXT("TXT_RoundTripPrice") : TEXT("TXT_OneWayPrice"), TEXT(""), 38, Purple), {5, 64},
             {250, 50});
        D.At(Content, D.Local(I ? TEXT("ReturnDiscount") : TEXT("SingleJourney"), 20, FLinearColor(.2f, .2f, .2f)),
             {10, 124}, {240, 60});
        D.At(Trip, B, {30. + I * 300, 166}, {260, 200});
    }
    D.At(Trip, D.LabelButton(TEXT("BTN_TripCancel"), TEXT("Cancel"), FLinearColor(.75f, .75f, .75f), 22), {180, 395},
         {260, 52});
    auto *Payment = D.Modal(Popups, TEXT("Popup_Payment"), {600, 470});
    D.At(Payment, D.Local(TEXT("Payment"), 34), {30, 30}, {540, 70});
    D.At(Payment, D.Text(TEXT("TXT_PaymentDescription"), TEXT(""), 24, FLinearColor(.35f, .35f, .35f)), {35, 110},
         {530, 70});
    D.At(Payment, D.Local(TEXT("Total"), 22, FLinearColor(.4f, .4f, .4f)), {60, 192}, {480, 40});
    D.At(Payment, D.Text(TEXT("TXT_PaymentTotal"), TEXT(""), 48), {60, 232}, {480, 65});
    D.At(Payment, D.Local(TEXT("TapCard"), 22), {30, 316}, {540, 68});
    D.At(Payment,
         D.LabelButton(TEXT("BTN_PaymentCancel"), TEXT("Cancel"), FLinearColor(.75f, .75f, .75f), 22, TEXT("_Payment")),
         {180, 400}, {240, 48});
    auto *Processing = D.Modal(Popups, TEXT("Popup_Processing"), {640, 200});
    D.At(Processing, D.Local(TEXT("Processing"), 30), {25, 40}, {590, 70});
    D.At(Processing, D.Local(TEXT("DoNotRemove"), 24, FLinearColor(.4f, .4f, .4f)), {25, 120}, {590, 60});
    auto *Success = D.Modal(Popups, TEXT("Popup_Success"), {720, 580});
    auto *Tick = D.Border(TEXT("SuccessCheck"), FLinearColor(.015f, .55f, .10f), 35);
    Tick->AddChild(D.Text(TEXT("TXT_Check"), TEXT("✓"), 45, White));
    D.At(Success, Tick, {325, 20}, {70, 70});
    D.At(Success, D.Local(TEXT("Success"), 30), {25, 104}, {670, 80});
    D.At(Success, D.Local(TEXT("Collect"), 23, FLinearColor(.35f, .35f, .35f)), {25, 190}, {670, 55});
    D.At(Success, D.Local(TEXT("Thanks"), 25), {25, 250}, {670, 40});
    D.At(Success, D.Text(TEXT("TXT_Receipt"), TEXT(""), 19, FLinearColor(.3f, .3f, .3f)), {35, 303}, {650, 175});
    D.At(Success, D.LabelButton(TEXT("BTN_SuccessHome"), TEXT("Done"), Green, 24), {110, 505}, {500, 52});
    auto *Assistance = D.Modal(Popups, TEXT("Popup_Assistance"), {680, 440});
    D.At(Assistance, D.Image(TEXT("IMG_AssistanceBell"), TEXT("Assistance")), {285, 30}, {110, 110});
    D.At(Assistance, D.Local(TEXT("HelpTitle"), 30), {25, 163}, {630, 80});
    D.At(Assistance, D.Local(TEXT("HelpMessage"), 24, FLinearColor(.4f, .4f, .4f)), {35, 260}, {610, 75});
    D.At(Assistance, D.LabelButton(TEXT("BTN_StaffClose"), TEXT("OK"), Green, 24), {210, 358}, {260, 52});
    Layer->SetVisibility(ESlateVisibility::Collapsed);
    // The projection regions are separate from the touchscreen: they align to actual
    // 3D hardware. No hardware illustration is flattened into the screen widget.
    for (const FString Name : {TEXT("BTN_HardwareHelp"), TEXT("BTN_HardwarePay"), TEXT("BTN_HardwareCard")})
    {
        auto *B = D.Button(Name, White, true);
        D.At(Root, B, {-1000, -1000}, {1, 1}, 10);
    }
    auto *Exit = D.LabelButton(TEXT("BTN_ExitKiosk"), TEXT("Exit"), FLinearColor(.8f, .8f, .85f), 20);
    auto *ExitSlot = D.At(Root, Exit, {-128, 12}, {116, 40}, 30);
    ExitSlot->SetAnchors(FAnchors(1, 0));
    auto *Notice = D.Local(TEXT("Demo"), 16, White);
    auto *NoticeSlot = D.At(Root, Notice, {12, -30}, {340, 24}, 30);
    NoticeSlot->SetAnchors(FAnchors(0, 1));
    Tree->ForEachWidget(
        [](UWidget *Child)
        {
            if (auto *ButtonSlot = Cast<UButtonSlot>(Child->Slot))
            {
                const bool bCanvas = Child->IsA<UCanvasPanel>();
                ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
                ButtonSlot->SetVerticalAlignment(bCanvas ? VAlign_Fill : VAlign_Center);
                ButtonSlot->SetPadding(bCanvas ? FMargin(0) : FMargin(8, 4));
            }
        });
    BP->ParentClass = UTransitKioskWidget::StaticClass();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status != BS_Error && SaveDesignObject(BP);
}
} // namespace

#include "Engine/StaticMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "EngineUtils.h"
#include "FileHelpers.h"

namespace
{
UMaterial *SolidHardwareMaterial(const FString &Name, FColor Color, float Metallic = 0.f)
{
    const FString Path = TEXT("/Game/Kiosk/Design/M_") + Name;
    if (auto *Existing = LoadObject<UMaterial>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        return Existing;
    auto *Material = NewObject<UMaterial>(CreatePackage(*Path), *(TEXT("M_") + Name), RF_Public | RF_Standalone);
    Material->SetShadingModel(MSM_DefaultLit);
    auto *Base = NewObject<UMaterialExpressionConstant3Vector>(Material);
    Base->Constant = FLinearColor::FromSRGBColor(Color);
    Material->GetExpressionCollection().AddExpression(Base);
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Base);
    for (bool bMetallic : {false, true})
    {
        auto *Value = NewObject<UMaterialExpressionConstant>(Material);
        Value->R = bMetallic ? Metallic : .38f;
        Material->GetExpressionCollection().AddExpression(Value);
        if (bMetallic)
            Material->GetEditorOnlyData()->Metallic.Connect(0, Value);
        else
            Material->GetEditorOnlyData()->Roughness.Connect(0, Value);
    }
    Material->PostEditChange();
    FAssetRegistryModule::AssetCreated(Material);
    return SaveDesignObject(Material) ? Material : nullptr;
}

UMaterial* InteractionGlowMaterial()
{
    const FString Path=TEXT("/Game/Kiosk/Design/M_KioskInteractionGlow");
    if (auto* Existing=LoadObject<UMaterial>(nullptr,*Path,nullptr,LOAD_NoWarn|LOAD_Quiet)) return Existing;
    auto* Material=NewObject<UMaterial>(CreatePackage(*Path),TEXT("M_KioskInteractionGlow"),RF_Public|RF_Standalone);
    Material->SetShadingModel(MSM_Unlit);
    auto* Color=NewObject<UMaterialExpressionVectorParameter>(Material);
    Color->ParameterName=TEXT("GlowColor");
    Color->DefaultValue=FLinearColor(.02f,.65f,1.f);
    auto* Bright=NewObject<UMaterialExpressionMultiply>(Material);
    Bright->A.Connect(0,Color);
    Bright->ConstB=1.5f;
    auto* Inverse=NewObject<UMaterialExpressionEyeAdaptationInverse>(Material);
    Inverse->LightValueInput.Connect(0,Bright);
    auto* Alpha=NewObject<UMaterialExpressionConstant>(Material);
    Alpha->R=1.f;
    Inverse->AlphaInput.Connect(0,Alpha);
    for (UMaterialExpression* Expression : TArray<UMaterialExpression*>{Color,Bright,Inverse,Alpha})
        Material->GetExpressionCollection().AddExpression(Expression);
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0,Inverse);
    Material->PostEditChange();
    FAssetRegistryModule::AssetCreated(Material);
    return SaveDesignObject(Material) ? Material : nullptr;
}

bool UpdateHelpAppearance(UBlueprint* BP)
{
    if (!BP->SimpleConstructionScript) return false;
    auto* Ring=SolidHardwareMaterial(TEXT("HelpRingOff"),FColor(20,60,26),.3f);
    if (!Ring) return false;
    BP->Modify();
    const auto Nodes=BP->SimpleConstructionScript->GetAllNodes();
    for (auto* Node : Nodes)
    {
        const auto Name=Node->GetVariableName().ToString();
        if (Name.StartsWith(TEXT("HelpIcon")) || Name==TEXT("HelpEarpiece") || Name==TEXT("HelpMouthpiece"))
            BP->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);
        else if (Name==TEXT("HelpMount")) CastChecked<UStaticMeshComponent>(Node->ComponentTemplate)->SetMaterial(0,Ring);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return true;
}

bool RefineHardware(UBlueprint *BP)
{
    auto *SCS = BP->SimpleConstructionScript.Get();
    if (!SCS || SCS->GetRootNodes().IsEmpty())
        return false;
    auto *Root = SCS->GetRootNodes()[0];
    auto *Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto *Grey = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Kiosk/Materials/Grey"));
    auto *Frame = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Kiosk/Materials/Frame"));
    auto *Screen = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Kiosk/Materials/Screen"));
    auto *Payment = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Kiosk/Materials/Payment"));
    auto *Help = SolidHardwareMaterial(TEXT("HardwareHelpYellow"), FColor(255, 215, 32));
    auto *Charcoal = SolidHardwareMaterial(TEXT("HardwareCharcoal"), FColor(24, 30, 38));
    auto *Metal = SolidHardwareMaterial(TEXT("HardwareMetal"), FColor(148, 158, 170), .65f);
    auto *Reader = SolidHardwareMaterial(TEXT("HardwareReader"), FColor(70, 42, 152));
    auto *GreenKey = SolidHardwareMaterial(TEXT("HardwareConfirm"), FColor(30, 174, 99));
    auto *RedKey = SolidHardwareMaterial(TEXT("HardwareCancel"), FColor(218, 49, 49));
    auto *Cabinet = SolidHardwareMaterial(TEXT("AccessibleCabinet"), FColor(170, 184, 195));
    auto *Panel = SolidHardwareMaterial(TEXT("AccessiblePanel"), FColor(42, 62, 80));
    auto *Card = SolidHardwareMaterial(TEXT("DemoTransitCard"), FColor(47, 188, 218));
    auto *Paper = SolidHardwareMaterial(TEXT("DemoTicket"), FColor(242, 245, 236));
    auto *Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cube || !Grey || !Frame || !Screen || !Payment || !Help || !Charcoal || !Metal || !Reader || !GreenKey || !RedKey ||
        !Cabinet || !Panel || !Card || !Paper || !Cylinder)
        return false;
    BP->Modify();
    // Retire the migration's decal-like hardware planes, leaving the touchscreen artwork alone.
    const auto OldNodes = SCS->GetAllNodes();
    for (auto *Node : OldNodes)
        if (Node->GetVariableName().ToString().StartsWith(TEXT("Design")))
            SCS->RemoveNodeAndPromoteChildren(Node);
    auto Part = [&](FName Name, FVector Position, FVector Dimensions, UMaterialInterface *Material,
                    bool bCollision = true) -> UStaticMeshComponent *
    {
        USCS_Node *Node = nullptr;
        for (auto *N : SCS->GetAllNodes())
            if (N->GetVariableName() == Name)
            {
                Node = N;
                break;
            }
        if (!Node)
        {
            Node = SCS->CreateNode(UStaticMeshComponent::StaticClass(), Name);
            Root->AddChildNode(Node);
        }
        else if (auto *Parent = SCS->FindParentNode(Node); Parent && Parent != Root)
        {
            Parent->RemoveChildNode(Node, false);
            Root->AddChildNode(Node, false);
        }
        auto *Mesh = CastChecked<UStaticMeshComponent>(Node->ComponentTemplate);
        Mesh->ComponentTags.AddUnique(Name);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetRelativeLocation(Position);
        Mesh->SetRelativeRotation(FRotator::ZeroRotator);
        Mesh->SetRelativeScale3D(Dimensions / 100);
        Mesh->SetMaterial(0, Material);
        Mesh->SetVisibility(true);
        Mesh->SetHiddenInGame(false);
        Mesh->SetCastShadow(true);
        Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        return Mesh;
    };
    auto Label = [&](FName Name, const FString &Text, FVector Position, float Size, FColor Color = FColor::White)
    {
        USCS_Node *Node = nullptr;
        for (auto *N : SCS->GetAllNodes())
            if (N->GetVariableName() == Name)
                Node = N;
        if (!Node)
        {
            Node = SCS->CreateNode(UTextRenderComponent::StaticClass(), Name);
            Root->AddChildNode(Node);
        }
        auto *TextMesh = CastChecked<UTextRenderComponent>(Node->ComponentTemplate);
        TextMesh->SetText(FText::FromString(Text));
        TextMesh->SetVisibility(true);
        TextMesh->SetHiddenInGame(false);
        TextMesh->SetWorldSize(Size);
        TextMesh->SetHorizontalAlignment(EHTA_Center);
        TextMesh->SetVerticalAlignment(EVRTA_TextCenter);
        TextMesh->SetTextRenderColor(Color);
        TextMesh->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Position));
    };

    // Fixed accessible cabinet. Every part is attached to the same actor; no actor-wide scaling.
    Part(TEXT("MachineBase"), {0, 0, 96}, {45, 112, 192}, Cabinet);
    Part(TEXT("CabinetFoot"), {0, 0, 2.5}, {52, 116, 5}, Metal);
    Part(TEXT("ControlPanel"), {25.5, 0, 124}, {5, 106, 70}, Panel);
    Part(TEXT("IdentificationPanel"), {26, 0, 181}, {6, 104, 12}, Panel);
    Label(TEXT("IdentificationSign"), TEXT("TICKET KIOSK"), {29.1, 0, 181}, 10.f);
    const FRotator ScreenTilt(12, 0, 0);
    const FVector ScreenOrigin(31, 20, 125);
    Part(TEXT("ScreenFrame"), ScreenOrigin, {6, 61, 46.1}, Frame)->SetRelativeRotation(ScreenTilt);
    Part(TEXT("ScreenDisplay"), ScreenOrigin + ScreenTilt.RotateVector(FVector(4, 0, 0)), {1.5, 55, 55.*700/960}, Screen)
        ->SetRelativeRotation(ScreenTilt);
    Part(TEXT("PaymentMount"), {28, -31, 128}, {6, 26, 28}, Metal);
    Part(TEXT("PaymentReader"), {32, -31, 128}, {8, 22, 24}, Payment);
    Part(TEXT("PaymentDisplayFrame"), {36.3, -31, 135}, {1, 19, 8}, Charcoal, false);
    Part(TEXT("PaymentDisplay"), {37, -31, 135}, {.7, 17, 6}, Screen, false);
    Label(TEXT("PaymentDisplayLabel"), TEXT("TAP CARD"), {37.5, -31, 135}, 2.2f);
    Label(TEXT("PaymentLabel"), TEXT("PAYMENT"), {28.2, -31, 143}, 3.f);
    for (int32 Row = 0; Row < 4; ++Row)
        for (int32 Column = 0; Column < 3; ++Column)
        {
            const FString Name = FString::Printf(TEXT("PaymentKey_%d_%d"), Row, Column);
            const FVector Position(36.7, -24 - Column * 7, 127 - Row * 3);
            auto *KeyMaterial = Row == 3 ? (Column == 0 ? RedKey : Column == 2 ? GreenKey : Metal) : Charcoal;
            Part(*Name, Position, {1.4, 5.5, 2.4}, KeyMaterial, false);
            const FString Legend = Row == 3 ? (Column == 0 ? TEXT("X") : Column == 1 ? TEXT("0") : TEXT("OK"))
                                            : FString::FromInt(Row * 3 + Column + 1);
            Label(*(Name + TEXT("Label")), Legend, Position + FVector(.8, 0, 0), Row == 3 ? 1.6f : 1.9f);
        }
    auto *HelpRim = Part(TEXT("HelpMount"), {30, -31, 107}, {12, 12, 3}, Metal);
    HelpRim->SetStaticMesh(Cylinder);
    HelpRim->SetRelativeRotation(FRotator(90, 0, 0));
    auto *HelpFace = Part(TEXT("HelpButton"), {32, -31, 107}, {10, 10, 2}, Help);
    HelpFace->SetStaticMesh(Cylinder);
    HelpFace->SetRelativeRotation(FRotator(90, 0, 0));
    Label(TEXT("AssistanceLabel"), TEXT("HELP"), {28.2, -43, 107}, 2.7f);

    const FRotator ReaderTilt(75, 0, 0);
    const FVector ReaderOrigin(39, 25, 98);
    auto ReaderPart = [&](FName Name, FVector Offset, FVector Size, UMaterialInterface *Material)
    {
        auto *Mesh = Part(Name, ReaderOrigin + ReaderTilt.RotateVector(Offset), Size, Material, false);
        Mesh->SetRelativeRotation(ReaderTilt);
        return Mesh;
    };
    ReaderPart(TEXT("TransitCardReader"), {0, 0, 0}, {3, 18, 12}, Reader)->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ReaderPart(TEXT("CardReaderPad"), {1.6, 0, 0}, {.4, 15, 10}, Charcoal);
    ReaderPart(TEXT("CardReaderIndicator"), {2, -6, 4}, {.4, 2, .8}, GreenKey);
    Label(TEXT("CardReaderLabel"), TEXT("TAP CARD"), ReaderOrigin + ReaderTilt.RotateVector(FVector(2, 0, 4)), 2.f);
    for (auto *N : SCS->GetAllNodes())
        if (N->GetVariableName() == TEXT("CardReaderLabel"))
            CastChecked<UTextRenderComponent>(N->ComponentTemplate)->SetRelativeRotation(ReaderTilt);
    ReaderPart(TEXT("CardHolderBase"), {2.1, 0, -2.8}, {1, 10, 6.5}, Metal);
    ReaderPart(TEXT("CardHolderLip"), {3, 0, -6}, {1.2, 10, .8}, Metal);
    ReaderPart(TEXT("CardHolderLeft"), {2.7, 4.8, -2.8}, {.8, .4, 6.5}, Metal);
    ReaderPart(TEXT("CardHolderRight"), {2.7, -4.8, -2.8}, {.8, .4, 6.5}, Metal);
    auto *CardProp = ReaderPart(TEXT("TransitCardProp"), {2.8, 0, -2.4}, {.12, 8.6, 5.4}, Card);
    CardProp->SetVisibility(false);
    CardProp->SetHiddenInGame(true);

    Part(TEXT("TicketCollection"), {29, -19, 96}, {6, 32, 9}, Frame);
    Part(TEXT("CollectionRecess"), {32.5, -19, 96}, {1, 28, 5}, Charcoal, false);
    Part(TEXT("CollectionTop"), {35, -19, 99.5}, {8, 32, 2}, Metal);
    Part(TEXT("CollectionBottom"), {35, -19, 92.5}, {8, 32, 2}, Metal);
    Part(TEXT("CollectionLeft"), {35, -4, 96}, {8, 2, 5}, Metal);
    Part(TEXT("CollectionRight"), {35, -34, 96}, {8, 2, 5}, Metal);
    Part(TEXT("CollectionTray"), {37, -19, 91}, {14, 32, 1.5}, Metal);
    Label(TEXT("CollectionLabel"), TEXT("COLLECT TICKET / CARD"), {28.2, -19, 87.5}, 2.8f);
    auto *Output = Part(TEXT("CollectionItem"), {37, -19, 95}, {7, 8.6, .12}, Paper, false);
    Output->SetVisibility(false);
    Output->SetHiddenInGame(true);
    USCS_Node *Assembly = nullptr;
    for (auto *Node : SCS->GetAllNodes())
        if (Node->GetVariableName() == TEXT("ScreenAssembly")) Assembly = Node;
    if (!Assembly)
    {
        Assembly = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("ScreenAssembly"));
        Root->AddChildNode(Assembly);
    }
    CastChecked<USceneComponent>(Assembly->ComponentTemplate)->SetRelativeTransform(FTransform(ScreenTilt, ScreenOrigin));
    for (auto *Node : SCS->GetAllNodes())
        if (Node->GetVariableName() == TEXT("ScreenFrame") || Node->GetVariableName() == TEXT("ScreenDisplay"))
        {
            if (auto *Parent = SCS->FindParentNode(Node); Parent && Parent != Assembly)
            {
                Parent->RemoveChildNode(Node, false);
                Assembly->AddChildNode(Node, false);
            }
            auto *Mesh = CastChecked<UStaticMeshComponent>(Node->ComponentTemplate);
            Mesh->SetRelativeLocation(Node->GetVariableName() == TEXT("ScreenDisplay") ? FVector(4,0,0) : FVector::ZeroVector);
            Mesh->SetRelativeRotation(FRotator::ZeroRotator);
        }
    if (!UpdateHelpAppearance(BP)) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error)
        return false;
    auto* ScreenMaterial = LoadObject<UMaterial>(nullptr,TEXT("/Game/Kiosk/Design/M_KioskTouchscreen"),nullptr,LOAD_NoWarn|LOAD_Quiet);
    if (!ScreenMaterial)
    {
        auto* Source = LoadObject<UMaterial>(nullptr,TEXT("/Engine/EngineMaterials/Widget3DPassThrough"));
        if (!Source) return false;
        ScreenMaterial = DuplicateObject<UMaterial>(Source,CreatePackage(TEXT("/Game/Kiosk/Design/M_KioskTouchscreen")),TEXT("M_KioskTouchscreen"));
        ScreenMaterial->SetFlags(RF_Public|RF_Standalone);
        ScreenMaterial->BlendMode = BLEND_Opaque;
        ScreenMaterial->TwoSided = false;
        auto* Inverse = NewObject<UMaterialExpressionEyeAdaptationInverse>(ScreenMaterial);
        auto& Emissive = ScreenMaterial->GetEditorOnlyData()->EmissiveColor;
        Inverse->LightValueInput.Connect(Emissive.OutputIndex,Emissive.Expression);
        auto* Alpha = NewObject<UMaterialExpressionConstant>(ScreenMaterial);
        Alpha->R = 1.f;
        Inverse->AlphaInput.Connect(0,Alpha);
        ScreenMaterial->GetExpressionCollection().AddExpression(Inverse);
        ScreenMaterial->GetExpressionCollection().AddExpression(Alpha);
        Emissive.Connect(0,Inverse);
        ScreenMaterial->PostEditChange();
        FAssetRegistryModule::AssetCreated(ScreenMaterial);
        if (!SaveDesignObject(ScreenMaterial)) return false;
    }
    CastChecked<ATransitKioskActor>(BP->GeneratedClass->GetDefaultObject())->TouchscreenMaterial=ScreenMaterial;
    auto* Glow=InteractionGlowMaterial();
    if (!Glow) return false;
    CastChecked<ATransitKioskActor>(BP->GeneratedClass->GetDefaultObject())->InteractionGlowMaterial=Glow;
    return SaveDesignObject(BP);
}
} // namespace

bool UpdateKioskInteractionAssets()
{
    auto* Hardware=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
    auto* UI=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Kiosk/UI/KioskUI"));
    if (!Hardware || !UI) return false;
    const FString Backup=FPaths::ProjectSavedDir()/TEXT("KioskInteractionAssetBackup")/FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    IFileManager::Get().MakeDirectory(*Backup,true);
    for (UObject* Object : TArray<UObject*>{Hardware,UI})
    {
        const auto Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if (IFileManager::Get().Copy(*(Backup/FPaths::GetCleanFilename(Filename)),*Filename)!=COPY_OK) return false;
    }
    const auto MapFile=FPackageName::LongPackageNameToFilename(TEXT("/Game/Kiosk/Maps/Demo"),FPackageName::GetMapPackageExtension());
    if (IFileManager::Get().Copy(*(Backup/TEXT("Demo.umap")),*MapFile)!=COPY_OK) return false;
    auto* Glow=InteractionGlowMaterial();
    if (!Glow || !EnsureCollectionWarning(UI) || !EnsureQuantityPopup(UI)) return false;
    UI->Modify();
    for (const FString Key : {TEXT("CollectTitle"),TEXT("CollectInstruction"),TEXT("CollectHelp")})
        if (auto* Text=Cast<UTextBlock>(UI->WidgetTree->FindWidget(*(TEXT("LOC_")+Key))))
            Text->SetText(FText::FromString(KioskText(TEXT("English"),*Key)));
    FBlueprintEditorUtils::MarkBlueprintAsModified(UI);
    FKismetEditorUtilities::CompileBlueprint(UI);
    if (UI->Status==BS_Error || !SaveDesignObject(UI)) return false;
    Hardware->Modify();
    if (!UpdateHelpAppearance(Hardware)) return false;
    FKismetEditorUtilities::CompileBlueprint(Hardware);
    if (Hardware->Status==BS_Error) return false;
    CastChecked<ATransitKioskActor>(Hardware->GeneratedClass->GetDefaultObject())->InteractionGlowMaterial=Glow;
    if (!SaveDesignObject(Hardware)) return false;
    auto* World=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (!World) return false;
    for (TActorIterator<ATransitKioskActor> It(World);It;++It)
    {
        It->Modify();
        It->RerunConstructionScripts();
        It->InteractionGlowMaterial=Glow;
    }
    const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/Kiosk/Maps/Demo"));
    UE_LOG(LogTemp,Display,TEXT("KIOSK_INTERACTION_ASSETS %s"),Saved ? TEXT("SUCCESS") : TEXT("FAILED"));
    return Saved;
}

bool MigrateKioskDesign(bool bHardwareOnly)
{
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    auto *ActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
    if (!BP || !ActorBP)
        return false;
    // The existing Git baseline and timestamped package copies keep recovery independent of
    // whether a later Blueprint compile/import succeeds. Only Unreal serializes uassets.
    const FString Folder =
        FPaths::ProjectSavedDir() / TEXT("KioskMigrationBackup") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    for (const FString Relative :
         {TEXT("Content/Kiosk/UI/KioskUI.uasset"), TEXT("Content/Kiosk/Blueprint/BP_TransitKiosk.uasset"),
          TEXT("Content/Kiosk/Maps/Demo.umap")})
    {
        const FString Backup = Folder / Relative;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (IFileManager::Get().Copy(*Backup, *(FPaths::ProjectDir() / Relative)) != COPY_OK)
            return false;
    }
    if (!bHardwareOnly)
    {
        TMap<FString, UTexture2D *> Art;
        for (const FString Name :
             {TEXT("Train"), TEXT("Map"), TEXT("Purchase"), TEXT("TopUp"), TEXT("Destination"), TEXT("DayPass"),
              TEXT("Collection"), TEXT("CardReader"), TEXT("PaymentTerminal"), TEXT("Help"), TEXT("Assistance"), TEXT("AssistanceLabel")})
        {
            auto *Texture = ImportArt(Name);
            if (!Texture)
                return false;
            Art.Add(Name, Texture);
        }
        auto *Font = DesignFont();
        if (!Font || !BuildDesignWidget(BP, Font, Art)) return false;
    }
    if (!EnsureCollectionWarning(BP) || !EnsureQuantityPopup(BP) || !RefineHardware(ActorBP))
        return false;
    if (!bHardwareOnly)
    {
        CastChecked<ATransitKioskActor>(ActorBP->GeneratedClass->GetDefaultObject())->DemoConfig = FTransitKioskConfig();
        if (!SaveDesignObject(ActorBP))
            return false;
    }
    UWorld *World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (!World)
        return false;
    for (TActorIterator<ATransitKioskActor> It(World); It; ++It)
    {
        It->Modify();
        It->TouchscreenMaterial = CastChecked<ATransitKioskActor>(ActorBP->GeneratedClass->GetDefaultObject())->TouchscreenMaterial;
        It->InteractionGlowMaterial = CastChecked<ATransitKioskActor>(ActorBP->GeneratedClass->GetDefaultObject())->InteractionGlowMaterial;
        if (!bHardwareOnly)
            It->DemoConfig = FTransitKioskConfig();
        It->RerunConstructionScripts();
    }
    const bool Saved = UEditorLoadingAndSavingUtils::SaveMap(World, TEXT("/Game/Kiosk/Maps/Demo"));
    UE_LOG(LogTemp, Display, TEXT("KIOSK_MIGRATION %s"), Saved ? TEXT("SUCCESS") : TEXT("FAILED"));
    return Saved;
}

#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"
#include "TimerManager.h"
#include "Misc/AutomationTest.h"
#include "Components/PanelWidget.h"

bool VerifyKioskDesign(bool bRender)
{
    FString Report;
    int32 Failures = 0;
    auto Check = [&](bool Pass, const FString &Name)
    {
        Report += (Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Name + TEXT("\n");
        if (!Pass)
        {
            ++Failures;
            UE_LOG(LogTemp, Error, TEXT("KIOSK_VERIFY %s"), *Name);
        }
    };
#if WITH_DEV_AUTOMATION_TESTS
    FAutomationTestFramework::Get().StartTestByName(TEXT("FTransitKioskTransactionTest"), 0);
    FAutomationTestExecutionInfo Info;
    Check(GIsAutomationTesting && FAutomationTestFramework::Get().StopTest(Info),
          TEXT("All station fares, languages, top-ups, navigation, duplicate payment and midnight regression tests"));
#endif
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Kiosk/UI/KioskUI"));
    auto *ActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
    if (!BP || !ActorBP)
        return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    FKismetEditorUtilities::CompileBlueprint(ActorBP);
    Check(BP->Status != BS_Error && ActorBP->Status != BS_Error, TEXT("Saved Blueprints compile"));
    auto *Pages = Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("WS_Pages")));
    auto *Popups = Cast<UWidgetSwitcher>(BP->WidgetTree->FindWidget(TEXT("WS_Popups")));
    Check(Pages && Pages->GetChildrenCount() == 7, TEXT("Seven editable shared screens"));
    Check(Popups && Popups->GetChildrenCount() == 6, TEXT("Six editable shared overlays including quantity"));
    if (!Pages || !Popups)
        return false;
    TSet<FName> Components;
    for (auto *Node : ActorBP->SimpleConstructionScript->GetAllNodes())
        Components.Add(Node->GetVariableName());
    for (FName Name : {TEXT("MachineBase"), TEXT("ScreenFrame"), TEXT("ScreenDisplay"), TEXT("PaymentReader"),
                       TEXT("HelpButton"), TEXT("TransitCardReader"), TEXT("TicketCollection")})
        Check(Components.Contains(Name), TEXT("Kiosk component ") + Name.ToString());
    UWorld *World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (!World)
        return false;
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(
            FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"))
                .CreateSlateRHIRenderer());
    auto *W = CreateWidget<UTransitKioskWidget>(World, BP->GeneratedClass.Get());
    W->AddToRoot();
    W->InitializeKiosk(FTransitKioskConfig(), 340);
    auto Slate = W->TakeWidget();
    auto Click = [&](FName Name)
    {
        auto *B = Cast<UButton>(W->GetWidgetFromName(Name));
        Check(B && B->GetIsEnabled() && B->OnClicked.IsBound(), TEXT("Interactive ") + Name.ToString());
        if (B && B->GetIsEnabled())
            B->OnClicked.Broadcast();
    };
    auto Timers = [&]()
    {
        ++GFrameCounter;
        World->GetTimerManager().Tick(.001f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(2.1f);
    };
    const FString Languages[] = {TEXT("English"), TEXT("Chinese"), TEXT("Malay"), TEXT("Tamil")};
    const int32 Fares[] = {200, 200, 200, 250, 300, 250, 400, 200};
    for (const auto &Language : Languages)
        for (int32 Station = 0; Station < 8; ++Station)
            for (bool Round : {false, true})
            {
                W->InitializeKiosk(FTransitKioskConfig(), 340);
                W->Refresh();
                Click(TEXT("BTN_Start"));
                Click(*(TEXT("BTN_Language") + Language));
                Click(TEXT("BTN_Purchase"));
                Click(TEXT("BTN_Destination"));
                auto *S = W->GetSession();
                Check(!W->GetWidgetFromName(TEXT("BTN_Confirm"))->GetIsEnabled(),
                      TEXT("Confirm disabled until station selected"));
                Click(*FString::Printf(TEXT("BTN_Station%d"), (Station + 1) % 8));
                Click(*FString::Printf(TEXT("BTN_Station%d"), Station));
                Check(S->Destination == Station, TEXT("Changing marker replaces selection"));
                Click(TEXT("BTN_Confirm"));
                Click(TEXT("BTN_TripCancel"));
                Check(S->Popup == EKioskPopup::None && S->Destination == Station,
                      TEXT("Trip cancel retains selected marker"));
                Click(TEXT("BTN_Confirm"));
                Click(Round ? TEXT("BTN_RoundTrip") : TEXT("BTN_OneWay"));
                Check(S->Popup==EKioskPopup::Quantity,TEXT("Ticket quantity precedes payment"));
                Check(!W->GetWidgetFromName(TEXT("BTN_QuantityMinus"))->GetIsEnabled(),TEXT("Minus disabled at one person"));
                Click(TEXT("BTN_QuantityPlus")); Click(TEXT("BTN_QuantityPlus"));
                Check(S->TicketQuantity==3,TEXT("Plus selects three people"));
                Click(TEXT("BTN_QuantityMinus"));
                Check(S->TicketQuantity==2,TEXT("Minus reduces quantity"));
                Click(TEXT("BTN_QuantityPlus"));
                Click(TEXT("BTN_QuantityConfirm"));
                Check(S->Popup == EKioskPopup::Payment && S->GetTotalCents() == 3*Fares[Station] * (Round ? 3 : 2) / 2,
                      TEXT("Payment displays correct station/trip total"));
                Click(TEXT("BTN_HardwareHelp"));
                Click(TEXT("BTN_StaffClose"));
                Check(S->Popup == EKioskPopup::Payment, TEXT("Assistance restores payment"));
                Click(TEXT("BTN_PaymentCancel"));
                Check(S->Popup==EKioskPopup::Quantity && S->TicketQuantity==3,TEXT("Payment cancel preserves quantity"));
                Click(TEXT("BTN_QuantityCancel")); Click(TEXT("BTN_TripCancel"));
                Check(S->Page == EKioskPage::Route && S->Destination == Station, TEXT("Payment cancel retains route"));
                Click(TEXT("BTN_Confirm"));
                Click(Round ? TEXT("BTN_RoundTrip") : TEXT("BTN_OneWay"));
                Click(TEXT("BTN_QuantityConfirm"));
                Click(TEXT("BTN_HardwarePay"));
                Check(S->Popup == EKioskPopup::Processing, TEXT("Simulated hardware tap starts processing"));
                Timers();
                Check(S->Popup == EKioskPopup::Success && !S->ReceiptId.IsEmpty() && S->Language == Language,
                      TEXT("Timed success retains receipt and language"));
                Check(S->bTicketAwaitingCollection && !S->CompleteAndReset(),TEXT("Ticket blocks completion before pickup"));
                Check(W->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility()==ESlateVisibility::Visible,TEXT("Ticket reminder appears immediately"));
                Check(!W->GetWidgetFromName(TEXT("BTN_HardwareHelp"))->GetIsEnabled(),TEXT("All machine controls locked during pickup"));
                Check(S->CollectTicket(),TEXT("Physical-collection event simulated for UI fixture"));
                W->Refresh();
                Click(TEXT("BTN_SuccessHome"));
                Check(S->Page == EKioskPage::Welcome, TEXT("Explicit completion returns Welcome"));
            }
    for (const auto &Language : Languages)
        for (int32 I = 0; I < 6; ++I)
        {
            W->InitializeKiosk(FTransitKioskConfig(), 340);
            W->Refresh();
            Click(TEXT("BTN_Start"));
            Click(*(TEXT("BTN_Language") + Language));
            Click(TEXT("BTN_TopUp"));
            Check(W->GetSession()->Page == EKioskPage::CardPlacement, TEXT("Top-up first asks for transit card"));
            Click(TEXT("BTN_HardwareCard"));
            Click(*FString::Printf(TEXT("BTN_TopUp%d"), I));
            auto *S = W->GetSession();
            const int32 Amount = S->Config.TopUpAmountsCents[I];
            Check(S->GetTotalCents() == Amount && S->CardBalanceCents == 340,
                  TEXT("Amount is correct before approval"));
            Click(TEXT("BTN_HardwarePay"));
            Timers();
            Check(S->Popup == EKioskPopup::Success && S->CardBalanceCents == 340 + Amount,
                  TEXT("Top-up credited once after approval"));
            Check(S->bCardAwaitingCollection && S->bCardPresent && !S->CompleteAndReset(),TEXT("Top-up keeps card until pickup"));
            Check(W->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility()==ESlateVisibility::Visible,TEXT("Card reminder appears immediately"));
            W->HandleAction(TEXT("Home"));
            Check(S->bCardPresent && S->Popup==EKioskPopup::Success,TEXT("Home cannot discard card"));
            Check(S->CollectCard(),TEXT("Physical card collection simulated for UI fixture"));
            W->Refresh();
            Click(TEXT("BTN_SuccessHome"));
            Check(S->Page==EKioskPage::Welcome,TEXT("Top-up navigation restored after pickup"));
        }
    W->InitializeKiosk(FTransitKioskConfig(), 340);
    W->Refresh();
    Click(TEXT("BTN_Start"));
    Click(TEXT("BTN_LanguageEnglish"));
    Click(TEXT("BTN_Purchase"));
    Click(TEXT("BTN_DayPass"));
    Check(W->GetSession()->Popup==EKioskPopup::Quantity,TEXT("Day pass also requests quantity"));
    Click(TEXT("BTN_QuantityConfirm"));
    Check(W->GetSession()->Popup == EKioskPopup::Payment && W->GetSession()->GetTotalCents() == 1000,
          TEXT("Day pass bypasses map and review"));
    Click(TEXT("BTN_HardwarePay"));
    Timers();
    Check(W->GetSession()->CollectTicket(),TEXT("Day pass collected before Home"));
    W->Refresh();
    Click(TEXT("BTN_SuccessHome"));
    Click(TEXT("BTN_Start"));
    Click(TEXT("BTN_LanguageEnglish"));
    Click(TEXT("BTN_Purchase"));
    Check(W->GetWidgetFromName(TEXT("BTN_DayPass"))->GetIsEnabled(), TEXT("Next customer can buy another day pass"));
    Click(TEXT("BTN_Back"));
    Click(TEXT("BTN_TopUp"));
    Click(TEXT("BTN_HardwareCard"));
    Click(TEXT("BTN_TopUp0"));
    Click(TEXT("BTN_HardwarePay"));
    W->ShutdownKiosk();
    Timers();
    Check(W->GetSession()->CardBalanceCents == 340, TEXT("Closing cancels payment timer"));
    if (bRender)
    {
        FWidgetRenderer Renderer(false);
        const FString Folder = FPaths::ProjectSavedDir() / TEXT("KioskDesignPreview");
        IFileManager::Get().MakeDirectory(*Folder, true);
        struct FFrame
        {
            int32 PDF;
            int32 Language;
            EKioskPage Page;
            EKioskPopup Popup;
            int32 Station = -1;
            EKioskProduct Product = EKioskProduct::None;
            int32 Amount = 0;
        };
        TArray<FFrame> Frames;
        Frames.Add({7, 0, EKioskPage::Welcome, EKioskPopup::None});
        Frames.Add({8, 0, EKioskPage::Language, EKioskPopup::None});
        for (int32 Lang = 0; Lang < 4; ++Lang)
        {
            const int32 Base = 9 + 35 * Lang;
            Frames.Add({Base, Lang, EKioskPage::MainMenu, EKioskPopup::Assistance});
            const int32 Bands[] = {0, 3, 4, 6};
            for (int32 I = 0; I < 4; ++I)
                Frames.Add({Base + 1 + I, Lang, EKioskPage::Route, EKioskPopup::Trip, Bands[I]});
            const EKioskPage MainPages[] = {EKioskPage::MainMenu, EKioskPage::TicketType, EKioskPage::Route,
                                            EKioskPage::TopUp, EKioskPage::CardPlacement};
            for (int32 I = 0; I < 5; ++I)
                Frames.Add({Base + 5 + I, Lang, MainPages[I], EKioskPopup::None});
            const EKioskProduct Products[] = {
                EKioskProduct::OneWay,    EKioskProduct::DayPass,   EKioskProduct::TopUp,     EKioskProduct::TopUp,
                EKioskProduct::TopUp,     EKioskProduct::OneWay,    EKioskProduct::OneWay,    EKioskProduct::OneWay,
                EKioskProduct::RoundTrip, EKioskProduct::RoundTrip, EKioskProduct::RoundTrip, EKioskProduct::RoundTrip,
                EKioskProduct::TopUp,     EKioskProduct::TopUp,     EKioskProduct::TopUp};
            const int32 Stations[] = {0, -1, -1, -1, -1, 3, 4, 6, 0, 3, 4, 6, -1, -1, -1};
            const int32 Amounts[] = {0, 0, 500, 1000, 2000, 0, 0, 0, 0, 0, 0, 0, 3000, 5000, 10000};
            for (int32 I = 0; I < 15; ++I)
            {
                const auto Product = Products[I];
                Frames.Add({Base + 10 + I, Lang,
                            Product == EKioskProduct::DayPass ? EKioskPage::TicketType
                            : Product == EKioskProduct::TopUp ? EKioskPage::TopUp
                                                              : EKioskPage::Route,
                            EKioskPopup::Payment, Stations[I], Product, Amounts[I]});
            }
            Frames.Add({Base + 25, Lang, EKioskPage::Route, EKioskPopup::Processing, 0, EKioskProduct::OneWay});
            Frames.Add({Base + 26, Lang, EKioskPage::Route, EKioskPopup::Success, 0, EKioskProduct::OneWay});
            for (int32 I = 0; I < 8; ++I)
                Frames.Add({Base + 27 + I, Lang, EKioskPage::Route, EKioskPopup::None, I});
        }
        for (int32 Width : {1280, 1920})
            for (const auto &Frame : Frames)
            {
                W->InitializeKiosk(FTransitKioskConfig(), 340);
                auto *S = W->GetSession();
                S->Language = Languages[Frame.Language];
                S->Page = Frame.Page;
                S->Product = Frame.Product;
                S->Destination = Frame.Station;
                S->TopUpCents = Frame.Amount;
                S->bCardPresent = Frame.Product == EKioskProduct::TopUp;
                S->Popup = Frame.Popup;
                if (Frame.Popup == EKioskPopup::Success)
                {
                    S->Popup = EKioskPopup::Processing;
                    S->CompletePayment(FDateTime(2026, 11, 6, 12));
                }
                W->Refresh();
                const FVector2D Size(Width, Width * 9 / 16);
                UTextureRenderTarget2D *Target = nullptr;
                for (int32 Draw = 0; Draw < 4; ++Draw)
                {
                    ++GFrameCounter;
                    FSlateApplication::Get().Tick();
                    W->InvalidateLayoutAndVolatility();
                    W->ForceLayoutPrepass();
                    if (!Target)
                        Target = Renderer.DrawWidget(Slate, Size);
                    else
                        Renderer.DrawWidget(Target, Slate, Size, 0.f);
                    FlushRenderingCommands();
                }
                // DrawWidget(false) produces linear colors. ExportRenderTarget2DAsPNG
                // labels this target sRGB without converting its pixels; convert explicitly
                // so the preview agrees with normal viewport Slate and the source artwork.
                FImage LinearImage, DisplayImage;
                TArray64<uint8> Buffer;
                bool bSaved = Target && FImageUtils::GetRenderTargetImage(Target, LinearImage);
                if (bSaved)
                {
                    LinearImage.GammaSpace = EGammaSpace::Linear;
                    LinearImage.CopyTo(DisplayImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
                    bSaved = FImageUtils::CompressImage(Buffer, TEXT("PNG"), DisplayImage);
                }
                const FString Filename = Folder / FString::Printf(TEXT("PDF_%03d_%d.png"), Frame.PDF, Width);
                Check(bSaved && FFileHelper::SaveArrayToFile(Buffer, *Filename), TEXT("Render ") + Filename);
                // Only descend into the active switcher branch, ignoring stale cached geometry
                // from prior frames. Catch translated labels that exceed their allocated height.
                TFunction<void(UWidget *)> CheckText = [&](UWidget *Item)
                {
                    if (!Item || Item->GetVisibility() == ESlateVisibility::Collapsed ||
                        Item->GetVisibility() == ESlateVisibility::Hidden)
                        return;
                    if (auto *Text = Cast<UTextBlock>(Item))
                    {
                        const FVector2D Bounds = Text->GetCachedGeometry().GetLocalSize();
                        Check(Text->GetDesiredSize().Y <= Bounds.Y + 3.,
                              FString::Printf(TEXT("Text fits PDF %d %d %s desired=%.1f allotted=%.1f"), Frame.PDF,
                                              Width, *Text->GetName(), Text->GetDesiredSize().Y, Bounds.Y));
                    }
                    if (auto *Switch = Cast<UWidgetSwitcher>(Item))
                        CheckText(Switch->GetActiveWidget());
                    else if (auto *Panel = Cast<UPanelWidget>(Item))
                        for (auto *Child : Panel->GetAllChildren())
                            CheckText(Child);
                };
                CheckText(W->WidgetTree->RootWidget);
            }
    }
    W->ShutdownKiosk();
    W->RemoveFromRoot();
    Report += FString::Printf(TEXT("\nFailures: %d\n"), Failures);
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("KioskVerification.txt")),
                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_VERIFY %s failures=%d"), Failures ? TEXT("FAILED") : TEXT("SUCCESS"),
           Failures);
    return Failures == 0;
}
