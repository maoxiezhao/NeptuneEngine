using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Text;
using ClangSharp.Interop;
using Fluid;
using System.Reflection.PortableExecutable;

namespace Neptune.Gen
{
    class Program
    {
        static int Main()
        {
            Mutex singleInstanceMutex = null;
            Stopwatch stopwatch = Stopwatch.StartNew();
            bool ret = true;
            try
            {
                var executingAssembly = System.Reflection.Assembly.GetExecutingAssembly();
                Version version = executingAssembly.GetName().Version;
                string versionString = string.Join(".", version.Major, version.Minor, version.Build);
                Log.Info(string.Format("Neptune.Gen {0}", versionString));

                CommandLine.Configure(typeof(Configuration), CommandLine.Get());
                if (Configuration.CurrentDirectory != null)
                    Environment.CurrentDirectory = Configuration.CurrentDirectory;

                Globals.Root = Directory.GetCurrentDirectory();
                Globals.Output = Configuration.OutputDirectory;
                if (Configuration.OutputDirectory == null)
                {
                    Globals.Output = Globals.Root;
                }

                if (Configuration.Mutex)
                {
                    singleInstanceMutex = new Mutex(true, "Neptune.Build", out var oneInstanceMutexCreated);
                    if (!oneInstanceMutexCreated)
                    {
                        try
                        {
                            if (!singleInstanceMutex.WaitOne(0))
                            {
                                Log.Warning("Wait for another instance(s) of Neptune.Build to end...");
                                singleInstanceMutex.WaitOne();
                            }
                        }
                        catch (AbandonedMutexException)
                        {
                            // Can occur if another Neptune.Build is killed in the debugger
                        }
                        finally
                        {
                            Log.Info("Waiting done.");
                        }
                    }
                }

                Log.Info("Workspace: " + Globals.Root);
                Log.Info("Output: " + Configuration.OutputDirectory);
                Log.Info("Arguments: " + CommandLine.Get());

                // Clean codes
                if (Configuration.Clean)
                {
                    CodeGenManager.Clean();
                }

                // Generate codes
                if (Configuration.Generate)
                {
                    Log.Info("Generating codes...");
                    ret |= CodeGenManager.Generate();
                }
            }
            catch (Exception ex)
            {
                Log.Exception(ex);
                return 1;
            }
            finally
            {
                if (singleInstanceMutex != null)
                {
                    singleInstanceMutex.Dispose();
                    singleInstanceMutex = null;
                }
                stopwatch.Stop();
                Log.Info(string.Format("Total time: {0}", stopwatch.Elapsed));
            }

            return ret ? 0 : 1;
        }
    }
}