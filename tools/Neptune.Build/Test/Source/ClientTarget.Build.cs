using System.IO;
using Neptune.Build;

public class FlaxEditor : EngineTarget
{
    public override void Init()
    {
        base.Init();

        OutputName = "Client";
        Modules.Add("Core");
    }

    public override void SetupTargetEnvironment(BuildOptions options)
    { 
        base.SetupTargetEnvironment(options);
    }
}