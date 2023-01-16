using Nett;
using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenManager
    {
        public static bool LoadSettings(ParsingSettings parsingSettings, CodeGenSettings codeGenSettings)
        {
            string configPath = null;
            if (Configuration.ConfigPath != null)
            {
                configPath = Configuration.ConfigPath;
            }
            else
            {
                string currentPath = typeof(CodeGenManager).Assembly.Location;
                var currentDirectory = Path.GetDirectoryName(currentPath);
                if (currentDirectory != null)
                    configPath = Path.Combine(currentDirectory, "Settings.toml");
            }

            if (configPath == null || !File.Exists(configPath))
                return false;

            var toml = Toml.ReadFile(configPath);
            if (toml == null)
                return false;

            // Load parsing settings
            if (!parsingSettings.LoadFromTomlConfig(toml))
                return false;

            // Load generating settings
            if (!codeGenSettings.LoadFromTomlConfig(toml))
                return false;

            return true;
        }

        public static List<string> GetProcessedFiles(List<string> SourcePaths)
        {
            List<string> processedFiles = new List<string>();
            if (SourcePaths.Count == 0)
                return processedFiles;

            for (var i = 0; i < SourcePaths.Count; i++)
            {
                var path = SourcePaths[i];
                if (!Directory.Exists(path))
                    continue;

                var files = Directory.GetFiles(path, "*", SearchOption.AllDirectories).
                    Where(s => s.EndsWith(".h") || s.EndsWith(".hpp")).ToArray();
                if (files.Length <= 0)
                    continue;

                var count = processedFiles.Count;
                if (processedFiles.Count == 0)
                {
                    processedFiles.AddRange(files);
                }
                else
                {
                    for (int j = 0; j < files.Length; j++)
                    {
                        bool unique = true;
                        for (int k = 0; k < count; k++)
                        {
                            if (processedFiles[k] == files[j])
                            {
                                unique = false;
                                break;
                            }
                        }
                        if (unique)
                            processedFiles.Add(files[j]);
                    }
                }
            }

            return processedFiles;
        }

        public static void Clean()
        {
            using (new ProfileEventScope("Clean"))
            {
            }
        }

        public static bool CheckHeaderFileHasAPI(ParsingSettings parsingSettings, string file)
        {
            if (!File.Exists(file))
                return false;

            bool hasApi = false;
            var searchTags = parsingSettings.ApiTokens.GetSearchTags();
            string headerFileContents = File.ReadAllText(file);
            for (int j = 0; j < searchTags.Count; j++)
            {
                if (headerFileContents.Contains(searchTags[j]))
                {
                    hasApi = true;
                    break;
                }
            }
            return hasApi;
        }

        public static bool Generate()
        {
            // Setup basic objects
            FileParser fileParser = new FileParser();
            CodeGenUnit codeGenUnit = new CodeGenUnit();
            if (!LoadSettings(fileParser.Settings, codeGenUnit.Settings))
            {
                Log.Error("Failed to load settings.");
                return false;
            }

            bool ret = true;
            using (new ProfileEventScope("Generate"))
            {
                List<string> SourcePaths = new List<string>();
                SourcePaths.Add(Globals.Root);

                var files = GetProcessedFiles(SourcePaths);
                if (files.Count <= 0)
                    return false;

                // Parsing bindings
                // TODO Multi-threaded
                using (new ProfileEventScope("Parsing bindings"))
                {
                    List<string> availiableFiles = new List<string>();
                    for (int i = 0; i < files.Count; i++)
                    {
                        if (CheckHeaderFileHasAPI(fileParser.Settings, files[i]))
                        {
                            availiableFiles.Add(files[i]);
                        }
                    }
                    files = availiableFiles;
                }

                using (new ProfileEventScope("LoadCache"))
                {
                    CodeGenCache.LoadCache();

                    // Check cached files
                    List<string> availiableFiles = new List<string>();
                    foreach (var file in files)
                    {
                        if (CodeGenCache.CacheFileInfos.TryGetValue(file, out var info))
                        {
                            if (info.IsValid)
                                availiableFiles.Add(file);
                        }
                    }
                    files = availiableFiles;
                }

                if (files.Count > 0)
                {
                    // Generate predefine macros file if necessary
                    GenerateMacrosFile();

                    var taskGraph = new TaskGraph();
                    ProcessFiles(taskGraph, fileParser, codeGenUnit, files);

                    // Prepare tasks
                    using (new ProfileEventScope("ExecuteTasks"))
                    {
                        taskGraph.SortTasks();

                        // Execute tasks
                        int executedTaskCount;
                        if (!taskGraph.Execute(out executedTaskCount))
                        {
                            return false;
                        }

                        if (executedTaskCount > 0)
                        {
                            CodeGenCache.ValidCacheFileFromTasks(codeGenUnit, taskGraph.Tasks);
                        }
                    }
                }

                using (new ProfileEventScope("SaveCache"))
                {
                    CodeGenCache.SaveCache();
                }
            }
            return ret;
        }

        private static void GenerateMacrosFile()
        {
        }

        private static void ProcessFiles(TaskGraph taskGraph, FileParser fileParser, CodeGenUnit codeGenUnit, List<string> files)
        {
            foreach (var file in files)
            {
                // Parsing task
                var parsingTask = new ParsingTask(fileParser, file);
                taskGraph.Add(parsingTask);

                // Generating task
                var generationTask = new CodeGenTask(codeGenUnit);
                generationTask.DependentTasks.Add(parsingTask);
                taskGraph.Add(generationTask);
            }
        }
    }
}
