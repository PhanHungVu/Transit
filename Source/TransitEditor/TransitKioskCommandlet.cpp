#include "TransitKioskCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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

#include "TransitKioskDesign.h"
UTransitKioskCommandlet::UTransitKioskCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
namespace
{
void DescribeWidget(UWidget *Widget, FString &Out, int32 Depth = 0)
{
    Out += FString::ChrN(Depth * 2, ' ') + Widget->GetName() + TEXT(" : ") + Widget->GetClass()->GetName();
    if (auto *Text = Cast<UTextBlock>(Widget))
        Out += FString::Printf(TEXT(" text=[%s] font=%.0f color=%s"), *Text->GetText().ToString(), Text->GetFont().Size,
                               *Text->GetColorAndOpacity().GetSpecifiedColor().ToString());
    if (auto *Border = Cast<UBorder>(Widget))
        Out += TEXT(" background=") + Border->GetBrushColor().ToString();
    if (auto *Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
        Out +=
            FString::Printf(TEXT(" position=%s size=%s"), *Slot->GetPosition().ToString(), *Slot->GetSize().ToString());
    Out += TEXT("\n");
    if (auto *Panel = Cast<UPanelWidget>(Widget))
        for (auto *Child : Panel->GetAllChildren())
            DescribeWidget(Child, Out, Depth + 1);
}
} // namespace

int32 UTransitKioskCommandlet::Main(const FString &Params)
{
    if (FParse::Param(*Params,TEXT("UpdateInteraction")))
        return UpdateKioskInteractionAssets() ? 0 : 1;
    if (FParse::Param(*Params, TEXT("InspectHardware")))
    {
        FString Source;
        UBlueprint *HardwareBP = nullptr;
        if (FParse::Value(*Params, TEXT("HardwareSource="), Source))
        {
            auto *Package = LoadPackage(nullptr, *Source, LOAD_None);
            HardwareBP = Package ? FindObject<UBlueprint>(Package, TEXT("BP_TransitKiosk")) : nullptr;
        }
        else
            HardwareBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"));
        if (!HardwareBP || !HardwareBP->SimpleConstructionScript)
            return 1;
        FString Hardware;
        for (auto *Node : HardwareBP->SimpleConstructionScript->GetAllNodes())
            if (auto *Mesh = Cast<UStaticMeshComponent>(Node->ComponentTemplate))
            {
                Hardware += FString::Printf(TEXT("%s\n transform=%s\n mesh=%s material=%s\n"),
                    *Node->GetVariableName().ToString(), *Mesh->GetRelativeTransform().ToString(),
                    *GetPathNameSafe(Mesh->GetStaticMesh()), *GetPathNameSafe(Mesh->GetMaterial(0)));
            }
        FFileHelper::SaveStringToFile(Hardware, *(FPaths::ProjectSavedDir() / TEXT("KioskHardwareInspection.txt")));
        UE_LOG(LogTemp, Display, TEXT("KIOSK_HARDWARE\n%s"), *Hardware);
        return 0;
    }
    if (FParse::Param(*Params, TEXT("UpdateHardware")))
        return MigrateKioskDesign(true) ? 0 : 1;
    if (FParse::Param(*Params, TEXT("MigrateDesign")) || FParse::Param(*Params, TEXT("Apply")))
        return MigrateKioskDesign() ? 0 : 1;
    if (FParse::Param(*Params, TEXT("Verify")))
        return VerifyKioskDesign(FParse::Param(*Params, TEXT("Render"))) ? 0 : 1;
    FString Out;
    const TArray<FString> Paths = {TEXT("/Game/Kiosk/UI/KioskUI"),
                                   TEXT("/Game/Kiosk/UI/InteractPrompt"),
                                   TEXT("/Game/Kiosk/Blueprint/BP_TransitKiosk"),
                                   TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"),
                                   TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"),
                                   TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode")};
    for (const auto &Path : Paths)
    {
        auto *BP = LoadObject<UBlueprint>(nullptr, *Path);
        if (!BP)
            return 1;
        Out += TEXT("\nASSET ") + Path + TEXT(" parent=") + GetPathNameSafe(BP->ParentClass) + TEXT("\n");
        if (auto *WBP = Cast<UWidgetBlueprint>(BP))
            DescribeWidget(WBP->WidgetTree->RootWidget, Out);
        if (BP->SimpleConstructionScript)
            for (auto *Node : BP->SimpleConstructionScript->GetAllNodes())
                Out += TEXT("COMPONENT ") + Node->GetVariableName().ToString() + TEXT(" : ") +
                       GetNameSafe(Node->ComponentClass) + TEXT("\n");
        TArray<UEdGraph *> Graphs;
        BP->GetAllGraphs(Graphs);
        for (auto *Graph : Graphs)
        {
            Out += TEXT("GRAPH ") + Graph->GetName() + TEXT("\n");
            for (UEdGraphNode *Node : Graph->Nodes)
            {
                Out += TEXT(" NODE ") + Node->GetName() + TEXT(" ") +
                       Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" ")) +
                       TEXT("\n");
                for (auto *Pin : Node->Pins)
                {
                    Out += TEXT("  ") + Pin->PinName.ToString() + TEXT("=") + Pin->DefaultValue;
                    for (auto *Linked : Pin->LinkedTo)
                        Out +=
                            TEXT(" -> ") + Linked->GetOwningNode()->GetName() + TEXT(".") + Linked->PinName.ToString();
                    Out += TEXT("\n");
                }
            }
        }
    }
    auto *IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default"));
    for (const auto &Mapping : IMC->GetMappings())
        Out += FString::Printf(TEXT("INPUT %s = %s\n"), *GetNameSafe(Mapping.Action), *Mapping.Key.ToString());
    UWorld *World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Kiosk/Maps/Demo"));
    if (World)
        for (TActorIterator<AActor> It(World); It; ++It)
            Out += FString::Printf(TEXT("ACTOR %s %s at %s\n"), *It->GetActorLabel(), *It->GetClass()->GetPathName(),
                                   *It->GetActorLocation().ToString());
    FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("KioskInspection.txt")),
                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("KIOSK_INSPECTION_COMPLETE"));
    return 0;
}
