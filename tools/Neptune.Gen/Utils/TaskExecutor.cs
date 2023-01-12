using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class TaskExecutor
    {
        private readonly List<TaskBase> _waitingTasks = new List<TaskBase>();
        private readonly HashSet<TaskBase> _executedTasks = new HashSet<TaskBase>();

        private readonly object _locker = new object();
        private int _counter;
        public int Execute(List<TaskBase> tasks)
        {
            _waitingTasks.Clear();
            _executedTasks.Clear();

            foreach (var task in tasks)
            {
                _waitingTasks.Add(task);
            }

            int count = _waitingTasks.Count;
            if (count == 0)
                return 0;
            _counter = 0;

            int threadsCount = Math.Min(Math.Max(1, Math.Min(Configuration.MaxConcurrency, Environment.ProcessorCount)), count);
            Log.Info(string.Format("Executing {0} {2} using {1} {3}", count, threadsCount, count == 1 ? "task" : "tasks", threadsCount == 1 ? "thread" : "threads"));

            var threads = new Thread[threadsCount];
            for (int threadIndex = 0; threadIndex < threadsCount; threadIndex++)
            {
                threads[threadIndex] = new Thread(ThreadMain) { Name = "Local Executor " + threadIndex, };
                threads[threadIndex].Start(threadIndex);
            }

            for (int threadIndex = 0; threadIndex < threadsCount; threadIndex++)
            {
                threads[threadIndex].Join();
            }

            return _counter;
        }

        private void ThreadMain(object obj)
        {
            var failedTasks = new List<TaskBase>();
            while (true)
            {
                TaskBase taskToRun = null;
                lock (_locker)
                {
                    if (_waitingTasks.Count == 0)
                        break;

                    foreach (var task in _waitingTasks)
                    {
                        bool hasCompletedDependencies = true;
                        bool hasAnyDependencyFailed = false;
                        if (task.DependentTasks != null)
                        {
                            foreach (var dependentTask in task.DependentTasks)
                            {
                                if (_executedTasks.Contains(dependentTask))
                                {
                                    if (dependentTask.Failed)
                                    {
                                        hasAnyDependencyFailed = true;
                                        break;
                                    }
                                }
                                else
                                {
                                    // Need to execute dependency task before this one
                                    hasCompletedDependencies = false;
                                    break;
                                }

                            }
                        }

                        if (hasAnyDependencyFailed)
                        {
                            failedTasks.Add(task);
                        }
                        else if (hasCompletedDependencies)
                        {
                            taskToRun = task;
                            break;
                        }
                    }

                    foreach (var task in failedTasks)
                    {
                        _waitingTasks.Remove(task);
                        task.Result = -1;
                        _executedTasks.Add(task);
                    }
                    failedTasks.Clear();

                    if (taskToRun != null)
                    {
                        _waitingTasks.Remove(taskToRun);
                    }
                }

                if (taskToRun != null)
                {
                    taskToRun.StartTime = DateTime.Now;
                    var result = taskToRun.Run();
                    taskToRun.EndTime = DateTime.Now;
                    if (result != 0)
                    {
                        Log.Error(string.Format("Task failed with exit code {2}", result));
                        Log.Error(taskToRun.GetErrorMsg());
                        Log.Error("");
                    }

                    lock (_locker)
                    {
                        taskToRun.Result = result;
                        _executedTasks.Add(taskToRun);
                        _counter++;
                    }
                }
                else
                {
                    Thread.Sleep(10);
                }
            }
        }
    }
}
