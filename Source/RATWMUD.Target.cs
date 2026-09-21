using UnrealBuildTool;
using System.Collections.Generic;

public class RATWMUDTarget : TargetRules
{
    public RATWMUDTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RATWMUD");
    }
}
