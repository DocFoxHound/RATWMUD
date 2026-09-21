using UnrealBuildTool;
using System.Collections.Generic;

public class RATWMUDServerTarget : TargetRules
{
    public RATWMUDServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RATWMUD");
    }
}
