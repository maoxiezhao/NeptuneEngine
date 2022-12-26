using System.IO;
using Neptune.Build;

public class NeptuneGame : EngineTarget
{
    public override void Init()
    {
        base.Init();

        OutputName = "NeptuneGame";
    }

    public override void SetupTargetEnvironment(BuildOptions options)
    { 
        base.SetupTargetEnvironment(options);
    }
}