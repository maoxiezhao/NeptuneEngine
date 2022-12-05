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
        /// Enables functions level linking support.
        /// </summary>
        public bool FunctionLevelLinking = false;

        /// <summary>
        /// Enables debug information generation.
        /// </summary>
        public bool DebugInformation = false;

        /// <summary>
        /// Enables runtime checks.
        /// </summary>
        public bool RuntimeChecks = false;

        /// <summary>
        /// Enables string pooling.
        /// </summary>
        public bool StringPooling = false;

        /// <summary>
        /// Enables the compiler intrinsic functions.
        /// </summary>
        public bool IntrinsicFunctions = false;

        /// <summary>
        /// Enables buffer security checks.
        /// </summary>
        public bool BufferSecurityCheck = true;

        /// <summary>
        /// Enables functions inlining support.
        /// </summary>
        public bool Inlining = false;

        /// <summary>
        /// Enables the whole program optimization.
        /// </summary>
        public bool WholeProgramOptimization = false;

        /// <summary>
        /// Hints to use Debug version of the standard library.
        /// </summary>
        public bool UseDebugCRT = false;

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
                UseDebugCRT = UseDebugCRT,
                FunctionLevelLinking = FunctionLevelLinking,
                DebugInformation = DebugInformation,
                RuntimeChecks = RuntimeChecks,
                StringPooling = StringPooling,
                IntrinsicFunctions = IntrinsicFunctions,
                BufferSecurityCheck = BufferSecurityCheck,
                Inlining = Inlining,
            };
            clone.PreprocessorDefinitions.AddRange(PreprocessorDefinitions);
            clone.IncludePaths.AddRange(IncludePaths);
            return clone;
        }
    }
}
