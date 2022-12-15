using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class TaskGraph
    {
        public readonly string Workspace;
        public readonly string CachePath;

        public readonly List<CmdTask> Tasks = new List<CmdTask>();

        public TaskGraph(string workspace)
        {
            Workspace = workspace;
            CachePath = Path.Combine(workspace, Configuration.IntermediateFolder, "TaskGraph.cache");
        }

        public void Setup()
        {
        }

        public bool Execute(out int executedTasksCount)
        {
            executedTasksCount = 0;
            return true;
        }

        public void LoadCache()
        { 
        }

        public void SaveCache()
        { 
        }

        public T Add<T>() where T : CmdTask, new()
        {
            var task = new T();
            Tasks.Add(task);
            return task;
        }
    }
}