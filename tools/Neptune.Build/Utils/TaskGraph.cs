using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Neptune.Build
{
    public class TaskGraph
    {
        public readonly string Workspace;
        public readonly string CachePath;

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
    }
}