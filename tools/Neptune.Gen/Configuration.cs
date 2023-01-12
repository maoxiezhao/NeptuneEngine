using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public static partial class Configuration
    {
        [CommandLine("workspace", "<path>", "The custom working directory.")]
        public static string CurrentDirectory = null;

        [CommandLine("output", "<path>", "The custom output directory.")]
        public static string OutputDirectory = null;

        [CommandLine("verbose", "Enables verbose logging and detailed diagnostics.")]
        public static bool Verbose = true;

        [CommandLine("clean", "Clean generated files.")]
        public static bool Clean = false;

        [CommandLine("generate", "Generate target codes.")]
        public static bool Generate = false;

        [CommandLine("mutex", "Enables using guard mutex to prevent running multiple instances of the tool.")]
        public static bool Mutex = false;

        [CommandLine("maxConcurrency", "<threads>", "The maximum allowed concurrency for a build system (maximum active worker threads count).")]
        public static int MaxConcurrency = 512;
    }
}
