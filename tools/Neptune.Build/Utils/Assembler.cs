using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace Neptune.Build
{
    /// <summary>
    /// The tool for building C# assemblies from custom set of source files.
    /// </summary>
    public class Assembler
    {
        /// <summary>
        /// The source files for compilation.
        /// </summary>
        public readonly List<string> SourceFiles = new List<string>();

        public static readonly Assembly[] DefaultReferences =
{
            typeof(Enumerable).Assembly, // System.Linq.dll
            typeof(ISet<>).Assembly,     // System.dll
            typeof(Builder).Assembly,    // Flax.Build.exe
        };

        /// <summary>
        /// Builds the assembly.
        /// </summary>
        public Assembly Build()
        {
            Dictionary<string, string> providerOptions = new Dictionary<string, string>();
            providerOptions.Add("CompilerVersion", "v4.0");
            CodeDomProvider provider = new Microsoft.CSharp.CSharpCodeProvider(providerOptions);

            HashSet<string> references = new HashSet<string>();
            foreach (var defaultReference in DefaultReferences)
                references.Add(defaultReference.Location);

            // Setup compilation options
            CompilerParameters cp = new CompilerParameters();
            cp.GenerateExecutable = false;
            cp.WarningLevel = 4;
            cp.TreatWarningsAsErrors = false;
            cp.ReferencedAssemblies.AddRange(references.ToArray());
            cp.GenerateInMemory = true;
            cp.IncludeDebugInformation = false;

            // HACK: C# will give compilation errors if a LIB variable contains non-existing directories
            Environment.SetEnvironmentVariable("LIB", null);

            CompilerResults cr = provider.CompileAssemblyFromFile(cp, SourceFiles.ToArray());
            // Process warnings and errors
            bool hasError = false;
            foreach (CompilerError ce in cr.Errors)
            {
                if (ce.IsWarning)
                {
                    Log.Warning(string.Format("{0} at {1}: {2}", ce.FileName, ce.Line, ce.ErrorText));
                }
                else
                {
                    Log.Error(string.Format("{0} at line {1}: {2}", ce.FileName, ce.Line, ce.ErrorText));
                    hasError = true;
                }
            }
            if (hasError)
                throw new Exception("Failed to build assembly.");

            return cr.CompiledAssembly;
        }
    }
}
