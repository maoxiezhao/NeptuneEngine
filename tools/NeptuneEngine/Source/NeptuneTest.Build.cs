using System.IO;
using System.Collections.Generic;
using Neptune.Build;

public class NeptuneTest : EngineTarget
{
    public override void Init()
    {
        base.Init();

        OutputName = "NeptuneTest";
    }

    public override void SetupTargetEnvironment(BuildOptions options)
    { 
        base.SetupTargetEnvironment(options);
    }
}