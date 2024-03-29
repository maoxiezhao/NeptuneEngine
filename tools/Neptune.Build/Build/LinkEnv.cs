﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public enum LinkerOutput
    {
        /// <summary>
        /// The executable file (aka .exe file).
        /// </summary>
        Executable,

        /// <summary>
        /// The shared library file (aka .dll file).
        /// </summary>
        SharedLibrary,

        /// <summary>
        /// The static library file (aka .lib file).
        /// </summary>
        StaticLibrary,

        /// <summary>
        /// The import library file (aka .lib file).
        /// </summary>
        ImportLibrary,
    }

    public class LinkEnv : ICloneable
    {
        public LinkerOutput Output;

        /// <summary>
        /// The collection of the object files to be linked.
        /// </summary>
        public readonly HashSet<string> InputFiles = new HashSet<string>();

        /// <summary>
        /// The collection of dependent static or import libraries paths.
        /// </summary>
        public readonly List<string> LibraryPaths = new List<string>();

        /// <summary>
        /// The collection of dependent static or import libraries that need to be linked.
        /// </summary>
        public readonly List<string> InputLibraries = new List<string>();

        /// <summary>
        /// Enables the code optimization.
        /// </summary>
        public bool Optimization = false;

        /// <summary>
        /// Hints to use incremental linking.
        /// </summary>
        public bool UseIncrementalLinking = false;

        /// <summary>
        /// Enables debug information generation.
        /// </summary>
        public bool DebugInformation = false;

        /// <summary>
        /// Enables the link time code generation (LTCG).
        /// </summary>
        public bool LinkTimeCodeGeneration = false;

        /// <summary>
        /// Use CONSOLE subsystem on Windows instead of the WINDOWS one.
        /// </summary>
        public bool LinkAsConsoleProgram = false;

        public object Clone()
        {
            var clone = new LinkEnv
            {
                Output = Output,
                Optimization = Optimization,
                DebugInformation = DebugInformation,
                LinkTimeCodeGeneration = LinkTimeCodeGeneration,
                UseIncrementalLinking = UseIncrementalLinking,
                LinkAsConsoleProgram = LinkAsConsoleProgram
            };

            foreach (var e in InputFiles)
                clone.InputFiles.Add(e);

            clone.InputLibraries.AddRange(InputLibraries);
            clone.LibraryPaths.AddRange(LibraryPaths);
            return clone;
        }
    }
}
