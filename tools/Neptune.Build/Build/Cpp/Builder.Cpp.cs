using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Builder;
using System.CodeDom.Compiler;

namespace Neptune.Build
{
    partial class Builder
    {
        public sealed class BuildData
        {
            public ProjectInfo Project;
            public TaskGraph Graph;
            public RulesAssembly Rules;
            public Target Target;
            public BuildOptions TargetOptions;
            public Platform Platform;
            public ToolChain Toolchain;
            public TargetArchitecture Architecture;
            public TargetConfiguration Configuration;
            public Dictionary<Module, BuildOptions> Modules = new Dictionary<Module, BuildOptions>(256);
            public List<Module> ModulesOrderList = new List<Module>();
            public IGrouping<string, Module>[] BinaryModules;
        }

        private static IGrouping<string, Module>[] GetBinaryModules(ProjectInfo project, Target target, Dictionary<Module, BuildOptions> buildModules)
        {
            var modules = new List<Module>();
            switch (target.LinkType)
            {
                case TargetLinkType.Monolithic:
                    modules.AddRange(buildModules.Keys);
                    break;
                case TargetLinkType.Modular:
                    {
                        var sourcePath = Path.Combine(project.ProjectFolderPath, "Source");
                        foreach (var module in buildModules.Keys)
                        {
                            if (module.FolderPath.StartsWith(sourcePath))
                                modules.Add(module);
                        }
                        break;
                    }
                default: throw new ArgumentOutOfRangeException();
            }
            modules.RemoveAll(x => x == null || string.IsNullOrEmpty(x.BinaryModuleName));
            return modules.GroupBy(x => x.BinaryModuleName).ToArray();
        }

        private static void BuildTargetReferenceNativeCpp(Dictionary<Target, BuildData> buildContext, BuildData buildData, ProjectInfo.Reference reference)
        {

        }

