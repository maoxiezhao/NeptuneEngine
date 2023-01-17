using Neptune.Gen;
using Nett;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Builder;

namespace Neptune.Build.Generator
{
    public static partial class CodeGenerator
    {
        private static bool AreGeneratedCodeFilesOutOfDate(BuildData buildData, List<GeneratorModuleInfo> moduleInfos)
        {
            DateTime toolTimestamp = File.GetLastWriteTime(Assembly.GetExecutingAssembly().Location);
            foreach (var moduleInfo in moduleInfos)
            {
                if (moduleInfo.isDirty)
                    return true;

                if (!Directory.Exists(moduleInfo.GeneratedCodeDirectory))
                    return true;

                string TimestampFile = Path.Combine(moduleInfo.GeneratedCodeDirectory, @"Timestamp");
                if (!File.Exists(TimestampFile))
                    return true;

                DateTime savedTimestamp = File.GetLastWriteTime(TimestampFile);
                if (toolTimestamp > savedTimestamp)
                    return true;

                string[] savedHeaderFiles = File.ReadAllLines(TimestampFile);
                if (moduleInfo.HeaderFiles.Count != savedHeaderFiles.Length)
                    return true;

                HashSet<string> currentHeadersSet = new HashSet<string>(moduleInfo.HeaderFiles);
                foreach (string headerFile in savedHeaderFiles)
                {
                    if (!currentHeadersSet.Contains(headerFile))
                        return true;
                }

                foreach (string headerFile in moduleInfo.HeaderFiles)
                {
                    if (File.GetLastWriteTime(headerFile) > savedTimestamp)
                       return true;
                }

            }
            return false;
        }

        public static void UpdateTimestampes(List<GeneratorModuleInfo> moduleInfos)
        {
            foreach (var moduleInfo in moduleInfos)
            {
                if (!moduleInfo.isDirty)
                    continue;

                if (!Directory.Exists(moduleInfo.GeneratedCodeDirectory))
                    continue;

                string TimestampFile = Path.Combine(moduleInfo.GeneratedCodeDirectory, @"Timestamp");
                File.WriteAllLines(TimestampFile, moduleInfo.HeaderFiles);
            }
        }

        public static void GenerateIfNecessary(BuildData buildData)
        {
            List<GeneratorModuleInfo> moduleInfos = new List<GeneratorModuleInfo>();
            using (new ProfileEventScope("Setup"))
            {
                SetupGeneratorModuleInfos(buildData, moduleInfos);
            }

            if (moduleInfos.Count > 0) 
            {
                using (new ProfileEventScope("Generate"))
                {
                    bool needsToRun = false;
                    if (Configuration.ForceGenerate)
                    {
                        needsToRun = true;
                    }
                    else if (AreGeneratedCodeFilesOutOfDate(buildData, moduleInfos))
                    {
                        needsToRun = true;
                    }

                    if (needsToRun)
                    {
                        // Write module infos for generator
                        Log.Info("Running CodeGenerator...");
                        Stopwatch stopwatch = new Stopwatch();
                        using (new ProfileEventScope("GenerateProcess"))
                        {
                            stopwatch.Start();
                            bool ret = RunCodeGeneratorProcess(buildData, moduleInfos);
                            stopwatch.Stop();
                            if (ret == false)
                            {
                                Log.Error(string.Format("Failed to generate code with exit code {0}", ret));
                                throw new Exception("Failed to generate codes.");
                            }
                        }

                        // Update timestamps
                        UpdateTimestampes(moduleInfos);

                        Log.Info(string.Format("Code generated in total time: {0}", stopwatch.Elapsed));
                    }
                    else
                    {
                        Log.Info("Generated code is up to date.");
                    }
                }
            }
        }
    }
}
