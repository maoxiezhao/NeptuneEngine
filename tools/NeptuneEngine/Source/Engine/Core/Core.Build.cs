using System.IO;
using System.Collections.Generic;
using Neptune.Build;

public class Core : EngineModule
{
    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        options.PublicDependencies.Add("Platform");
    }
}
