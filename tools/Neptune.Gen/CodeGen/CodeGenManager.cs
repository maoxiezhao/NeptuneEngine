using System;
using System.Collections.Generic;
using System.Data;
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

        public static string[] GetProcessedFiles()
        {
            string[] processedFiles = new string[0];

            return processedFiles;
        }

        public static bool Generate()
        {
            bool ret = true;
            using (new ProfileEventScope("Generate"))
            {
                var files = GetProcessedFiles();
                if (files.Length < 0)
                {
                    return false;
                }

                GenerateMacrosFile();

                var taskGraph = new TaskGraph();
                ProcessFiles(taskGraph, files);

                // Prepare tasks
                using (new ProfileEventScope("PrepareTasks"))
                {
                    taskGraph.Setup();
                    taskGraph.LoadCache();
                }

                // Execute tasks
                int executedTaskCount;
                if (!taskGraph.Execute(out executedTaskCount))
                {
                    return false;
                }

                // Save cache
                if (executedTaskCount > 0)
                {
                    taskGraph.SaveCache();
                }
            }
            return ret;
        }

        private static void GenerateMacrosFile()
        {
        }

        private static void ProcessFiles(TaskGraph taskGraph, string[] files)
        {
            foreach (var file in files)
            {
                // Parsing task

                // Generating task
            }
        }
    }
}
