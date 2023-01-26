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

        public void Add<T>(T task) where T : TaskBase
        {
            Tasks.Add(task);
        }

        public static int TaskCompare(TaskBase a, TaskBase b)
        {
            // Sort by amount of input files
            return Math.Sign(b.DependentTasksCount - a.DependentTasksCount);
        }

        public void SortTasks()
        {
            Tasks.Sort(TaskCompare);
        }

        public bool Execute(out int executedTasksCount, bool noWide = false)
        {
            if (noWide)
            {
                executedTasksCount = 0;
                foreach (var task in Tasks)
                {
                    if (task.Run() == 0)
                        executedTasksCount++;
                }
            }
            else
            {
                var executor = new TaskExecutor();
                executedTasksCount = executor.Execute(Tasks);
            }

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

            return true;
        }
    }
}
