using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class TaskGraph
    {
        public readonly List<TaskBase> Tasks = new List<TaskBase>();

        public static int TaskCompare(TaskBase a, TaskBase b)
        {
            // Sort by amount of input files
            return Math.Sign(b.DependentTasksCount - a.DependentTasksCount);
        }

        public void SortTasks()
        {
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
                Log.Verbose("Done!");

            return true;
        }
    }
}
