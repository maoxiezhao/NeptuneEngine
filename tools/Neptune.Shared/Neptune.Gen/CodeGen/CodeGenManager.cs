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
        public static List<string> GetProcessedFiles(List<string> SourcePaths)
        {
            List<string> processedFiles = new List<string>();
            return processedFiles;
        }

        public static bool CheckHeaderFileHasAPI(ParsingSettings parsingSettings, string file)
        {
            return false;
        }

        public static bool Generate(ParsingSettings parsingSettings)
        {
            // Setup basic objects
            FileParser fileParser = new FileParser(parsingSettings);
            CodeGenUnit codeGenUnit = new CodeGenUnit();


            bool ret = true;
            using (new ProfileEventScope("Generate"))
            {
                List<string> SourcePaths = new List<string>();
                // SourcePaths.Add(Globals.Root);

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
#if false
                        if (CheckHeaderFileHasAPI(fileParser.Settings, files[i]))
                        {
                            availiableFiles.Add(files[i]);
                        }
#endif
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
                // Generating task
                // var generationTask = new CodeGenTask(codeGenUnit);
                // generationTask.DependentTasks.Add(parsingTask);
                // taskGraph.Add(generationTask);
            }
        }
    }
}
