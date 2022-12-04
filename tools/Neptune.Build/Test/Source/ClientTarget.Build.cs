using System.IO;
using Neptune.Build;

public class ClientTarget : EngineTarget
{
    public override void Init()
    {
        base.Init();

        OutputName = "Client";
    }

    public override void SetupTargetEnvironment(BuildOptions options)
    { 
        base.SetupTargetEnvironment(options);
    }
}