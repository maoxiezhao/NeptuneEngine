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

        [CommandLine("generate", "Generate target codes.")]
        public static bool Generate = false;
    }
}
