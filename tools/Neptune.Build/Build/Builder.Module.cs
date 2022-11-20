using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    partial class Builder
    {
        public sealed class BuildData
        {
            public Dictionary<Module, BuildOptions> Modules = new Dictionary<Module, BuildOptions>(256);
        }

        /// <summary>
        /// Collects the modules required by the given target to build (includes dependencies).
        /// </summary>
        public static Dictionary<Module, BuildOptions> CollectModules(
            RulesAssembly rules,
            Platform platform,
            Target target,
            BuildOptions targetBuildOptions,
            ToolChain toolchain,
            TargetArchitecture architecture,
            TargetConfiguration configuration)
        {
            return CollectModules(
                rules,
                platform,
                target,
                targetBuildOptions,
                toolchain,
                architecture,
                configuration,
                target.Modules);
        }

        public static Dictionary<Module, BuildOptions> CollectModules(
            RulesAssembly rules,
            Platform platform,
            Target target,
            BuildOptions targetBuildOptions,
            ToolChain toolchain,
            TargetArchitecture architecture,
            TargetConfiguration configuration,
            IEnumerable<string> moduleNames)
        {
            var buildData = new BuildData();

            // Collect all modules
            foreach (var moduleName in moduleNames)
            {
            }

            return buildData.Modules;
        }

        public static BuildOptions GetBuildOptions(
            Target target, 
            Platform platform, 
            ToolChain toolchain, 
            TargetArchitecture architecture, 
            TargetConfiguration configuration, 
            string workingDirectory)
        {
            var platformName = platform.Target.ToString();
            var architectureName = architecture.ToString();
            var configurationName = configuration.ToString();
            var options = new BuildOptions
            {
                Target = target,
                Platform = platform,
                Toolchain = toolchain,
                Architecture = architecture,
                Configuration = configuration,
                IntermediateFolder = Path.Combine(workingDirectory, Configuration.IntermediateFolder, target.Name, platformName, architectureName, configurationName),
                OutputFolder = Path.Combine(workingDirectory, Configuration.BinariesFolder, target.Name, platformName, architectureName, configurationName),
                WorkingDirectory = workingDirectory,
            };
            return options;
        }
    }
}
