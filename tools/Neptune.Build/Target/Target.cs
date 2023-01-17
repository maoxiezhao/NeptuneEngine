using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;

namespace Neptune.Build
{
    public enum TargetLinkType
    {
        /// <summary>
        /// Link all modules (included into a build) into a single binary.
        /// </summary>
        Monolithic,

        /// <summary>
        /// Link all modules into individual dynamic libraries.
        /// </summary>
        Modular
    }

    public enum TargetOutputType
    {
        Executable,
        Library,
    }

    public class Target
    {
        /// <summary>
        /// The target name.
        /// </summary>
        public string Name;

        /// <summary>
        /// The target file path.
        /// </summary>
        public string FilePath;

        /// <summary>
        /// The project name.
        /// </summary>
        public string ProjectName;

        /// <summary>
        /// The target output binary name.
        /// </summary>
        public string OutputName;

        /// <summary>
        /// The path of the folder that contains this target file.
        /// </summary>
        public string FolderPath;

        /// <summary>
        /// The target link type.
        /// </summary>
        public TargetLinkType LinkType = TargetLinkType.Modular;

        /// <summary>
        /// Output type
        /// </summary>
        public TargetOutputType OutputType = TargetOutputType.Executable;

        /// <summary>
        /// Global macro definitions
        /// </summary>
        public List<string> GlobalDefinitions = new List<string>();

        /// <summary>
        /// The target platform architectures.
        /// </summary>
        public TargetArchitecture[] Architectures = { 
            TargetArchitecture.x64
        };

        public TargetConfiguration[] Configurations = {
            TargetConfiguration.Debug,
            TargetConfiguration.Development,
            TargetConfiguration.Release,
        };

        /// <summary>
        /// The target platforms.
        /// </summary>
        public TargetPlatform[] Platforms = {
            TargetPlatform.Windows,
        };

        /// <summary>
        /// Required modules
        /// </summary>
        public List<string> Modules = new List<string>();

        public bool IsPreBuilt = false;

        public Target()
        {
            var type = GetType();
            Name = type.Name;
            ProjectName = Name;
            OutputName = Name;
        }

        public virtual void Init()
        {
        }

        public virtual void PreBuild()
        {
        }

        public virtual void PostBuild()
        {
        }

        public virtual void SetupTargetEnvironment(BuildOptions options)
        {
            // Add preprocessor definitions
            options.CompileEnv.PreprocessorDefinitions.AddRange(GlobalDefinitions);

            // Record the output of link environment
            options.LinkEnv.Output = OutputType == TargetOutputType.Executable ? LinkerOutput.Executable : LinkerOutput.SharedLibrary;

            // Setup compile environment
            options.SetupCompileEnvironment();

            if (options.CompileEnv.UseDebugCRT)
                options.CompileEnv.PreprocessorDefinitions.Add("_DEBUG");
            else
                options.CompileEnv.PreprocessorDefinitions.Add("NDEBUG");
        }

        public virtual string GetOutputFilePath(BuildOptions options, TargetOutputType? outputType = null)
        {
            LinkerOutput linkerOutput;
            switch (outputType ?? OutputType)
            {
                case TargetOutputType.Executable:
                    linkerOutput = LinkerOutput.Executable;
                    break;
                case TargetOutputType.Library:
                    linkerOutput = LinkerOutput.SharedLibrary;
                    break;
                default: throw new ArgumentOutOfRangeException();
            }
            return Path.Combine(options.OutputFolder, options.Platform.GetLinkOutputFileName(OutputName, linkerOutput));
        }
    }
}