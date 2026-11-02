using UnrealBuildTool;

public class TransitEditor : ModuleRules
{
    public TransitEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Transit", "UnrealEd", "UMG", "UMGEditor",
            "Slate", "SlateCore", "SlateRHIRenderer", "Kismet", "KismetCompiler", "BlueprintGraph", "EnhancedInput", "InputCore", "RenderCore"
        });
    }
}
