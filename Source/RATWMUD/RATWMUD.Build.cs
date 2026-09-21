using UnrealBuildTool;
using System.IO;

public class RATWMUD : ModuleRules
{
    public RATWMUD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
            "Json", "JsonUtilities", "HTTP", "SQLiteCore", "OnlineSubsystemUtils"
        });
        RuntimeDependencies.Add("$(ProjectDir)/Data/Cells/*.cell", StagedFileType.NonUFS);
    }
}
