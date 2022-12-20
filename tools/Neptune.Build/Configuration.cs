using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public static partial class Configuration
    {
        /// <summary>
        /// Enables verbose logging and detailed diagnostics.
        /// </summary>
        [CommandLine("verbose", "Enables verbose logging and detailed diagnostics.")]
        public static bool Verbose = true;

        /// <summary>
        /// The intermediate build files folder path relative to the working directory.
        /// </summary>
        [CommandLine("intermediate", "<path>")]
        public static string IntermediateFolder = "Cache/Intermediate";

        /// <summary>
        /// The output binaries folder path relative to the working directory.
        /// </summary>
        [CommandLine("binaries", "<path>", "The output binaries folder path relative to the working directory.")]
        public static string BinariesFolder = "Bin";

        /// <summary>
        /// Cleans all the targets and whole build cache data.
        /// </summary>
        [CommandLine("clean", "Cleans the build system cache.")]
        public static bool Clean = false;

        /// <summary>
        /// Generates the projects for the workspace.
        /// </summary>
        [CommandLine("genproject", "Generates the projects for the workspace.")]
        public static bool GenerateProject = false;

        /// <summary>
        /// Builds the targets. Builds all the targets, use <see cref="BuildTargets"/> to select a custom set of targets for the build.
        /// </summary>
        [CommandLine("build", "Builds the targets.")]
        public static bool Build = false;

        [CommandLine("Rebuild", "Rebuilds the targets.")]
        public static bool Rebuild = false;

        /// <summary>
        /// The set of targets to build.
        /// </summary>
        [CommandLine("buildtargets", "<target1>,<target2>,<target3>...", "The set of targets to build.")]
        public static string[] BuildTargets;

        /// <summary>
        /// The target configuration to build. If not specified builds all supported configurations.
        /// </summary>
        [CommandLine("configuration", "Debug", "The target configuration to build. If not specified builds all supported configurations.")]
        public static TargetConfiguration[] BuildConfigurations;

        /// <summary>
        /// The target platform to build. If not specified builds all supported platforms.
        /// </summary>
        [CommandLine("platform", "Windows", "The target platform to build. If not specified builds all supported platforms.")]
        public static TargetPlatform[] BuildPlatforms;

        /// <summary>
        /// The target platform architecture to build. If not specified builds all valid architectures.
        /// </summary>
        [CommandLine("arch", "<x64/x86/arm/arm64>", "The target platform architecture to build. If not specified builds all valid architectures.")]
        public static TargetArchitecture[] BuildArchitectures;

        /// <summary>
        /// Generates Visual Studio 2022 project format files. Valid only with -genproject option.
        /// </summary>
        [CommandLine("vs2022", "Generates Visual Studio 2022 project.")]
        public static bool ProjectVS2022 = false;

        /// <summary>
        /// The maximum allowed concurrency for a build system (maximum active worker threads count).
        /// </summary>
        [CommandLine("maxConcurrency", "<threads>", "The maximum allowed concurrency for a build system (maximum active worker threads count).")]
        public static int MaxConcurrency = 512;
    }
}
