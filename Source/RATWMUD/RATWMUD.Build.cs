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
            "Json", "JsonUtilities", "HTTP", "SQLiteCore", "OnlineSubsystemUtils", "ImageWrapper"
        });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
        // The world database client (Core/RatwPg.cpp) loads the system libpq at run time.
        if (Target.Platform == UnrealTargetPlatform.Linux)
            PublicSystemLibraries.Add("dl");
        RuntimeDependencies.Add("$(ProjectDir)/Data/Cells/*.cell", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Data/Portraits/*.png", StagedFileType.NonUFS);
        // The map's Unicode terrain glyphs are drawn in the bundled DejaVu Sans Mono (see Data/Fonts).
        RuntimeDependencies.Add("$(ProjectDir)/Data/Fonts/*", StagedFileType.NonUFS);
        // Bundled Atlas worlds (e.g. Greyfen for -RatwTown): manifest and runtime cells.
        RuntimeDependencies.Add("$(ProjectDir)/Data/Worlds/...", StagedFileType.NonUFS);
    }
}
