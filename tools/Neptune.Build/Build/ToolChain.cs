using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class ToolChain
    {
        public Platform Platform { get; private set; }
        public TargetArchitecture Architecture { get; private set; }

        /// <summary>
        /// The default system include paths (for native C++ compilation).
        /// </summary>
        public readonly List<string> SystemIncludePaths = new List<string>();

        /// <summary>
        /// The default system library paths (for native C++ linking).
        /// </summary>
        public readonly List<string> SystemLibraryPaths = new List<string>();

        protected ToolChain(Platform platform, TargetArchitecture architecture)
        {
            Platform = platform;
            Architecture = architecture;
        }

        public virtual void SetupEnvironment(BuildOptions options)
        {
            options.CompileEnv.IncludePaths.AddRange(SystemIncludePaths);
            options.LinkEnv.LibraryPaths.AddRange(SystemLibraryPaths);
        }
    }
}
