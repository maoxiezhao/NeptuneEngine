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
            var buildData = new BuildData
            {
                Rules = rules,
                Target = target,
                TargetOptions = targetBuildOptions,
                Platform = platform,
                Toolchain = toolchain,
                Architecture = architecture,
                Configuration = configuration,
            };

            // Collect all modules
            foreach (var moduleName in moduleNames)
            {
                var module = rules.GetModule(moduleName);
                if (module == null)
                {
                    Log.Warning(string.Format("Missing module {0} (or invalid name specified)", moduleName));
                    continue;
                }
                CollectModules(buildData, module);
            }

            return buildData.Modules;
        }

        private static BuildOptions CollectModules(BuildData buildData, Module module)
        {
            if (buildData.Modules.TryGetValue(module, out var moduleOptions))
            {
                return moduleOptions;
            }

            var outputPath = Path.Combine(buildData.TargetOptions.IntermediateFolder, module.Name);
            if (!Directory.Exists(outputPath))
                Directory.CreateDirectory(outputPath);

            var generatedPath = Path.Combine(buildData.TargetOptions.GeneratedFolder, module.Name);
            if (!Directory.Exists(generatedPath))
                Directory.CreateDirectory(generatedPath);

            moduleOptions = new BuildOptions
            {
                Target = buildData.Target,
                Platform = buildData.Platform,
                Toolchain = buildData.Toolchain,
                Architecture = buildData.Architecture,
                Configuration = buildData.Configuration,
                IntermediateFolder = outputPath,
                GeneratedFolder = generatedPath,
                OutputFolder = outputPath,
                WorkingDirectory = buildData.TargetOptions.WorkingDirectory,
                CompileEnv = (CompileEnv)buildData.TargetOptions.CompileEnv.Clone(),
                LinkEnv = (LinkEnv)buildData.TargetOptions.LinkEnv.Clone(),
            };
            moduleOptions.SourcePaths.Add(module.FolderPath);
            module.Setup(moduleOptions);
            moduleOptions.FillSourceFilesFromSourcePaths();

            // Collect dependent modules (private)
            foreach (var moduleName in moduleOptions.PrivateDependencies)
            {
                var dependencyModule = buildData.Rules.GetModule(moduleName);
                if (dependencyModule == null)
                {
                    Log.Warning(string.Format("Missing module {0} referenced by module {1} (or invalid name specified)", moduleName, module.Name));
                    continue;
                }

                var options = CollectModules(buildData, dependencyModule);
                foreach (var e in options.PublicDefinitions)
                    moduleOptions.PrivateDefinitions.Add(e);
            }

            // Collect dependent modules (public)
            foreach (var moduleName in moduleOptions.PublicDependencies)
            {
                var dependencyModule = buildData.Rules.GetModule(moduleName);
                if (dependencyModule == null)
                {
                    Log.Warning(string.Format("Missing module {0} referenced by module {1} (or invalid name specified)", moduleName, module.Name));
                    continue;
                }

                var options = CollectModules(buildData, dependencyModule);
                foreach (var e in options.PublicDefinitions)
                    moduleOptions.PublicDefinitions.Add(e);
            }

            buildData.Modules.Add(module, moduleOptions);
            buildData.ModulesOrderList.Add(module);

            return moduleOptions;
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
                GeneratedFolder = Path.Combine(workingDirectory, Configuration.IntermediateFolder, target.Name, platformName, architectureName, "Generated"),
                OutputFolder = Path.Combine(workingDirectory, Configuration.BinariesFolder, target.Name, platformName, architectureName, configurationName),
                WorkingDirectory = workingDirectory,
                CompileEnv = new CompileEnv(),
                LinkEnv = new LinkEnv(),
            };
            toolchain?.SetupEnvironment(options);
            target.SetupTargetEnvironment(options);
            return options;
        }
    }
}