        public static BuildData BuildTargetNativeCpp(Dictionary<Target, BuildData> buildContext, ProjectInfo project, RulesAssembly rules, TaskGraph graph, Target target, ToolChain toolchain, TargetConfiguration configuration)
        {
            if (buildContext.TryGetValue(target, out var buildData))
                return buildData;

            if (target.Modules.Count == 0)
            {
                Log.Warning(string.Format("Target {0} has no modules to build", target.Name));
                return null;
            }

            var buildOptions = GetBuildOptions(target, toolchain.Platform, toolchain, toolchain.Architecture, configuration, project.ProjectFolderPath);
            if (buildOptions == null)
            {
                Log.Warning(string.Format("Failed to get build options in {0}", target.Name));
                return null;
            }

            // Pre build
            using (new ProfileEventScope("PreBuild"))
            {
                target.PreBuild();

                // Ensure that target build directories exist
                if (!Directory.Exists(buildOptions.IntermediateFolder))
                    Directory.CreateDirectory(buildOptions.IntermediateFolder);
                if (!Directory.Exists(buildOptions.OutputFolder))
                    Directory.CreateDirectory(buildOptions.OutputFolder);
            }

            buildData = new BuildData
            {
                Project = project,
                Graph = graph,
                Rules = rules,
                Target = target,
                TargetOptions = buildOptions,
                Platform = toolchain.Platform,
                Toolchain = toolchain,
                Architecture = toolchain.Architecture,
                Configuration = configuration,
            };
            buildContext.Add(target, buildData);

            // Firstly build all referenced projects 
            if (buildData.Target.LinkType == TargetLinkType.Modular)
            {
                foreach (var reference in buildData.Project.References)
                {
                    using (new ProfileEventScope(reference.Project.Name))
                    {
                        BuildTargetReferenceNativeCpp(buildContext, buildData, reference);
                    }
                }
            }

            // Collect all modules
            foreach (var moduleName in target.Modules)
            {
                var module = rules.GetModule(moduleName);
                if (module == null)
                {
                    Log.Warning(string.Format("Missing module {0} (or invalid name specified)", moduleName));
                    continue;
                }

                CollectModules(buildData, module);
            }

            // Setup binary modules
            using (new ProfileEventScope("SetupBinaryModules"))
            {
                // Collect all binary modules to include into target
                buildData.BinaryModules = GetBinaryModules(project, target, buildData.Modules);

                for (int i = 0; i < buildData.BinaryModules.Length; i++)
                {
                    var binaryModule = buildData.BinaryModules[i];
                    var binaryModuleNameUpper = binaryModule.Key.ToUpperInvariant();
                    foreach (var module in binaryModule)
                    {
                        if (!buildData.Modules.TryGetValue(module, out var moduleOptions))
                            continue;

                        if (buildData.Target.LinkType == TargetLinkType.Modular)
                        {
                            // Export symbols from binary module
                            moduleOptions.CompileEnv.PreprocessorDefinitions.Add(binaryModuleNameUpper + "_API=" + toolchain.DllExport);

                            foreach (var moduleName in moduleOptions.PrivateDependencies)
                            {
                                var dependencyModule = buildData.Rules.GetModule(moduleName);
                                if (dependencyModule == null || string.IsNullOrEmpty(dependencyModule.BinaryModuleName) || dependencyModule.BinaryModuleName != binaryModule.Key)
                                    continue;

                                if (!buildData.Modules.TryGetValue(dependencyModule, out var dependencyOptions))
                                    continue;

                                // Import symbols from referenced binary module
                                moduleOptions.CompileEnv.PreprocessorDefinitions.Add(dependencyModule.BinaryModuleName.ToUpperInvariant() + "_API=" + toolchain.DllImport);
                            }
                            foreach (var moduleName in moduleOptions.PublicDependencies)
                            {
                                var dependencyModule = buildData.Rules.GetModule(moduleName);
                                if (dependencyModule == null || string.IsNullOrEmpty(dependencyModule.BinaryModuleName) || dependencyModule.BinaryModuleName != binaryModule.Key)
                                    continue;

                                if (!buildData.Modules.TryGetValue(dependencyModule, out var dependencyOptions))
                                    continue;

                                // Import symbols from referenced binary module
                                moduleOptions.CompileEnv.PreprocessorDefinitions.Add(dependencyModule.BinaryModuleName.ToUpperInvariant() + "_API=" + toolchain.DllImport);
                            }
                        }
                        else
                        {
                            // Export symbols from all binary modules in the build
                            foreach (var q in buildData.BinaryModules)
                            {
                                moduleOptions.CompileEnv.PreprocessorDefinitions.Add(q.Key.ToUpperInvariant() + "_API=" + toolchain.DllExport);
                            }
                        }
                    }
                }
            }

            // Generate code for binary modules included in the target
            using (new ProfileEventScope("GenerateCodes"))
            {
                Generator.CodeGenerator.Generate(buildData);
            }

            // Build all modules
            using (new ProfileEventScope("BuildModules"))
            {
                foreach (var module in buildData.ModulesOrderList)
                {
                    if (!buildData.BinaryModules.Any(x => x.Contains(module)))
                        continue;

                    if (buildData.Modules.TryGetValue(module, out var moduleOptions))
                    {
                        using (new ProfileEventScope(module.Name))
                        {
                            Log.Info(string.Format("Building module {0}", module.Name));
                            BuildModuleInner(buildData, module, moduleOptions);
                        }
                    }

                    // Merge module into target environment
                    buildData.TargetOptions.LinkEnv.InputFiles.AddRange(moduleOptions.OutputFiles);
                    buildData.TargetOptions.DependencyFiles.AddRange(moduleOptions.DependencyFiles);
                    buildData.TargetOptions.Libraries.AddRange(moduleOptions.Libraries);
                }
            }

            // Post build
            using (new ProfileEventScope("PostBuild"))
            {
                target.PostBuild();
            }

            return buildData;
        }

