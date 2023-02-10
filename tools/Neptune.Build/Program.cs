using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Text;

namespace Neptune.Build
{
    class Program
    {
        static int Main()
        {       
           Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

            Stopwatch stopwatch = Stopwatch.StartNew();
            bool ret = true;
            Mutex singleInstanceMutex = null;
            try
            {
                // Print assembly version
                var executingAssembly = System.Reflection.Assembly.GetExecutingAssembly();
                Version version = executingAssembly.GetName().Version;
                string versionString = string.Join(".", version.Major, version.Minor, version.Build);
                Log.Info(string.Format("Neptune.Build {0}", versionString));

                CommandLine.Configure(typeof(Configuration), CommandLine.Get());
                if (Configuration.CurrentDirectory != null)
                    Environment.CurrentDirectory = Configuration.CurrentDirectory;

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
                    
                Globals.Root = Directory.GetCurrentDirectory();
                if (Configuration.EngineDirectory != null)
                    Globals.EngineRoot = Configuration.EngineDirectory;
                else
                    Globals.EngineRoot = Utils.RemovePathRelativeParts(Path.Combine(Path.GetDirectoryName(executingAssembly.Location), "..", "..", "..", "..", "Test", "Engine"));

                Log.Info("Arguments: " + CommandLine.Get());
                Log.Info("Workspace: " + Globals.Root);
                Log.Info("Engine: " + Globals.EngineRoot);

                // Load project
                var projectFiles = Directory.GetFiles(Globals.Root, "*.nproj", SearchOption.TopDirectoryOnly);
                if (projectFiles.Length == 1)
                    Globals.Project = ProjectInfo.Load(projectFiles[0]);
                else if (projectFiles.Length > 1)
                    throw new Exception("Too many project files. Don't know which to pick.");
                else
                    Log.Warning("Missing project file.");

                // Collect all targets and modules from the workspace
                Builder.GenerateRulesAssembly();

                // Build 3rd party libs
                if (Configuration.BuildLibs)
                {
                    Log.Info("Building 3rd party libs...");
                }

                // Clean
                if (Configuration.Clean || Configuration.Rebuild)
                {
                    Log.Info("Cleaning build workspace...");
                    Builder.Clean();
                }

                // Generate projects for the targets and solution for the workspace
                if (Configuration.GenerateProject)
                {
                    Log.Info("Generating project files...");
                    Builder.GenerateProjects();
                }

                // Build targets
                if (Configuration.Build || Configuration.Rebuild)
                {
                    Log.Info("Building targets...");
                    ret |= Builder.BuildTargets();
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