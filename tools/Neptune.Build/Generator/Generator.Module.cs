using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Builder;

namespace Neptune.Build.Generator
{
    public enum GeneratorModuleType
    {
        Engine,
        Game
    }

    public class GeneratorModuleInfo
    {
        public GeneratorModuleType ModuleType;
        public string ModuleName;
        public string ModuleDirectory;
        public Module ModuleInst;
        public List<string> SourcePaths = new List<string>();
        public List<string> HeaderFiles = new List<string>();
        public List<string> PublicDefines = new List<string>();
        public List<string> IncludePaths = new List<string>();
        public string GeneratedCodeDirectory;
        public bool isDirty = false;
    }

    public static partial class CodeGenerator
    {
        private static GeneratorModuleType GetModuleType(Module module)
        {
            if (module is EngineModule)
                return GeneratorModuleType.Engine;
            else 
                return GeneratorModuleType.Game;
        }

        private static GeneratorModuleInfo ParseGeneratorModuleInfo(Module module, BuildOptions moduleOptions)
        {
            var moduleInfo = new GeneratorModuleInfo
            {
                ModuleType = GetModuleType(module),
                ModuleName = module.Name,
                ModuleDirectory = module.FolderPath,
                ModuleInst = module,
                SourcePaths = moduleOptions.SourcePaths,
                GeneratedCodeDirectory = moduleOptions.GeneratedFolder
            };
            if (string.IsNullOrEmpty(moduleInfo.ModuleName))
                throw new Exception("Module name cannot be empty.");

            var headerFiles = new List<string>(moduleOptions.SourceFiles.Count / 2);
            for (int i = 0; i < moduleOptions.SourceFiles.Count; i++)
            {
                if (moduleOptions.SourceFiles[i].EndsWith(".h", StringComparison.OrdinalIgnoreCase) ||
                    moduleOptions.SourceFiles[i].EndsWith(".hpp", StringComparison.OrdinalIgnoreCase))
                {
                    headerFiles.Add(moduleOptions.SourceFiles[i]);
                }
            }

            // Special case for Core module to ignore API tags defines
            if (module.Name == "Core")
            {
                for (int i = 0; i < headerFiles.Count; i++)
                {
                    if (headerFiles[i].EndsWith("config.h", StringComparison.Ordinal))
                    {
                        headerFiles.RemoveAt(i);
                        break;
                    }
                }
            }

            if (headerFiles.Count == 0)
                return moduleInfo;

            // Sort file paths to have stable results
            headerFiles.Sort();

            using (new ProfileEventScope("LoadCache"))
            {
                LoadCache(moduleInfo, moduleOptions, headerFiles);
            }

            // Check whether the given file contains reflection markup
            foreach (var headerFile in headerFiles)
            {
                if (ContainsReflectionMarkup(headerFile))
                {
                    moduleInfo.HeaderFiles.Add(headerFile);
                }
            }

            using (new ProfileEventScope("SaveCache"))
            {
                try
                {
                    SaveCache(moduleInfo, moduleOptions, moduleInfo.HeaderFiles);
                }
                catch (Exception ex)
                {
                    Log.Error($"Failed to save generating cache for module {moduleInfo.ModuleName}");
                    Log.Exception(ex);
                }
            }

            moduleInfo.PublicDefines.AddRange(moduleOptions.PublicDefinitions.ToArray());
            moduleInfo.IncludePaths.AddRange(moduleOptions.CompileEnv.IncludePaths);
            moduleInfo.isDirty = IsCurrentModuleDirty();

            return moduleInfo;
        }

        public static void SetupGeneratorModuleInfos(BuildData buildData, List<GeneratorModuleInfo> moduleInfos) 
        {
            List<Module> modulesSorted = buildData.ModulesOrderList.ToList();
            foreach (var module in modulesSorted)
            {
                if (!buildData.BinaryModules.Any(x => x.Contains(module)))
                    continue;

                BuildOptions moduleOptions = null;
                if (!buildData.Modules.TryGetValue(module, out moduleOptions))
                    continue;

                var moduleInfo = ParseGeneratorModuleInfo(module, moduleOptions);
                if (moduleInfo != null)
                {
                    // Remove any stale generated code directory
                    if (moduleInfo.HeaderFiles.Count <= 0)
                    {
                        if (moduleInfo.GeneratedCodeDirectory.Length > 0 && Directory.Exists(moduleInfo.GeneratedCodeDirectory))
                        {
                            Directory.Delete(moduleInfo.GeneratedCodeDirectory, true);
                        }
                    }
                    else
                    {
                        moduleInfos.Add(moduleInfo);
                    }
                }
            }
        }

        public static void SetupBuildEnvironment(BuildOptions buildOptions)
        {
            string generatedFolder = buildOptions.GeneratedFolder;
            if (string.IsNullOrEmpty(generatedFolder) || !Directory.Exists(generatedFolder))
                return;

            var directories = Directory.EnumerateDirectories(generatedFolder);
            foreach (var dir in directories)
            {
                buildOptions.CompileEnv.IncludePaths.Add(dir);
                buildOptions.SourcePaths.Add(dir);
            }
        }
    }
}
