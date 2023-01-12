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
        private static CodeGenSettings _settings;

        public static CodeGenSettings LoadSettings()
        {
            if (_settings != null)
                return _settings;

            using (new ProfileEventScope("LoadSettings"))
            {
                _settings = new CodeGenSettings();
            }

            return _settings;
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

        private static ParsingSettings _parsingSettings = null;
        public static ParsingSettings GetParsingSettings()
        {
            if (_parsingSettings != null)
                return _parsingSettings;

            _parsingSettings = new ParsingSettings();
            return _parsingSettings;
        }

        public static void Clean()
        { 
        }

        public static bool Generate()
        {
            var parsingSettings = GetParsingSettings();
            if (parsingSettings == null)
            {
                return false;
            }

            bool ret = true;
            using (new ProfileEventScope("Generate"))
            {
                List<string> SourcePaths = new List<string>();
                SourcePaths.Add(Globals.Root);

                var files = GetProcessedFiles(SourcePaths);
                if (files.Count <= 0)
                {
                    return false;
                }

                // Generate predefine macros file if necessary
                GenerateMacrosFile();

                FileParser fileParser = new FileParser();
                fileParser.SetParsingSettings(parsingSettings);

                var taskGraph = new TaskGraph();
                ProcessFiles(taskGraph, fileParser, files);

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
                }


            }
            return ret;
        }

        private static void GenerateMacrosFile()
        {
        }

        private static void ProcessFiles(TaskGraph taskGraph, FileParser fileParser, List<string> files)
        {
            foreach (var file in files)
            {
                // Parsing task

                // Generating task
            }
        }
    }
}
