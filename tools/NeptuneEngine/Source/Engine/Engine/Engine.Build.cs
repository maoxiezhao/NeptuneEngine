using System.IO;
using System.Collections.Generic;
using Neptune.Build;

public class Engine : EngineModule
{
    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        options.PublicDependencies.Add("Core");
    }
}
