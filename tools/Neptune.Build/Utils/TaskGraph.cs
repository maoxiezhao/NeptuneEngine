using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.ComTypes;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class TaskGraph
    {
        public readonly string Workspace;
        public readonly string CachePath;

        public readonly List<CmdTask> Tasks = new List<CmdTask>();
        public readonly Dictionary<string, CmdTask> FileToProducingTaskMap = new Dictionary<string, CmdTask>();

        public TaskGraph(string workspace)
        {
            Workspace = workspace;
            CachePath = Path.Combine(workspace, Configuration.IntermediateFolder, "TaskGraph.cache");
        }

        public void Setup()
        {
            FileToProducingTaskMap.Clear();
            foreach(CmdTask task in Tasks)
            {
                for (int j = 0; j < task.ProducedFiles.Count; j++)
                {
                    FileToProducingTaskMap[task.ProducedFiles[j]] = task;
                }
            }
        }

        public static int TaskCompare(CmdTask a, CmdTask b)
        {
            // Sort by total number of dependent files
            var dependentTasksCountDiff = b.DependentTasksCount - a.DependentTasksCount;
            if (dependentTasksCountDiff != 0)
            {
                return Math.Sign(dependentTasksCountDiff);
            }

            // Sort by the cost
            var costDiff = b.Cost - a.Cost;
            if (costDiff != 0)
            {
                return Math.Sign(costDiff);
            }

            // Sort by amount of input files
            return Math.Sign(b.PrerequisiteFiles.Count - a.PrerequisiteFiles.Count);
        }

        public void SortTasks()
        {
            var taskToDependentActionsMap = new Dictionary<CmdTask, HashSet<CmdTask>>();
            const int maximumSearchDepth = 6;
            for (int depth = 0; depth < maximumSearchDepth; depth++)
            {
                foreach (var task in Tasks)
                {
                    foreach (var prerequisiteFile in task.PrerequisiteFiles)
                    {
                        if (FileToProducingTaskMap.TryGetValue(prerequisiteFile, out var prerequisiteTask))
                        {
                            HashSet<CmdTask> dependentTasks;
                            if (!taskToDependentActionsMap.TryGetValue(prerequisiteTask, out dependentTasks))
                            {
                                dependentTasks = new HashSet<CmdTask>();
                                taskToDependentActionsMap[prerequisiteTask] = dependentTasks;
                            }

                            // Build dependencies list
                            dependentTasks.Add(task);
                            if (taskToDependentActionsMap.ContainsKey(task))
                            {
                                dependentTasks.UnionWith(taskToDependentActionsMap[task]);
                            }
                        }
                    }
                }
            }

            // Assign the dependencies
            foreach (var e in taskToDependentActionsMap)
            {
                foreach (var c in e.Value)
                {
                    if (c.DependentTasks == null)
                        c.DependentTasks = new HashSet<CmdTask>();

                    c.DependentTasks.Add(e.Key);
                    // Remove any reference to itself (prevent deadlocks)
                    c.DependentTasks.Remove(c);
                }
            }

            Tasks.Sort(TaskCompare);
        }

        public bool Execute(out int executedTasksCount)
        {
            Log.Verbose(string.Format("Total {0} tasks", Tasks.Count));

            var executor = new TaskExecutor();
            executedTasksCount = executor.Execute(Tasks);

            var failedCount = 0;
            foreach (var task in Tasks)
            {
                if (task.Failed)
                    failedCount++;
            }

            if (failedCount == 1)
                Log.Error("1 task failed");
            else if (failedCount != 0)
                Log.Error(string.Format("{0} tasks failed", failedCount));
            else
                Log.Info("Done!");

            Log.Info("");
            return failedCount == 0;
        }

        public readonly int CacheVersion = 1;
        private struct BuildResultCache
        {
            public string CmdLine;
            public int[]  FileIndices;
        }
        private readonly List<BuildResultCache> _prevBuildCache = new List<BuildResultCache>();
        private readonly List<string> _prevBuildCacheFiles = new List<string>();

        private struct CacheFileInfo
        {
            public DateTime Dates;
            public bool IsValid;
        }
        public void LoadCache()
        {
            var path = CachePath;
            if (!File.Exists(path))
            {
                Log.Verbose("Missing Task Graph cache file");
                return;
            }

            try
            {
                // Builds tasks cache
                var tasksCache = new Dictionary<string, CmdTask>();
                foreach (var task in Tasks)
                {
                    var cmdLine = task.WorkingDirectory + task.CommandPath + task.CommandArguments;
                    tasksCache[cmdLine] = task;
                }

                var validCount = 0;
                var invalidCount = 0;
                using (var stream = new FileStream(CachePath, FileMode.Open))
                using (var reader = new BinaryReader(stream))
                {
                    int version = reader.ReadInt32();
                    if (version != CacheVersion)
                    {
                        Log.Error("Unsupported cache version ");
                        return;
                    }
                    
                    // Paths
                    int pathsCount = reader.ReadInt32();
                    _prevBuildCacheFiles.Capacity = pathsCount;
                    var cacheFileInfos = new CacheFileInfo[pathsCount];
                    for (int i = 0; i < pathsCount; i++)
                    {
                        // Path
                        var file = reader.ReadString();
                        // Last File Write
                        var lastWrite = new DateTime(reader.ReadInt64());

                        var isValid = true;
                        if (File.Exists(file))
                        {
                            if (File.GetLastWriteTime(file) > lastWrite)
                            {
                                isValid = false;
                            }
                        }
                        else if (lastWrite != DateTime.MinValue)
                        {
                            isValid = false;
                        }
        
                        cacheFileInfos[i] = new CacheFileInfo
                        {
                            Dates = lastWrite,
                            IsValid = isValid
                        };
                        _prevBuildCacheFiles.Add(file);
                    }

                    // Tasks
                    var fileIndices = new List<int>();
                    int tasksCount = reader.ReadInt32();
                    for (int i = 0; i < tasksCount; i++)
                    {
                        // Working Dir + Command + Arguments
                        var cmdLine = reader.ReadString();

                        // File Indices
                        var filesCount = reader.ReadInt32();
                        bool allValid = true;
                        for (int j = 0; j < filesCount; j++)
                        {
                            var fileIndex = reader.ReadInt32();
                            allValid &= cacheFileInfos[fileIndex].IsValid;
                            fileIndices.Add(fileIndex);
                        }

                        if (allValid)
                        {
                            if (tasksCache.TryGetValue(cmdLine, out var task))
                            {
                                task.HasValidCachedResults = true;
                            }
                            else
                            {
                                _prevBuildCache.Add(new BuildResultCache
                                {
                                    CmdLine = cmdLine,
                                    FileIndices = fileIndices.ToArray(),
                                });
                            }

                            validCount++;
                        }
                        else
                        {
                            invalidCount++;
                        }
                    }
                }

                // Validate all cached results
                foreach (var task in Tasks)
                {
                    if (task.HasValidCachedResults && task.DependentTasksCount > 0)
                    {
                        // Allow task to use cached results only if all dependency tasks are also cached
                        if (!ValidateCachedResults(task))
                            task.HasValidCachedResults = false;
                    }
                }

                Log.Info(string.Format("Found {0} valid and {1} invalid actions in Task Graph cache", validCount, invalidCount));
            }
            catch(Exception ex)
            {
                Log.Exception(ex);
                Log.Error("Failed to load task graph cache.");
                Log.Error(path);
                CleanCache();
            }
        }

        public void CleanCache()
        {
            var path = CachePath;
            if (File.Exists(path))
            {
                Log.Info("Clean cache: " + path);
                File.Delete(path);
            }
        }

        public void SaveCache()
        {
            var files = new List<string>();

            var fileIndices = new List<int>();
            var doneTasks = Tasks.Where(task => task.Result == 0).ToArray();
            foreach (var task in doneTasks)
            {
                files.Clear();
                files.AddRange(task.ProducedFiles);
                files.AddRange(task.PrerequisiteFiles);
            
                fileIndices.Clear();
                foreach (var file in files)
                {
                    int fileIndex = _prevBuildCacheFiles.IndexOf(file);
                    if (fileIndex == -1)
                    {
                        fileIndex = _prevBuildCacheFiles.Count;
                        _prevBuildCacheFiles.Add(file);
                    }

                    fileIndices.Add(fileIndex);
                }

                _prevBuildCache.Add(new BuildResultCache
                {
                    CmdLine = task.WorkingDirectory + task.CommandPath + task.CommandArguments,
                    FileIndices = fileIndices.ToArray(),
                });
            }

            using (var stream = new FileStream(CachePath, FileMode.Create))
            using (var writer = new BinaryWriter(stream))
            {
                // Version
                writer.Write(CacheVersion);

                // TODO: Need to clear _prevBuildCacheFiles?
                // Paths
                writer.Write(_prevBuildCacheFiles.Count);
                foreach (var file in _prevBuildCacheFiles)
                {
                    // Path
                    writer.Write(file);

                    // Last File Write
                    DateTime lastWrite;
                    if (File.Exists(file))
                        lastWrite = File.GetLastWriteTime(file);
                    else
                        lastWrite = DateTime.MinValue;
                    writer.Write(lastWrite.Ticks);
                }

                // Tasks
                writer.Write(_prevBuildCache.Count);
                foreach (var cache in _prevBuildCache)
                {
                    // Working Dir + Command + Arguments
                    writer.Write(cache.CmdLine);

                    // Files
                    writer.Write(cache.FileIndices.Length);
                    for (var i = 0; i < cache.FileIndices.Length; i++)
                    {
                        writer.Write(cache.FileIndices[i]);
                    }
                }
            }
        }

        private static bool ValidateCachedResults(CmdTask task)
        {
            foreach(var dependentTask in task.DependentTasks)
            {
                if (!dependentTask.HasValidCachedResults)
                    return false;

                if (dependentTask.DependentTasksCount > 0 && !ValidateCachedResults(dependentTask))
                    return false;
            }
            return true;
        }

        public T Add<T>() where T : CmdTask, new()
        {
            var task = new T();
            Tasks.Add(task);
            return task;
        }
    }
}