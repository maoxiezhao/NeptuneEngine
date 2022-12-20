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
        public readonly int CacheVersion = 1;
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
            var dependentTasksCountDiff = b.DependentTasks.Count - a.DependentTasks.Count;
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
            Log.Verbose("");
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

            return failedCount != 0;
        }

        public void LoadCache()
        {
            var path = CachePath;
            if (!File.Exists(path))
            {
                Log.Verbose("Missing Task Graph cache file");
                return;
            }
        }

        private struct BuildResultCache
        {
            public string CmdLine;
            public int[] FileIndices;
        }
        private readonly List<BuildResultCache> _prevBuildCache = new List<BuildResultCache>();

        private readonly List<string> _prevBuildCacheFiles = new List<string>();
        public void SaveCache()
        {
            var files = new List<string>();

            var doneTasks = Tasks.Where(task => task.Result == 0).ToArray();
            foreach (var task in doneTasks)
            {
                files.Clear();
                files.AddRange(task.ProducedFiles);
                files.AddRange(task.PrerequisiteFiles);
            }

            using (var stream = new FileStream(CachePath, FileMode.Create))
            using (var writer = new BinaryWriter(stream))
            {
                // Version
                writer.Write(CacheVersion);

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

        public T Add<T>() where T : CmdTask, new()
        {
            var task = new T();
            Tasks.Add(task);
            return task;
        }
    }
}