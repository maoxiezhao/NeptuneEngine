using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    [AttributeUsage(AttributeTargets.Method)]
    public class CodeGenAttribute : Attribute
    {
        public string Name;
        public string Description;
    }

    public class CodeGenTask : TaskBase
    {
        private CodeGenUnit _codeGenUnit;
        private HeaderFile _headerFile;

        public CodeGenTask(CodeGenUnit codeGenUnit, HeaderFile headerFile)
        {
            _codeGenUnit = codeGenUnit;
            _headerFile = headerFile;
        }

        public override int Run()
        {
            _codeGenUnit.GenerateCode(_headerFile);
            return 0;
        }
    }

    public class CodeGenFactory
    {
        public struct HeaderInfo
        {
            public CodeGenTask? Task { get; set; }
        }
        public HeaderInfo[] HeaderInfos { get; set; }
        public List<CodeGenResult> GenResults = new List<CodeGenResult>();

        private Process _process;
        private CodeGenExporter _codeGenExporter;


        public CodeGenFactory(Process process, CodeGenExporter codeGenExporter)
        {
            _process = process;
            _codeGenExporter = codeGenExporter;
            HeaderInfos = new HeaderInfo[process.HeaderFileIDGenerator.Count];
        }

        public string GetGeneratedFilePath(HeaderFile headerFile, string filename)
        {
            var module = headerFile.ModuleInfo;
            if (module == null)
                return string.Empty;

            return Path.Combine(module.GeneratedCodeDirectory, filename);
        }

        public void WriteFileIfChanged(string path, string contents)
        {
            bool changed = true;
            if (File.Exists(path))
            {
                string oldContents = null;
                try
                {
                    oldContents = File.ReadAllText(path);
                }
                catch (Exception)
                {
                    Log.Warning(string.Format("Failed to read file contents while trying to save it.", path));
                }

                // No files changed 
                if (string.Equals(contents, oldContents, StringComparison.OrdinalIgnoreCase))
                    changed = false;
            }

            string tempPath = path + ".tmp";
            if (changed)
            {
                Utils.WriteFile(tempPath, contents);
            }

            lock (GenResults)
            {
                GenResults.Add(new CodeGenResult {
                    FilePath = path,
                    TempFilePath = tempPath,
                    Completed = changed
                });
            }
        }

        private void CleanupGeneratedDirectory(string genDirectory, HashSet<string> generatedFiles)
        {
            foreach (string filePath in Directory.EnumerateFiles(genDirectory))
            {
                string fileName = Path.GetFileName(filePath);
                if (generatedFiles.Contains(fileName))
                    continue;

                File.Delete(Path.Combine(genDirectory, filePath));
            }
        }

        public void Run()
        {
            var taskGraph = new TaskGraph();
            foreach (var headerFile in _process.SortedHeaderFiles)
            {
                var codeGenUnit = _codeGenExporter.Delegate.Invoke(this);
                if (codeGenUnit == null)
                    throw new Exception("Failed to create codeGenUnit " + _codeGenExporter.Name);

                var parsingResult = headerFile.FileParsingResult;
                if (parsingResult.Errors.Count > 0)
                    continue;

                var task = new CodeGenTask(codeGenUnit, headerFile);
                foreach (var referenced in headerFile.ReferencedHeadersNoLock)
                {
                    if (referenced != headerFile && HeaderInfos[referenced.FileID].Task != null)
                        task.DependentTasks.Add(HeaderInfos[referenced.FileID].Task);
                }

                HeaderInfos[headerFile.FileID].Task = task;
                taskGraph.Add(task);
            }

            if (taskGraph.Tasks.Count > 0)
            {
                using (new ProfileEventScope("ExecuteTasks"))
                {
                    taskGraph.SortTasks();

                    // Execute tasks
                    int executedTaskCount;
                    if (!taskGraph.Execute(out executedTaskCount, true))
                    {
                        throw new Exception("Failed to generate codes from header files");
                    }
                }

                if (GenResults.Count > 0)
                {
                    using (new ProfileEventScope("ProcessGenResults"))
                    {
                        Dictionary<string, HashSet<string>> filesInDirectory = new(StringComparer.OrdinalIgnoreCase);
                        List<CodeGenResult> savedResults = new List<CodeGenResult>();
                        foreach (var result in GenResults)
                        {
                            string? fileDirectory = Path.GetDirectoryName(result.FilePath);
                            if (fileDirectory != null)
                            {
                                if (!filesInDirectory.TryGetValue(fileDirectory, out HashSet<string>? files))
                                {
                                    files = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                                    filesInDirectory.Add(fileDirectory, files);
                                }
                                files.Add(Path.GetFileName(result.FilePath));
                            }

                            if (result.Completed)
                                savedResults.Add(result);
                        }

                        // Create or rename generated files
                        foreach (var result in savedResults)
                        {
                            Utils.RenameFile(result.TempFilePath, result.FilePath);
                            Log.Info(string.Format("Saved generated file to {0}", result.FilePath));
                        }

                        // Flush and clear generated directory
                        foreach (KeyValuePair<string, HashSet<string>> kvp in filesInDirectory)
                            CleanupGeneratedDirectory(kvp.Key, kvp.Value);
                    }
                }
            }
        }
    }
}
