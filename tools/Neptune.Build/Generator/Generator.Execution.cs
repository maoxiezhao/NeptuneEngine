using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Builder;

namespace Neptune.Build.Generator
{
    public static partial class CodeGenerator
    {
        public static Gen.Manifest CreateCodeGeneratorManifest(BuildData buildData, List<GeneratorModuleInfo> moduleInfos)
        {
            List<Gen.Manifest.ModuleInfo> Modules = new List<Gen.Manifest.ModuleInfo>();
            foreach (GeneratorModuleInfo module in moduleInfos)
            {
                Modules.Add(new Gen.Manifest.ModuleInfo()
                {
                    ModuleType = (Gen.ModuleType)Enum.Parse(typeof(Gen.ModuleType), module.ModuleType.ToString()),
                    ModuleName = module.ModuleName,
                    ModuleDirectory = module.ModuleDirectory,
                    SourcePaths = module.SourcePaths,
                    HeaderFiles = module.HeaderFiles,
                    PublicDefines = module.PublicDefines,
                    GeneratedCodeDirectory = module.GeneratedCodeDirectory,
                });
            }
            return new Gen.Manifest
            {
                TargetName = buildData.Target.Name,
                Modules = Modules,
            };
        }

        private static bool RunCodeGeneratorProcess(BuildData buildData, List<GeneratorModuleInfo> moduleInfos)
        {
            var manifest = CreateCodeGeneratorManifest(buildData, moduleInfos);
            if (manifest == null)
            {
                Log.Error("Failed to create code generate manifest.");
                return false;
            }

            var parsingSettings = GetParsingSettings();
            if (parsingSettings == null) 
            {
                Log.Error("Failed to create code generate parsing settings.");
                return false;
            }

            Gen.Process process = new Gen.Process(
                manifest, 
                parsingSettings, 
                false);
            process.LogInfoOutput = ProcessDebugOutput;
            return process.Run();
        }

        private static void ProcessDebugOutput(string msg)
        {
            Log.Info(msg);
        }
    }
}