        internal static void BuildModuleInner(BuildData buildData, Module module, BuildOptions moduleOptions)
        {
            // Inherit build environment from dependent modules
            foreach (var moduleName in moduleOptions.PrivateDependencies)
            {
                var dependencyModule = buildData.Rules.GetModule(moduleName);
                if (dependencyModule != null && buildData.Modules.TryGetValue(dependencyModule, out var dependencyOptions))
                {
                    moduleOptions.LinkEnv.InputFiles.AddRange(dependencyOptions.OutputFiles);
                    moduleOptions.DependencyFiles.AddRange(dependencyOptions.DependencyFiles);
                    moduleOptions.PrivateIncludePaths.AddRange(dependencyOptions.PublicIncludePaths);
                    moduleOptions.Libraries.AddRange(dependencyOptions.Libraries);
                }
            }
            foreach (var moduleName in moduleOptions.PublicDependencies)
            {
                var dependencyModule = buildData.Rules.GetModule(moduleName);
                if (dependencyModule != null && buildData.Modules.TryGetValue(dependencyModule, out var dependencyOptions))
                {
                    moduleOptions.LinkEnv.InputFiles.AddRange(dependencyOptions.OutputFiles);
                    moduleOptions.DependencyFiles.AddRange(dependencyOptions.DependencyFiles);
                    moduleOptions.PrivateIncludePaths.AddRange(dependencyOptions.PublicIncludePaths);
                    moduleOptions.Libraries.AddRange(dependencyOptions.Libraries);
                }
            }

            // Setup actual build environment
            module.SetupEnvironment(moduleOptions);
            moduleOptions.FillSourceFilesFromSourcePaths();

            // Collect all files to compile
            var cppFiles = new List<string>(moduleOptions.SourceFiles.Count / 2);
            for (int i = 0; i < moduleOptions.SourceFiles.Count; i++)
            {
                if (moduleOptions.SourceFiles[i].EndsWith(".cpp", StringComparison.OrdinalIgnoreCase))
                    cppFiles.Add(moduleOptions.SourceFiles[i]);
            }

            // Generate codes
            using (new ProfileEventScope("GenerateBindings"))
            {
                // TODO
            }

            // Compile all source files
            var compilationOutput = buildData.Toolchain.CompileCppFiles(buildData.Graph, moduleOptions, cppFiles, moduleOptions.OutputFolder);
            foreach (var e in compilationOutput.ObjectFiles)
                moduleOptions.LinkEnv.InputFiles.Add(e);

            if (buildData.Target.LinkType != TargetLinkType.Monolithic)
            {
                // Use the library includes required by this module
                moduleOptions.LinkEnv.InputLibraries.AddRange(moduleOptions.Libraries);

                // Link all object files into module library
                var outputLib = buildData.Platform.GetLinkOutputFileName(module.Name, moduleOptions.LinkEnv.Output);
                outputLib = Path.Combine(buildData.TargetOptions.OutputFolder, outputLib);
                LinkNativeBinary(buildData, moduleOptions, outputLib);

                if (moduleOptions.LinkEnv.Output == LinkerOutput.Executable || 
                    moduleOptions.LinkEnv.Output == LinkerOutput.SharedLibrary)
                {
                    moduleOptions.OutputFiles.Add(Path.ChangeExtension(outputLib, buildData.Platform.StaticLibraryFileExtension));
                }
            }
            else
            {
                // Use direct linking of the module object files into the target
                moduleOptions.OutputFiles.AddRange(compilationOutput.ObjectFiles);

                // Forward the library includes required by this module
                moduleOptions.OutputFiles.AddRange(moduleOptions.LinkEnv.InputFiles);
            }
        }

        private static void LinkNativeBinary(BuildData buildData, BuildOptions buildOptions, string outputPath)
        {
            using (new ProfileEventScope("LinkNativeBinary"))
            {
                buildData.Toolchain.LinkFiles(buildData.Graph, buildOptions, outputPath);

                // Produce additional import library if will use binary module references
                var linkerOutput = buildOptions.LinkEnv.Output;
                if (linkerOutput == LinkerOutput.Executable || linkerOutput == LinkerOutput.SharedLibrary)
                {
                    buildOptions.LinkEnv.Output = LinkerOutput.ImportLibrary;
                    buildData.Toolchain.CreateImportLib(buildData.Graph, buildOptions, Path.ChangeExtension(outputPath, buildData.Toolchain.Platform.StaticLibraryFileExtension));
                    buildOptions.LinkEnv.Output = linkerOutput;
                }
            }
        }
    }
}
