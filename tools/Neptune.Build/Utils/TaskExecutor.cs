using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class TaskExecutor
    {
        private readonly object _locker = new object();
        private readonly List<CmdTask> _waitingTasks = new List<CmdTask>();
        private readonly HashSet<CmdTask> _executedTasks = new HashSet<CmdTask>();

        private int _counter;
        public int Execute(List<CmdTask> tasks)
        {
            _waitingTasks.Clear();
            _executedTasks.Clear();

            foreach (var task in tasks)
            {
                if (!task.HasValidCachedResults)
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
            var failedTasks = new List<CmdTask>();
            while (true)
            {
                CmdTask taskToRun = null;
                lock (_locker)
                {
                    // End when run out of the tasks to perform
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
                                else if (!dependentTask.HasValidCachedResults)
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
                    var result = ExecuteTask(taskToRun);
                    taskToRun.EndTime = DateTime.Now;
                    if (result != 0)
                    {
                        Log.Error(string.Format("Task {0} {1} failed with exit code {2}", taskToRun.CommandPath, taskToRun.CommandArguments, result));
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

        private int ExecuteTask(CmdTask task)
        {
            var startInfo = new ProcessStartInfo
            {
                WorkingDirectory = task.WorkingDirectory,
                FileName = task.CommandPath,
                Arguments = task.CommandArguments,
                UseShellExecute = false,
                RedirectStandardInput = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                StandardOutputEncoding = Encoding.GetEncoding("GBK"),
                StandardErrorEncoding = Encoding.GetEncoding("GBK")
            };

            if (task.InfoMessage != null)
            {
                Log.Info(task.InfoMessage);
            }

            Process process = null;
            try
            {
                try
                {
                    process = new Process();
                    process.StartInfo = startInfo;
                    process.OutputDataReceived += ProcessDebugOutput;
                    process.ErrorDataReceived += ProcessDebugOutput;
                    process.Start();

                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();
                }
                catch (Exception ex)
                {
                    Log.Error("Failed to start local process for task");
                    Log.Exception(ex);
                    return -1;
                }

                // Hang until process end
                process.WaitForExit();
                return process.ExitCode;
            }
            finally
            {
                process?.Close();
            }
        }

        private static void ProcessDebugOutput(object sender, DataReceivedEventArgs e)
        {
            string output = e.Data;
            if (output != null)
            {
                Log.Info(output);
            }
        }
    }
}
