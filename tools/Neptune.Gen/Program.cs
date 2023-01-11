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

                Log.Info("Workspace: " + Globals.Root);
                Log.Info("Output: " + Configuration.OutputDirectory);
                Log.Info("Arguments: " + CommandLine.Get());

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
                stopwatch.Stop();
                Log.Info(string.Format("Total time: {0}", stopwatch.Elapsed));
            }

            return ret ? 0 : 1;
        }
    }
}