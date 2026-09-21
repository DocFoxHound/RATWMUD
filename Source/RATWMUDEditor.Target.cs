using UnrealBuildTool;
using System.Collections.Generic;

public class RATWMUDEditorTarget : TargetRules
{
    public RATWMUDEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RATWMUD");
    }
}
