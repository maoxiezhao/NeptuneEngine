using System.IO;
using System.Collections.Generic;
using Neptune.Build;

public class Platform : EngineModule
{
    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        options.SourcePaths.Clear();
        options.SourceFiles.AddRange(Directory.GetFiles(FolderPath, "*.*", SearchOption.TopDirectoryOnly));
        options.SourcePaths.Add(Path.Combine(FolderPath, "Base"));

        switch (options.Platform.Target)
        {
        case TargetPlatform.Windows:
            options.SourcePaths.Add(Path.Combine(FolderPath, "Windows"));
            break;
        default: 
            throw new InvalidPlatformException(options.Platform.Target);
        }
    }
}
