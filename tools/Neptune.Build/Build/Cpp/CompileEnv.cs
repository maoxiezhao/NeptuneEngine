using System;
using System.Collections.Generic;

namespace Neptune.Build
{
    public enum OptimizationHit
    {
        /// <summary>
        /// The default option.
        /// </summary>
        Neither,

        /// <summary>
        /// The fast code.
        /// </summary>
        FastCode,

        /// <summary>
        /// The small code.
        /// </summary>
        SmallCode
    }

    public enum CppVersion
    {
        /// <summary>
        /// C++17
        /// </summary>
        Cpp17,

        /// <summary>
        /// C++20
        /// </summary>
        Cpp20,

        /// <summary>
        /// The latest version supported by the compiler.
        /// </summary>
        Latest,
    }

    public class CompileEnv : ICloneable
    {
        public CppVersion CppVersion = CppVersion.Cpp17;
        public OptimizationHit OptimizationHit = OptimizationHit.Neither;
        public bool Optimization = false;

        /// <summary>
        /// The collection of defines with preprocessing symbol for a source files.
        /// </summary>
        public readonly List<string> PreprocessorDefinitions = new List<string>();

        /// <summary>
        /// The additional paths to add to the list of directories searched for include files.
        /// </summary>
        public readonly List<string> IncludePaths = new List<string>();

        public object Clone()
        {
            var clone = new CompileEnv
            {
                CppVersion = CppVersion,
                Optimization = Optimization,
            };
            clone.PreprocessorDefinitions.AddRange(PreprocessorDefinitions);
            clone.IncludePaths.AddRange(IncludePaths);
            return clone;
        }
    }
}
