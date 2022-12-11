using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;
using System.Reflection;

namespace Neptune.Build
{
    class Program
    {
        static int Main()
        {
            Stopwatch stopwatch = Stopwatch.StartNew();
            bool ret = true;
            try
            {
                // Print assembly version
                var executingAssembly = System.Reflection.Assembly.GetExecutingAssembly();
                Version version = executingAssembly.GetName().Version;
                string versionString = string.Join(".", version.Major, version.Minor, version.Build);
                Log.Info(string.Format("Neptune.Build {0}", versionString));

                CommandLine.Configure(typeof(Configuration), CommandLine.Get());
                Globals.Root = Directory.GetCurrentDirectory();
                Globals.EngineRoot = Utils.RemovePathRelativeParts(Path.Combine(Path.GetDirectoryName(executingAssembly.Location), "..", "..", "Test"));

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

                // Clean
                if (Configuration.Clean)
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
                stopwatch.Stop();
                Log.Info(string.Format("Total time: {0}", stopwatch.Elapsed));
            }

            return ret ? 0 : 1;
        }
    }
}